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

		logger::info("[🧥] Resurrecting {}", *actor);
		if (!actor->IsDead()) {
			logger::info("[🧥] \t⚠️ {} is not dead", *actor);
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
