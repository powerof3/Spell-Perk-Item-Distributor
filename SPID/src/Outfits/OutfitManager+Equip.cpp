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
								logger::info("[🧥][INVENTORY] {} took {} {} from {}", *toActor, count, *item, *fromActor);
							} else {
								logger::info("[🧥][INVENTORY] {} put {} {} to {}", *fromActor, count, *item, *to);
							}
						} else {
							logger::info("[🧥][INVENTORY] {} dropped {} {}", *fromActor, count, *item);
						}
					} else {  // from is inanimate container
						if (const auto to = RE::TESForm::LookupByID<RE::TESObjectREFR>(toID); to) {
							if (const auto toActor = to->As<RE::Actor>()) {
								logger::info("[🧥][INVENTORY] {} took {} {} from {}", *toActor, count, *item, *from);
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
							logger::info("[🧥][INVENTORY] {} picked up {} {}", *toActor, count, *item);
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

	
	bool Manager::ProcessResetReference(RE::Actor* actor, std::function<bool()> funcCall)
	{
		logger::info("[🧥][RECYCLE] Recycling {}", *actor);
		RevertOutfit(actor, false);
		// NEXT: After reseting, processed flag should be cleared, allowing new distribution to occur.
		return funcCall();
	}

	void Manager::ProcessResetInventory(RE::Actor* actor, std::function<void()> funcCall)
	{
		logger::info("[🧥][RESET] Resetting inventory of {}", *actor);
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
		funcCall();
	}

	/// Utility functions for ProcessUpdateWornGear.
	namespace utils
	{
		RE::InventoryEntryData* GetObjectInSlot(RE::Actor* actor, int slotIndex)
		{
			using func_t = decltype(&GetObjectInSlot);
			static REL::Relocation<func_t> func{ RELOCATION_ID(0, 419334) };
			return func(actor, slotIndex);
		}

		RE::BGSBipedObjectForm* GetBipedObject(RE::TESBoundObject* object)
		{
			if (const auto armo = object->As<RE::TESObjectARMO>(); armo) {
				return armo;
			} else if (const auto arma = object->As<RE::TESObjectARMA>(); arma) {
				return arma;
			}
			return nullptr;
		}

		inline bool HasOverlappingSlot(RE::Actor* actor, RE::TESBoundObject* obj)
		{
			if (const auto biped = GetBipedObject(obj); biped) {
				for (int slot = 0; slot < RE::BIPED_OBJECT::kEditorTotal; ++slot) {
					if (const auto equipped = GetObjectInSlot(actor, slot); equipped && equipped->object) {
						if (const auto equippedBiped = GetBipedObject(equipped->object); equippedBiped) {
							if (biped->bipedModelData.bipedObjectSlots & equippedBiped->bipedModelData.bipedObjectSlots) {
								return true;
							}
						}
					}
				}
			}
			return false;
		}
	}

	void Manager::ProcessUpdateWornGear(RE::Actor* actor, RE::BGSOutfit* outfit, bool forceUpdate, std::function<void()> funcCall)
	{
		auto effectiveOutfit = outfit;
		bool isDefault = true;
		if (!IsSuspendedReplacement(actor)) {
			if (const auto worn = GetWornOutfit(actor); worn && worn->distributed) {
				effectiveOutfit = worn->distributed;
				isDefault = false;
			}
		}

		if (actor && !actor->HasOutfitItems(effectiveOutfit) && (!actor->IsDead() || !RE::BGSSaveLoadGame::GetSingleton()->GetChange(actor, 32))) {
			if (auto changes = actor->GetInventoryChanges(); changes) {
				auto level = actor->GetLevel();
				logger::info("[🧥][UPDATE] Initializing worn outfit {} for {}", *effectiveOutfit, *actor);
				changes->InitOutfitItems(effectiveOutfit, level);
			}
		}

		// Logic related to horses onlyt appears in AE version of the game.
		// SE only checks the outfit as written above.
		#ifdef SKYRIM_SUPPORT_AE
		if (actor && actor->IsHorse()) {
			for (const auto& item : effectiveOutfit->outfitItems) {
				if (const auto obj = item->As<RE::TESBoundObject>(); obj) {
					if (utils::HasOverlappingSlot(actor, obj)) {
						logger::info("[🧥][UPDATE] {} has equipped something in slot for {}", *actor, *obj);
						return;
					}
				}
			}
		}
		#endif

		logger::info("[🧥][UPDATE] Equipping {} outfit {} to {}", isDefault ? "default" : "distributed", *effectiveOutfit, *actor);
		AddWornOutfit(actor, effectiveOutfit, forceUpdate, false);
	}
}
