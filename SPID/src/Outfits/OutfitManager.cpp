#include "OutfitManager.h"
#include "Distribute.h"
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
		if (actor && actor->GetActorBase()) {
			logger::info("[🧥][REVERT]\tReverting Outfit Replacement for {}", *actor);
		if (auto outfit = actor->GetActorBase()->defaultOutfit; outfit) {
			logger::info("[🧥][REVERT]\t\t{:R} ({})", replacement, *outfit);
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
			logger::info("[🧥][SET] Evaluating outfit for {}", *actor);
			logger::info("[🧥][SET] \tDefault Outfit: {}", *defaultOutfit);
			if (auto worn = wornReplacements.find(actor->formID); worn != wornReplacements.end()) {
				logger::info("[🧥][SET] \tWorn Outfit: {}", *worn->second.distributed);
			} else {
				logger::info("[🧥][SET] \tWorn Outfit: None");
			}
			logger::info("[🧥][SET] \tNew Outfit: {}", *outfit);
			//#endif
			if (!CanEquipOutfit(actor, outfit)) {
				//#ifndef NDEBUG
				logger::warn("[🧥][SET] \tAttempted to set Outfit {} that can't be worn by given actor.", *outfit);
				//#endif
				return false;
			}
		}

		if (auto replacement = ResolvePendingOutfit(data, outfit, isDeathOutfit, isFinalOutfit); replacement) {
			//#ifndef NDEBUG
			if (replacement->distributed) {
				logger::info("[🧥][SET] \tResolved Pending Outfit: {}", *replacement->distributed);
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

		// Empty outfit might be used to undress the actor.
		if (outfit->outfitItems.empty()) {
			logger::info("[🧥][APPLY] \t⚠️ Outfit {} is empty - {} will appear naked.", *outfit, *actor);
		}

		//#ifndef NDEBUG
		logger::info("[🧥][APPLY] BEFORE Outfit items present in {} inventory", *actor);
		LogWornOutfitItems(actor, "[🧥][APPLY] \t\t");
		//#endif

		if (IsSuspendedReplacement(actor)) {
			//#ifndef NDEBUG
			logger::info("[🧥][APPLY] Skipping outfit equip because distribution is suspended for {}", *actor);
			//#endif
			return false;
		}

		if (IsWearingOutfit(actor, outfit)) {
			//#ifndef NDEBUG
			logger::info("[🧥][APPLY] Outfit {} is already equipped on {}", *outfit, *actor);
			//#endif
			return true;
		}

		//#ifndef NDEBUG
		logger::info("[🧥][APPLY] Equipping Outfit {}", *outfit);
		//#endif
		actor->InitInventoryIfRequired();
		actor->RemoveOutfitItems(nullptr);
		if (!actor->IsDisabled()) {
			AddWornOutfit(actor, outfit, shouldUpdate3D);
		}
		//#ifndef NDEBUG
		logger::info("[🧥][APPLY] AFTER Outfit items present in {} inventory", *actor);
		LogWornOutfitItems(actor, "[🧥][APPLY] \t\t");
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

	bool Manager::HasWornOutfit(const RE::Actor* actor) const
	{
		ReadLocker lock(_wornLock);
		return wornReplacements.find(actor->formID) != wornReplacements.end();
	}

	bool Manager::IsProcessed(const RE::Actor* actor) const
	{
		ReadLocker lock(_processedLock);
		return processedActors.contains(actor->formID);
	}

	// NEXT: Subscribe to serialization interface for Revert.
	// Reset processed flag for all reverted NPCs, so that reloading the same save will always re-process NPCs.
	// Right now reloading the save will result in using outfit baked into the save (e.g. from previous game session).
	// OR - check the loading logic, and don't overwrite wornReplacements map if anything is already there
	// (however, this has some other cases when currently not loaded NPCs would become out of sync.
	//
	// Maybe... make outfit distribution permanent. So once something is picked for NPC, they will stick with it in this save game.
	void Manager::MarkProcessed(const RE::Actor* actor)
	{
		WriteLocker lock(_processedLock);
		processedActors.insert(actor->formID);
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
		if (!IsProcessed(actor)) {
			auto npcData = NPCData(actor);
			Distribute::DistributeOutfits(npcData, false);

			if (!HasPendingOutfit(actor)) {
				SetOutfit(actor, nullptr, NPCData::IsDead(actor), false);
			}
			MarkProcessed(actor);
		}

		return funcCall();
	}

	void Manager::ProcessFilterInventoryItems(RE::NiPointer<RE::TESObjectREFR>& ptr, std::function<void()> funcCall)
	{
		if (auto refr = ptr.get(); refr) {
			if (auto actor = refr->As<RE::Actor>(); actor) {
				if (auto npc = actor->GetActorBase(); npc) {
					if (npc->defaultOutfit) {
						if (auto worn = GetWornOutfit(actor); worn && worn->distributed) {
							auto old = npc->defaultOutfit;
							npc->defaultOutfit = worn->distributed;
							funcCall();
							npc->defaultOutfit = old;
							return;
						}
					}
				}
			}
		}
		funcCall();
	}
}
