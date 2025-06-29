#include "OutfitManager.h"
#include "DistributeManager.h"
#include "Helpers.h"

namespace Outfits
{
	void Manager::HandleMessage(SKSE::MessagingInterface::Message* message)
	{
		switch (message->type) {
		case SKSE::MessagingInterface::kPostLoad:
			{
				logger::info("🧥Outfit Manager");
				InitializeSerialization();
				InitializeHooks();
			}
			break;
#ifndef NDEBUG
		case SKSE::MessagingInterface::kPostPostLoad:
			LOG_HEADER("OUTFITS");  // This is where TESNPCs start initializing, so we give it a nice header.
			break;
#endif
		case SKSE::MessagingInterface::kPreLoadGame:
			isLoadingGame = true;
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
			isLoadingGame = false;
			break;
		default:
			break;
		}
	}

	bool Manager::HasDefaultOutfit(const RE::TESNPC* npc, const RE::BGSOutfit* outfit) const
	{
		if (!outfit) {
			return false;
		}

		if (auto existing = initialOutfits.find(npc->formID); existing != initialOutfits.end()) {
			return existing->second == outfit;
		}

		return npc->defaultOutfit == outfit;
	}

	bool Manager::CanEquipOutfit(const RE::Actor* actor, const RE::BGSOutfit* outfit) const
	{
		// Actors that don't have default outfit can't wear any outfit.
		if (!actor->GetActorBase()->defaultOutfit) {
			return false;
		}

		const auto race = actor->GetRace();
		for (const auto& item : outfit->outfitItems) {
			if (const auto armor = item->As<RE::TESObjectARMO>()) {
				if (!std::any_of(armor->armorAddons.begin(), armor->armorAddons.end(), [&](const auto& arma) {
						return arma && arma->IsValidRace(race);
					})) {
					return false;
				}
			}
		}

		return true;
	}
	
	void Manager::RestoreOutfit(RE::Actor* actor)
	{
		UpdateWornOutfit(actor, [&](OutfitReplacement& W) {
			if (W.distributed == actor->GetActorBase()->defaultOutfit) {
				wornReplacements.erase(actor->formID);
			} else {
				W.isDeathOutfit = false;
			}
		});
	}

	bool Manager::RevertOutfit(RE::Actor* actor)
	{
		if (!actor) {
			return false;
		}
		if (const auto replacement = PopWornOutfit(actor); replacement) {
			return RevertOutfit(actor, *replacement);
		}
		return false;
	}

	bool Manager::RevertOutfit(RE::Actor* actor, const OutfitReplacement& replacement) const
	{
		//#ifndef NDEBUG
		logger::info("\tReverting Outfit Replacement for {}", *actor);
		logger::info("\t\t{:R}", replacement);
		//#endif
		if (actor && actor->GetActorBase()) {
			if (auto outfit = actor->GetActorBase()->defaultOutfit; outfit) {
				actor->InitInventoryIfRequired();
				actor->RemoveOutfitItems(nullptr);
				if (!actor->IsDisabled()) {
					AddWornOutfit(actor, outfit, true);
				}
				return true;
			}
		}
		return false;
	}
	
	bool Manager::SetOutfit(const NPCData& data, RE::BGSOutfit* outfit, bool isDeathOutfit, bool isFinalOutfit)
	{
		const auto actor = data.GetActor();
		const auto npc = data.GetNPC();

		if (!actor || !npc) {  // invalid call
			return false;
		}

		const auto defaultOutfit = npc->defaultOutfit;

		if (!defaultOutfit) {
			return false;
		}

		// If outfit is nullptr, we just track that distribution didn't provide any outfit for this actor.
		if (outfit) {
			//#ifndef NDEBUG
			logger::info("Evaluating outfit for {}", *actor);
			logger::info("\tDefault Outfit: {}", *defaultOutfit);
			if (auto worn = wornReplacements.find(actor->formID); worn != wornReplacements.end()) {
				logger::info("\tWorn Outfit: {}", *worn->second.distributed);
			} else {
				logger::info("\tWorn Outfit: None");
			}
			logger::info("\tNew Outfit: {}", *outfit);
			//#endif
			if (!CanEquipOutfit(actor, outfit)) {
				//#ifndef NDEBUG
				logger::warn("\tAttempted to set Outfit {} that can't be worn by given actor.", *outfit);
				//#endif
				return false;
			}
		}

		if (auto replacement = ResolvePendingOutfit(data, outfit, isDeathOutfit, isFinalOutfit); replacement) {
			//#ifndef NDEBUG
			if (replacement->distributed) {
				logger::info("\tResolved Pending Outfit: {}", *replacement->distributed);
			}
			//#endif
		}

		return true;
	}

