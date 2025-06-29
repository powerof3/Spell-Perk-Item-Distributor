#include "Helpers.h"
#include "OutfitManager.h"

namespace Outfits
{
	RE::BSEventNotifyControl Manager::ProcessEvent(const RE::TESContainerChangedEvent* event, RE::BSTEventSource<RE::TESContainerChangedEvent>*)
	{
		if (event) {
			auto fromID = event->oldContainer;
			auto toID = event->newContainer;
			auto itemID = event->baseObj;
			auto count = event->itemCount;

			if (const auto item = RE::TESForm::LookupByID<RE::TESBoundObject>(itemID)) {
				if (const auto from = RE::TESForm::LookupByID<RE::TESObjectREFR>(fromID); from) {
					if (const auto fromActor = from->As<RE::Actor>()) {
						if (const auto to = RE::TESForm::LookupByID<RE::TESObjectREFR>(toID); to) {
							if (const auto toActor = to->As<RE::Actor>()) {
								logger::info("[INVENTORY] {} took {} {} from {}", *toActor, count, *item, *fromActor);
							} else {
								logger::info("[INVENTORY] {} put {} {} to {}", *fromActor, count, *item, *to);
							}
						} else {
							logger::info("[INVENTORY] {} dropped {} {}", *fromActor, count, *item);
						}
					} else {  // from is inanimate container
						if (const auto to = RE::TESForm::LookupByID<RE::TESObjectREFR>(toID); to) {
							if (const auto toActor = to->As<RE::Actor>()) {
								logger::info("[INVENTORY] {} took {} {} from {}", *toActor, count, *item, *from);
							} else {
								//logger::info("[INVENTORY] {} {} transfered from {} to {}", count, *item, *from, *to);
							}
						} else {
							//logger::info("[INVENTORY] {} {} removed from {}", count, *item, *from);
						}
					}
				} else {  // From is none
					if (const auto to = RE::TESForm::LookupByID<RE::TESObjectREFR>(toID); to) {
						if (const auto toActor = to->As<RE::Actor>()) {
							logger::info("[INVENTORY] {} picked up {} {}", *toActor, count, *item);
						} else {
							//logger::info("[INVENTORY] {} {} transfered to {}", count, *item, *to);
						}
					} else {
						//logger::info("[INVENTORY] {} {} materialized out of nowhere and vanished without a trace.", count, *item);
					}
				}
			}
		}

		return RE::BSEventNotifyControl::kContinue;
	}

	RE::NiAVObject* Manager::ProcessLoad3D(RE::Actor* actor, std::function<RE::NiAVObject*()> funcCall)
	{
		if (!isLoadingGame) {
			if (const auto outfit = ResolveWornOutfit(actor, false); outfit) {
				ApplyOutfit(actor, outfit->distributed);
			}
		}

		return funcCall();
	}

	void Manager::ProcessResetInventory(RE::Actor* actor, bool reapplyOutfitNow, std::function<void()> funcCall)
	{
		if (reapplyOutfitNow) {
			logger::info("[OUTFIT RESET] Restoring worn outfit after inventory was reset for {}", *actor);

			if (!IsSuspendedReplacement(actor)) {
				if (const auto outfit = GetWornOutfit(actor); outfit) {
					ApplyOutfit(actor, outfit->distributed);
					return;
				}
			}
		} else if (actor && !actor->Get3D2()) {
			if (UpdateWornOutfit(actor, [&](OutfitReplacement& replacement) { replacement.needsInitialization = true; })) {
				logger::info("[OUTFIT RESET] Queuing restoring worn outfit after inventory was reset for {}", *actor);
			}
		}

		funcCall();
	}

	void Manager::ProcessResurrect(RE::Actor* actor, bool resetInventory, std::function<void()> funcCall)
	{
		RestoreOutfit(actor);
		funcCall();
		// If resurrection will reset inventory, then ResetInventory hook will be responsible for re-applying outfit,
		// otherwise we need to manually re-apply outfit.
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
		RevertOutfit(actor, false);
		return funcCall();
	}

	void Manager::ProcessInitializeDefaultOutfit(RE::TESNPC* npc, RE::Actor* actor, std::function<void()> funcCall)
	{
		if (!npc || !actor || !npc->defaultOutfit || actor->IsPlayerRef()) {
			return funcCall();
		}

		// TODO: There is a case when NPC might appear naked, as the game removed outfit items. In this case we need to restore the outfit.
		// However, we can't simply check if outfit items are present in NPC's inventory, as they might be missing due to the fact that player (or someone else) took those items.
		if (const auto worn = GetWornOutfit(actor); worn && worn->distributed) {
			if (!IsSuspendedReplacement(actor)) {
				if (worn->needsInitialization) {
					UpdateWornOutfit(actor, [&](OutfitReplacement& replacement) {
						replacement.needsInitialization = false;
					});
					logger::info("[OUTFIT INIT] Resotring outfit for {} after reset", *actor);
					ApplyOutfit(actor, worn->distributed, true);
				} else {
					logger::info("[OUTFIT INIT] Default outfit init ignored for {} as it is SPID managed", *actor);
					logger::info("[OUTFIT INIT] Outfit items present in {} inventory", *actor);
					LogWornOutfitItems(actor);
				}
				return;
			}
		}

		logger::info("[OUTFIT INIT] BEFORE Outfit items present in {} inventory", *actor);
		LogWornOutfitItems(actor);

		logger::info("[OUTFIT INIT] Initializing default outfit for {}?", *actor);
		funcCall();

		logger::info("[OUTFIT INIT] AFTER Outfit items present in {} inventory", *actor);
		LogWornOutfitItems(actor);
	}
}
