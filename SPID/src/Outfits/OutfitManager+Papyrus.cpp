#include "OutfitManager.h"

namespace Outfits
{
	void Manager::ProcessInitItemImpl(RE::TESNPC* npc, std::function<void()> funcCall)
	{
		funcCall();

		if (npc->defaultOutfit) {
			//#ifndef NDEBUG
			//			logger::info("{}: {}", *npc, *npc->defaultOutfit);
			//#endif
			WriteLocker lock(_initialLock);
			initialOutfits.try_emplace(npc->formID, npc->defaultOutfit);
		}
	}

	void Manager::ProcessSetOutfitActor(RE::Actor* actor, RE::BGSOutfit* outfit, std::function<void()> funcCall)
	{
		logger::info("[🧥][PAPYRUS] SetOutfit({}) was called for actor {}.", *outfit, *actor);

		// Empty outfit might be used to undress the actor.
		if (outfit->outfitItems.empty()) {
			logger::info("[🧥][PAPYRUS] \t⚠️ Outfit {} is empty - Actor will appear naked unless followed by another call to SetOutfit.", *outfit);
		}

		// If there is no distributed outfit there is nothing to suspend/resume.
		if (const auto wornOutfit = GetWornOutfit(actor); wornOutfit) {
			const auto initialOutfit = GetInitialOutfit(actor);
			if (initialOutfit) {
				if (initialOutfit == outfit) {
					if (IsSuspendedReplacement(actor)) {
						logger::info("[🧥][PAPYRUS] \t▶️ Resuming outfit distribution for {} as defaultOutfit has been reverted to its initial state", *actor);
						if (actor->GetActorBase()->defaultOutfit != outfit) {
							actor->GetActorBase()->SetDefaultOutfit(outfit);
						}
						ApplyOutfit(actor, wornOutfit->distributed, true);  // apply our distributed outfit instead of defaultOutfit
						return;
					}
				} else {
					actor->RemoveOutfitItems(wornOutfit->distributed);  // remove distributed outfit, so that it won't be stuck in the inventory
					logger::info("[🧥][PAPYRUS] \t⏸️ Suspending outfit distribution for {} due to manual change of the outfit", *actor);
					logger::info("[🧥][PAPYRUS] \t\tTo resume distribution SetOutfit({}) should be called for this actor", *initialOutfit);
				}
			}
		}

		funcCall();
	}
}