	bool Manager::ApplyOutfit(RE::Actor* actor, RE::BGSOutfit* outfit, bool shouldUpdate3D) const
	{
		if (!actor) {
			return false;
		}

		const auto npc = actor->GetActorBase();
		if (!npc) {
			return false;
		}

		// If default outfit is nullptr, then actor can't wear any outfit at all.
		if (!npc->defaultOutfit) {
			return false;
		}

		//#ifndef NDEBUG
		logger::info("[OUTFIT APPLY] BEFORE Outfit items present in {} inventory", *actor);
		LogWornOutfitItems(actor);
		//#endif

		if (IsSuspendedReplacement(actor)) {
			//#ifndef NDEBUG
			logger::info("[OUTFIT APPLY] Skipping outfit equip because distribution is suspended for {}", *actor);
			//#endif
			return false;
		}

		if (IsWearingOutfit(actor, outfit)) {
			//#ifndef NDEBUG
			logger::info("[OUTFIT APPLY] Outfit {} is already equipped on {}", *outfit, *actor);
			//#endif
			return true;
		}

		//#ifndef NDEBUG
		logger::info("[OUTFIT APPLY] Equipping Outfit {}", *outfit);
		//#endif
		actor->InitInventoryIfRequired();
		actor->RemoveOutfitItems(nullptr);
		if (!actor->IsDisabled()) {
			AddWornOutfit(actor, outfit, shouldUpdate3D);
		}
		//#ifndef NDEBUG
		logger::info("[OUTFIT APPLY] AFTER Outfit items present in {} inventory", *actor);
		LogWornOutfitItems(actor);
		//#endif
		return true;
	}

	RE::BGSOutfit* Manager::GetInitialOutfit(const RE::Actor* actor) const
	{
		if (const auto npc = actor->GetActorBase(); npc) {
			ReadLocker lock(_initialLock);
			if (const auto it = initialOutfits.find(npc->formID); it != initialOutfits.end()) {
				return it->second;
			}
		}
		return nullptr;
	}

	std::optional<Manager::OutfitReplacement> Manager::GetWornOutfit(const RE::Actor* actor) const
	{
		ReadLocker lock(_wornLock);
		if (const auto it = wornReplacements.find(actor->formID); it != wornReplacements.end()) {
			return it->second;
		}
		return std::nullopt;
	}

	bool Manager::UpdateWornOutfit(const RE::Actor* actor, std::function<void(OutfitReplacement&)> mutate)
	{
		WriteLocker lock(_wornLock);
		if (const auto it = wornReplacements.find(actor->formID); it != wornReplacements.end()) {
			auto& replacement = it->second;
			mutate(replacement);
			return true;
		}
		return false;
	}

	std::optional<Manager::OutfitReplacement> Manager::PopWornOutfit(const RE::Actor* actor)
	{
		WriteLocker lock(_wornLock);
		if (const auto it = wornReplacements.find(actor->formID); it != wornReplacements.end()) {
			auto replacement = it->second;
			wornReplacements.erase(it);
			return replacement;
		}
		return std::nullopt;
	}

	Manager::OutfitReplacementMap Manager::GetWornOutfits() const
	{
		ReadLocker lock(_wornLock);
		return wornReplacements;
	}

	std::optional<Manager::OutfitReplacement> Manager::GetPendingOutfit(const RE::Actor* actor) const
	{
		ReadLocker lock(_pendingLock);
		if (const auto it = pendingReplacements.find(actor->formID); it != pendingReplacements.end()) {
			return it->second;
		}
		return std::nullopt;
	}

	bool Manager::HasPendingOutfit(const RE::Actor* actor) const
	{
		ReadLocker lock(_pendingLock);
		return pendingReplacements.find(actor->formID) != pendingReplacements.end();
	}

	bool Manager::IsSuspendedReplacement(const RE::Actor* actor) const
	{
		if (const auto npc = actor->GetActorBase(); npc && npc->defaultOutfit) {
			if (const auto initialOutfit = GetInitialOutfit(actor)) {
				return initialOutfit != npc->defaultOutfit;
			}
		}

		return false;
	}

	RE::BSEventNotifyControl Manager::ProcessEvent(const RE::TESFormDeleteEvent* event, RE::BSTEventSource<RE::TESFormDeleteEvent>*)
	{
		if (event && event->formID != 0) {
			{
				WriteLocker lock(_wornLock);
				wornReplacements.erase(event->formID);
			}
			{
				WriteLocker lock(_pendingLock);
				pendingReplacements.erase(event->formID);
			}
			{
				WriteLocker lock(_initialLock);
				initialOutfits.erase(event->formID);
			}
		}
		return RE::BSEventNotifyControl::kContinue;
	}

	bool Manager::ProcessShouldBackgroundClone(RE::Character* actor, std::function<bool()> funcCall)
	{
		// For now, we only support a single distribution per game session.
		// As such if SPID_Processed is detected, we assume that the actor was already given an outfit if needed.
		// Previous setup relied on the fact that each ShouldBackgroundClone call would also refresh distribution of the outfit,
		// allowing OutfitManager to then swap it, thus perform rotation.
		if (!HasPendingOutfit(actor) && !actor->HasKeyword(Distribute::processed)) {
			SetOutfit(actor, nullptr, NPCData::IsDead(actor), false);
		}

		return funcCall();
	}
}
