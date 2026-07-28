#include "OutfitManager.h"

namespace Outfits
{
	RE::BSEventNotifyControl Manager::ProcessEvent(const RE::TESDeathEvent* event, RE::BSTEventSource<RE::TESDeathEvent>*)
	{
		if (!event || event->dead) {
			return RE::BSEventNotifyControl::kContinue;
		}

		if (const auto actor = event->actorDying->As<RE::Actor>(); actor && !actor->IsPlayerRef()) {
			if (actor->IsSummoned()) {
				// The game can recycle a summoned actor's FormID for an unrelated future summon (same or different NPC) without ever going through ResetReference or TESFormDeleteEvent.
				// Luckily for us, summoned actor is killed when it is dispelled, so we use death event to remove summoned actor from our tracking.
				UntrackActor(actor->formID);
				return RE::BSEventNotifyControl::kContinue;
			}

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

		logger::info("[🧥] Resurrecting {}", *actor);
		if (!actor->IsDead()) {
			logger::warn("[🧥] \t⚠️ {} is not dead", *actor);
		}
		if (resetInventory) {
			logger::info("[🧥] \tInventory will be reset.");
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
}
