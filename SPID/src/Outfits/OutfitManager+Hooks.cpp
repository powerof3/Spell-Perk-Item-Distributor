#include "Hooking.h"
#include "OutfitManager.h"

namespace Outfits
{
	/// This hook performs distribution of outfits.
	struct ShouldBackgroundClone
	{
		using Target = RE::Character;
		static inline constexpr std::size_t index{ 0x6D };

		static bool thunk(RE::Character* actor)
		{
#ifndef NDEBUG
			//	logger::info("Outfits: ShouldBackgroundClone({})", *(actor->As<RE::Actor>()));
#endif
			return Manager::GetSingleton()->ProcessShouldBackgroundClone(actor, [&] { return func(actor); });
		}

		static inline void post_hook()
		{
			logger::info("\t\t🪝Installed ShouldBackgroundClone hook.");
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	/// This hook applies pending outfit replacements before loading 3D model. Outfit Replacements are created by SetDefaultOutfit.
	struct Load3D
	{
		using Target = RE::Character;
		static inline constexpr std::size_t index{ 0x6A };

		static RE::NiAVObject* thunk(RE::Character* actor, bool a_backgroundLoading)
		{
			//logger::info("Load3D ({}); Background: {}", *(actor->As<RE::Actor>()), a_backgroundLoading);
			return Manager::GetSingleton()->ProcessLoad3D(actor, [&] { return func(actor, a_backgroundLoading); });
		}

		static inline void post_hook()
		{
			logger::info("\t\t🪝Installed Load3D hook.");
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	/// This hook builds a map of initialOutfits for all NPCs that can have it.
	/// The map is later used to determine when manual calls to SetOutfit should suspend/resume SPID-managed outfits.
	struct InitItemImpl
	{
		using Target = RE::TESNPC;
		static inline constexpr std::size_t index{ 0x13 };

		static void thunk(RE::TESNPC* npc)
		{
			Manager::GetSingleton()->ProcessInitItemImpl(npc, [&] { func(npc); });
		}

		static inline void post_hook()
		{
			logger::info("\t\t🪝Installed InitItemImpl hook.");
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	/// This hook is used to track when NPCs come back to life to clear the isDeathOutfit flag and restore general outfit behavior,
	/// as alive NPCs are allowed to change their outfits (even if looted).
	///
	/// Note: recycleactor console command also triggers resurrect when needed.
	/// Note: NPCs who Start Dead cannot be resurrected.
	struct Resurrect
	{
		using Target = RE::Character;
		static inline constexpr std::size_t index{ 0xAB };

		static void thunk(RE::Character* actor, bool resetInventory, bool attach3D)
		{
			Manager::GetSingleton()->ProcessResurrect(actor, resetInventory, [&] { func(actor, resetInventory, attach3D); });
		}

		static inline void post_hook()
		{
			logger::info("\t\t🪝Installed Resurrect hook.");
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	/// This hook ensures that NPCs receive distributed outfit when inventory is reset.
	struct ResetInventory
	{
		static inline constexpr REL::ID     relocation = RELOCATION_ID(36332, 37322);
		static inline constexpr std::size_t offset = OFFSET(0x56, 0x56);

		static void thunk(RE::Actor* actor, bool leveledOnly)
		{
			Manager::GetSingleton()->ProcessResetInventory(actor, [&] { func(actor, leveledOnly); });
		}

		static inline void post_hook()
		{
			logger::info("\t\t🪝Installed ResetInventory hook.");
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	/// This hook is used to clear outfit replacements info since the actor will be completely reset,
	/// thus eligible for a fresh distribution.
	struct ResetReference
	{
		static inline constexpr REL::ID     relocation = RELOCATION_ID(21556, 22038);
		static inline constexpr std::size_t offset = OFFSET(0x4B, 0x4B);

		static bool thunk(std::int64_t a1, std::int64_t a2, std::int64_t a3, RE::TESObjectREFR* refr, std::int64_t a5, std::int64_t a6, int a7, int* a8)
		{
			if (refr) {
				if (const auto actor = refr->As<RE::Actor>(); actor) {
					return Manager::GetSingleton()->ProcessResetReference(actor, [&] { return func(a1, a2, a3, refr, a5, a6, a7, a8); });
				}
			}
			return func(a1, a2, a3, refr, a5, a6, a7, a8);
		}

		static inline void post_hook()
		{
			logger::info("\t\t🪝Installed ResetReference hook.");
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	/// This hook is used to detect manual changes of Actor's outfit and suspend SPID-managed outfits.
	/// Suspending allows SPID-managed outfits to behave like default outfits.
	struct SetOutfitActor
	{
		static inline constexpr REL::ID     relocation = RELOCATION_ID(53960, 54784);
		static inline constexpr std::size_t offset = OFFSET(0x3E60, 0x47D5);

		// This re-creates original papyrus::Actor::SetOutfit function.
		static void thunk(RE::BSScript::Internal::VirtualMachine* vm, RE::VMStackID stackID, RE::Actor* actor, RE::BGSOutfit* outfit, bool isSleepOutfit)
		{
			if (!outfit) {
				using PapyrusLog = void(RE::TESObjectREFR*, const char*, RE::BSScript::Internal::VirtualMachine*, RE::VMStackID, RE::BSScript::ErrorLogger::Severity);
				REL::Relocation<PapyrusLog> writeLog{ RELOCATION_ID(53024, 53824) };
				writeLog(actor, "Cannot set sleep or default outfit to None", vm, stackID, RE::BSScript::ErrorLogger::Severity::kError);
				return;
			}

			auto npc = actor->GetActorBase();

			// Sleep outfits retain original logic... for now :)
			if (isSleepOutfit) {
				if (npc->sleepOutfit == outfit) {
					return;
				}
				actor->RemoveOutfitItems(npc->sleepOutfit);
				npc->SetSleepOutfit(outfit);

				actor->InitInventoryIfRequired();
				if (!actor->IsDisabled()) {
					actor->AddWornOutfit(outfit, true);
				}
			} else {
				Manager::GetSingleton()->ProcessSetOutfitActor(actor, outfit, [&] { func(vm, stackID, actor, outfit, isSleepOutfit); });
			}
		}

		static inline void post_hook()
		{
			logger::info("\t\t🪝Installed SetOutfit hook.");
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	// NEXT: These will be disabled for now, but in the future we might want to use these hooks to handle equip/unequip as events and triggers for some actions.
	// For future reference, there is a situation when the game repeatedly calls (spams) Equip/Unequip calls
	// during a scripted scene where NPC practices archery (an arrow is being equipped/unequipped), causing some lagging.

	//struct EquipObject
	//{
	//	static inline constexpr REL::ID     relocation = RELOCATION_ID(37938, 38894);
	//	static inline constexpr std::size_t offset = OFFSET(0xE5, 0x170);

	//	static void thunk(RE::ActorEquipManager* manager, RE::Actor* actor, RE::TESBoundObject* object, RE::ObjectEquipParams* params)
	//	{
	//		if (actor && object) {
	//			logger::info("[EQUIP] {} equips {}", *actor, *object);
	//		}
	//		func(manager, actor, object, params);
	//	}

	//	static inline void post_hook()
	//	{
	//		logger::info("\t\t🪝Installed EquipObject hook.");
	//	}

	//	static inline REL::Relocation<decltype(thunk)> func;
	//};

	//struct UnequipObject
	//{
	//	static inline constexpr REL::ID     relocation = RELOCATION_ID(37945, 38901);
	//	static inline constexpr std::size_t offset = OFFSET(0x138, 0x1B9);

	//	static void thunk(RE::ActorEquipManager* manager, RE::Actor* actor, RE::TESBoundObject* object, RE::ObjectEquipParams* params)
	//	{
	//		if (actor && object) {
	//			logger::info("[UNEQUIP] {} unequips {}", *actor, *object);
	//		}
	//		func(manager, actor, object, params);
	//	}

	//	static inline void post_hook()
	//	{
	//		logger::info("\t\t🪝Installed UnequipObject hook.");
	//	}

	//	static inline REL::Relocation<decltype(thunk)> func;
	//};

	/// This hook ensures that items from distributed outfit are not accessible in the inventory.
	struct FilterInventoryItems
	{
		static inline constexpr REL::ID     relocation = RELOCATION_ID(50216, 51145);
		static inline constexpr std::size_t offset = OFFSET(0xED, 0xBE);

		static void thunk(RE::ItemList* itemList, RE::InventoryChanges* invChanges, RE::NiPointer<RE::TESObjectREFR>& container)
		{
			return Manager::GetSingleton()->ProcessFilterInventoryItems(container, [&] { return func(itemList, invChanges, container); });
		}

		static inline void post_hook()
		{
			logger::info("\t\t🪝Installed FilterInventoryItems hook.");
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	/// This hook ensures that items from distributed outfit are not accessible in the inventory.
	struct FilterInventoryItems2
	{
		static inline constexpr REL::ID     relocation = RELOCATION_ID(50217, 51146);
		static inline constexpr std::size_t offset = OFFSET(0x152, 0x12E);

		static void thunk(RE::ItemList* itemList, RE::InventoryChanges* invChanges, RE::InventoryEntryData* item, RE::NiPointer<RE::TESObjectREFR>& container)
		{
			return Manager::GetSingleton()->ProcessFilterInventoryItems(container, [&] { return func(itemList, invChanges, item, container); });
		}

		static inline void post_hook()
		{
			logger::info("\t\t🪝Installed FilterInventoryItems2 hook.");
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

#ifdef SKYRIM_SUPPORT_AE
	/// This hook ensures that items from distributed outfit are not accessible in the inventory.
	struct FilterInventoryItemsAE1170
	{
		static inline constexpr REL::ID     relocation = REL::ID(51145);
		static inline constexpr std::size_t offset = 0xEB;

		static void thunk(RE::ItemList* itemList, RE::InventoryChanges* invChanges, RE::NiPointer<RE::TESObjectREFR>& container)
		{
			return Manager::GetSingleton()->ProcessFilterInventoryItems(container, [&] { return func(itemList, invChanges, container); });
		}

		static inline void post_hook()
		{
			logger::info("\t\t🪝Installed FilterInventoryItems hook.");
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	/// This hook ensures that items from distributed outfit are not accessible in the inventory.
	struct FilterInventoryItems2AE1170
	{
		static inline constexpr REL::ID     relocation = REL::ID(51146);
		static inline constexpr std::size_t offset = 0x15B;

		static void thunk(RE::ItemList* itemList, RE::InventoryChanges* invChanges, RE::InventoryEntryData* item, RE::NiPointer<RE::TESObjectREFR>& container)
		{
			return Manager::GetSingleton()->ProcessFilterInventoryItems(container, [&] { return func(itemList, invChanges, item, container); });
		}

		static inline void post_hook()
		{
			logger::info("\t\t🪝Installed FilterInventoryItems2 hook.");
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	/// This hook ensures that items from distributed outfit are not accessible in the inventory.
	struct FilterInventoryItems3AE
	{
		// In AE one of the functions inlines call to function used in the above hook (FilterInventoryItems), so we need to add extra hook for it.
		static inline constexpr REL::ID     relocation = REL::ID(51144);
		static inline constexpr std::size_t offset = 0xE2;

		static void thunk(RE::ItemList* itemList, RE::InventoryChanges* invChanges, RE::NiPointer<RE::TESObjectREFR>& container)
		{
			return Manager::GetSingleton()->ProcessFilterInventoryItems(container, [&] { return func(itemList, invChanges, container); });
		}

		static inline void post_hook()
		{
			logger::info("\t\t🪝Installed FilterInventoryItemsAE hook.");
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	/// This hook ensures that items from distributed outfit are not accessible in the inventory.
	struct FilterInventoryItems3AE1170
	{
		// In AE one of the functions inlines call to function used in the above hook (FilterInventoryItems), so we need to add extra hook for it.
		static inline constexpr REL::ID     relocation = REL::ID(51144);
		static inline constexpr std::size_t offset = 0x10F;

		static void thunk(RE::ItemList* itemList, RE::InventoryChanges* invChanges, RE::NiPointer<RE::TESObjectREFR>& container)
		{
			return Manager::GetSingleton()->ProcessFilterInventoryItems(container, [&] { return func(itemList, invChanges, container); });
		}

		static inline void post_hook()
		{
			logger::info("\t\t🪝Installed FilterInventoryItemsAE hook.");
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};
#endif

	///  This hook stubs call to HasOutfitItems, so that the UpdateWornGear function would always call AddWornOutfit where we handle outfit.
	/// Our hook will re-iomplement the entire logic related to outfit.
	struct UpdateWornGear_HasOutfitItems_stub
	{
		static inline constexpr REL::ID     relocation = RELOCATION_ID(24234, 418622);
		static inline constexpr std::size_t offset = OFFSET(0x158, 0x15B);

		static bool thunk(RE::TESObjectREFR* refr, RE::BGSOutfit* outfit)
		{
			return true;
		}

		static inline void post_hook()
		{
			logger::info("\t\t🪝Installed HasOutfitItems stub.");
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

#ifdef SKYRIM_SUPPORT_AE
	/// This hook stubs call to IsHorse, so that the UpdateWornGear function would always call AddWornOutfit where we handle outfit.
	/// Our hook will re-iomplement the entire logic related to outfit.
	struct UpdateWornGear_IsHorse_stub
	{
		// This logic related to horse only appears in AE version of the game.
		static inline constexpr REL::ID     relocation = REL::ID(418622);
		static inline constexpr std::size_t offset = 0x1C0;

		static bool thunk(RE::TESObjectREFR* refr)
		{
			return false;
		}

		static inline void post_hook()
		{
			logger::info("\t\t🪝Installed IsHorse stub.");
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};
#endif

	/// This hook implements part of the UpdateWornGear related to the default outfit.
	struct UpdateWornGear_AddWornOutfit
	{
		static inline constexpr REL::ID     relocation = RELOCATION_ID(24234, 418622);
		static inline constexpr std::size_t offset = OFFSET(0x1C3, 0x25E);

		static void thunk(RE::Actor* actor, RE::BGSOutfit* outfit, bool forceUpdate)
		{
			Manager::GetSingleton()->ProcessUpdateWornGear(actor, outfit, forceUpdate, [&]() { return func(actor, outfit, forceUpdate); });
		}

		static inline void post_hook()
		{
			logger::info("\t\t🪝Installed AddWornOutfit hook.");
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	void Manager::InitializeHooks()
	{
		if (const auto scripts = RE::ScriptEventSourceHolder::GetSingleton()) {
			scripts->AddEventSink<RE::TESFormDeleteEvent>(this);
			logger::info("\t\t📝Registered for {}.", typeid(RE::TESFormDeleteEvent).name());
			scripts->AddEventSink<RE::TESDeathEvent>(this);
			logger::info("\t\t📝Registered for {}.", typeid(RE::TESDeathEvent).name());
			//#ifndef NDEBUG
			scripts->AddEventSink<RE::TESContainerChangedEvent>(this);
			logger::info("\t\t📝Registered for {}.", typeid(RE::TESContainerChangedEvent).name());
			//#endif
		}

		// Cache initial outfits for all NPCs.
		stl::install_hook<InitItemImpl>();

		// Main outfit distribution hook.
		stl::install_hook<ShouldBackgroundClone>();

		// Initial equipping of distributed outfits.
		stl::install_hook<Load3D>();

		// A bunch of cases when NPC/actor outfit needs to be reset or re-initialized.
		stl::install_hook<Resurrect>();
		stl::install_hook<ResetReference>();
		stl::install_hook<ResetInventory>();

		// Papyrus handling (suspending/resuming SPID outfits).
		stl::install_hook<SetOutfitActor>();

		// Hide distributed outfit items from the inventory.
#ifdef SKYRIM_SUPPORT_AE
		auto gameVersion = REL::Module::get().version();
		if (gameVersion >= SKSE::RUNTIME_SSE_1_6_1170) {
			stl::install_hook<FilterInventoryItemsAE1170>();
			stl::install_hook<FilterInventoryItems2AE1170>();
			stl::install_hook<FilterInventoryItems3AE1170>();
		} else if (gameVersion >= SKSE::RUNTIME_SSE_1_6_640) {
			stl::install_hook<FilterInventoryItems3AE>();
		} else {
#else
		{
#endif
			stl::install_hook<FilterInventoryItems>();
			stl::install_hook<FilterInventoryItems2>();
		}
		// Track attempts to keep best gear equipped.
		stl::install_hook<UpdateWornGear_HasOutfitItems_stub>();
#ifdef SKYRIM_SUPPORT_AE
		stl::install_hook<UpdateWornGear_IsHorse_stub>();
#endif
		stl::install_hook<UpdateWornGear_AddWornOutfit>();
	}
}
