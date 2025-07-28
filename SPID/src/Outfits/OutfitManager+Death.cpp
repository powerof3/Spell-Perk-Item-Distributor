#include "OutfitManager.h"

namespace Outfits
{
	RE::BSEventNotifyControl Manager::ProcessEvent(const RE::TESDeathEvent* event, RE::BSTEventSource<RE::TESDeathEvent>*)
	{
		if (!event || event->dead) {
			return RE::BSEventNotifyControl::kContinue;
		}

		if (const auto actor = event->actorDying->As<RE::Actor>(); actor && !actor->IsPlayerRef()) {
			// If there is no pending outfit after death, that means death distriubtion didn't provide anything, so we finalize current outfit by marking it as dead.
			if (!HasPendingOutfit(actor)) {
				auto data = NPCData(actor, true);
				SetOutfit(data, nullptr, true, false);
			}

			if (const auto outfit = ResolveWornOutfit(actor, true); outfit) {
				ApplyOutfit(actor, outfit->distributed);
			}
		}

		return RE::BSEventNotifyControl::kContinue;
	}

	void Manager::ProcessResurrect(RE::Actor* actor, bool resetInventory, std::function<void()> funcCall)
	{
		if (!actor) {
			funcCall();
			return;
		}

		logger::info("[🧥][RESURRECT] Resurrecting {}", *actor);
		if (!actor->IsDead()) {
			logger::info("[🧥][RESURRECT] \t⚠️ {} is not dead", *actor);
		}
		if (resetInventory) {
			logger::info("[🧥][RESURRECT] \tInventory will be reset.");
		}

		RestoreOutfit(actor);
		funcCall();
		if (!resetInventory) {
			if (const auto wornOutfit = GetWornOutfit(actor); wornOutfit) {
				ApplyOutfit(actor, wornOutfit->distributed);
			} else {
				actor->InitInventoryIfRequired();
				actor->RemoveOutfitItems(nullptr);
				actor->AddWornOutfit(actor->GetActorBase()->defaultOutfit, true);
			}
		}
	}

	bool Manager::ProcessResetReference(RE::Actor* actor, std::function<bool()> funcCall)
	{
		logger::info("[🧥][RECYCLE] Recycling {}", *actor);
		RevertOutfit(actor, false);
		// NEXT: After reseting, processed flag should be cleared, allowing new distribution to occur.
		return funcCall();
	}

	// FIX: When resetting inventory of a dead NPC, the game adds defaultOutfit items instead of distributed outfit.
	void Manager::ProcessResetInventory(RE::Actor* actor, std::function<void()> funcCall)
	{
		logger::info("[🧥][RESET] Resetting inventory of {}", *actor);
		funcCall();
	}
}
