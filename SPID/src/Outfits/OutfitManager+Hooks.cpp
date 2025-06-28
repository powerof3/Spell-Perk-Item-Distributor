#include "OutfitManager.h"
#include "Hooking.h"

namespace Outfits
{
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
			//#ifndef NDEBUG
			logger::info("Resurrect({}); IsDead: {}, ResetInventory: {}, Attach3D: {}", *(actor->As<RE::Actor>()), actor->IsDead(), resetInventory, attach3D);
			//#endif
			return Manager::GetSingleton()->ProcessResurrect(actor, resetInventory, [&] { return func(actor, resetInventory, attach3D); });
		}

		static inline void post_hook()
		{
			logger::info("\t\t🪝Installed Resurrect hook.");
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
					//#ifndef NDEBUG
					logger::info("RecycleActor({})", *(actor->As<RE::Actor>()));
					//#endif
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

	/// This hook is used to prevent game from re-initializing default outfit when SPID already manages actor's outfit.
	/// In cases when distribution is suspended we remove distributed outfit and allow game to restore default one.
	struct InitializeDefaultOutfit
	{
		static inline constexpr REL::ID     relocation = RELOCATION_ID(24221, 24725);
		static inline constexpr std::size_t offset = OFFSET(0x151, 0x156);

		static void thunk(RE::TESNPC* npc, RE::Actor* actor, bool arg3, bool arg4, bool arg5, bool arg6)
		{
			Manager::GetSingleton()->ProcessInitializeDefaultOutfit(npc, actor, [&] { func(npc, actor, arg3, arg4, arg5, arg6); });
		}

		static inline void post_hook()
		{
			logger::info("\t\t🪝Installed InitializeDefaultOutfit hook.");
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	/// This hook is used to prevent game from re-initializing default outfit when SPID already manages actor's outfit.
	/// In cases when distribution is suspended we remove distributed outfit and allow game to restore default one.
	struct EquipDefaultOutfitAfterResetInventory
	{
		static inline constexpr REL::ID     relocation = RELOCATION_ID(0, 37322);
		static inline constexpr std::size_t offset = OFFSET(0x0, 0xDF);

		static void thunk(RE::TESNPC* npc, RE::Actor* actor, bool arg3, bool arg4, bool arg5, bool arg6)
		{
			Manager::GetSingleton()->ProcessResetInventory(actor, true, [&] { func(npc, actor, arg3, arg4, arg5, arg6); });
		}

		static inline void post_hook()
		{
			logger::info("\t\t🪝Installed EquipDefaultOutfitAfterResetInventory hook.");
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	/// This hook is used to track when the game resets actor's inventory (thus removing outfit items)
	/// and queue re-applying SPID outfit next time the game tries to update worn gear.
	struct ResetInventory
	{
		static inline constexpr REL::ID     relocation = RELOCATION_ID(0, 37322);
		static inline constexpr std::size_t offset = OFFSET(0x0, 0x56);

		static void thunk(RE::Actor* actor, bool leveledOnly)
		{
			Manager::GetSingleton()->ProcessResetInventory(actor, false, [&] { func(actor, leveledOnly); });
		}

		static inline void post_hook()
		{
			logger::info("\t\t🪝Installed ResetInventory hook.");
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

	struct EquipObject
	{
		static inline constexpr REL::ID     relocation = RELOCATION_ID(37938, 38894);
		static inline constexpr std::size_t offset = OFFSET(0xE5, 0x170);

		static void thunk(RE::ActorEquipManager* manager, RE::Actor* actor, RE::TESBoundObject* object, RE::ExtraDataList* list)
		{
			if (actor && object) {
				logger::info("[EQUIP] {} equips {}", *actor, *object);
			}
			func(manager, actor, object, list);
		}

		static inline void post_hook()
		{
			logger::info("\t\t🪝Installed EquipObject hook.");
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct UnequipObject
	{
		static inline constexpr REL::ID     relocation = RELOCATION_ID(37945, 38901);
		static inline constexpr std::size_t offset = OFFSET(0x138, 0x1B9);

		static void thunk(RE::ActorEquipManager* manager, RE::Actor* actor, RE::TESBoundObject* object, RE::ExtraDataList* list)
		{
			if (actor && object) {
				logger::info("[UNEQUIP] {} unequips {}", *actor, *object);
			}
			func(manager, actor, object, list);
		}

		static inline void post_hook()
		{
			logger::info("\t\t🪝Installed UnequipObject hook.");
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

		stl::install_hook<InitItemImpl>();
		stl::install_hook<ShouldBackgroundClone>();

		stl::install_hook<Load3D>();
		stl::install_hook<Resurrect>();
		stl::install_hook<ResetReference>();
		stl::install_hook<SetOutfitActor>();

		stl::install_hook<EquipObject>();
		stl::install_hook<UnequipObject>();

		stl::install_hook<InitializeDefaultOutfit>();
		stl::install_hook<EquipDefaultOutfitAfterResetInventory>();
		stl::install_hook<ResetInventory>();
	}
}
