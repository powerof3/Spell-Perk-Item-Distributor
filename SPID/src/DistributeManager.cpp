#include "DistributeManager.h"
#include "Distribute.h"
#include "DistributePCLevelMult.h"
#include "LookupForms.h"

namespace Distribute
{
	bool detail::should_process_NPC(RE::TESNPC* a_npc)
	{
		if (a_npc->IsDeleted() || a_npc->HasKeyword(processed)) {
			return false;
		}

		a_npc->AddKeyword(processed);

		return true;
	}

	void detail::force_equip_outfit(RE::Actor* a_actor, const RE::TESNPC* a_npc)
	{
		if (!a_actor->HasOutfitItems(a_npc->defaultOutfit)) {
			if (const auto invChanges = a_actor->GetInventoryChanges(false)) {
				invChanges->InitOutfitItems(a_npc->defaultOutfit, a_npc->GetLevel());
			}
		}
		equip_worn_outfit(a_actor, a_npc->defaultOutfit);
	}

	namespace Actor
	{
		// Initial distribution
		struct ShouldBackgroundClone
		{
			static bool thunk(RE::Character* a_this)
			{
				if (auto npc = a_this->GetActorBase(); npc && detail::should_process_NPC(npc)) {
					auto npcData = NPCData(a_this, npc);
					Distribute(npcData, false);
				}

				return func(a_this);
			}
			static inline REL::Relocation<decltype(thunk)> func;

			static inline constexpr std::size_t index{ 0 };
			static inline constexpr std::size_t size{ 0x6D };
		};

		// Force outfit equip
		struct Load3D
		{
			static RE::NiAVObject* thunk(RE::Character* a_this, bool a_arg1)
			{
				const auto root = func(a_this, a_arg1);

				if (const auto npc = a_this->GetActorBase()) {
					if (npc->HasKeyword(processedOutfit)) {
						detail::force_equip_outfit(a_this, npc);
					}
				}

				return root;
			}

			static inline REL::Relocation<decltype(thunk)> func;

			static inline constexpr std::size_t index{ 0 };
			static inline constexpr std::size_t size{ 0x6A };
		};

		// Post distribution
		struct InitLoadGame
		{
			static void thunk(RE::Character* a_this, RE::BGSLoadFormBuffer* a_buf)
			{
				func(a_this, a_buf);

				if (const auto npc = a_this->GetActorBase()) {
					// some npcs are completely reset upon loading
					if (a_this->Is3DLoaded() && detail::should_process_NPC(npc)) {
						auto npcData = NPCData(a_this, npc);
						Distribute(npcData, false);
					}
					/*if (npc->HasKeyword(processedOutfit)) {
						detail::force_equip_outfit(a_this, npc);
					}*/
				}
			}
			static inline REL::Relocation<decltype(thunk)> func;

			static inline constexpr std::size_t index{ 0 };
			static inline constexpr std::size_t size{ 0x10 };
		};

		void Install()
		{
			stl::write_vfunc<RE::Character, ShouldBackgroundClone>();
			// stl::write_vfunc<RE::Character, Load3D>();
			stl::write_vfunc<RE::Character, InitLoadGame>();

			logger::info("Installed actor load hooks");
		}
	}

	namespace NPC
	{
		struct CopyFromTemplateForms
		{
			static void thunk(RE::TESActorBaseData* a_this, RE::TESActorBase** a_templateForms)
			{
				func(a_this, a_templateForms);

				if (!a_this) {
					return;
				}

				const auto npc = stl::adjust_pointer<RE::TESNPC>(a_this, -0x30);
				if (!npc || npc->IsDeleted()) {
					return;
				}

				std::call_once(distributeInit, []() {
					if (shouldDistribute = Lookup::DoFormLookup(); shouldDistribute) {
						SetupDistribution();
					}
				});

				const auto npcData = NPCData(npc);
				for_each_form<RE::BGSOutfit>(npcData, Forms::outfits, [&](auto* a_outfit) {
					if (detail::can_equip_outfit(npc, a_outfit)) {
						npc->defaultOutfit = a_outfit;
						return true;
					}
					return false;
				});
			}
			static inline REL::Relocation<decltype(thunk)> func;

			static inline size_t index{ 1 };
			static inline size_t size{ 0x4 };
		};

		void Install()
		{
			stl::write_vfunc<RE::TESNPC, CopyFromTemplateForms>();
			logger::info("Installed npc init hooks");
		}
	}

	void SetupDistribution()
	{
		// Create tag keywords
		if (const auto factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSKeyword>()) {
			if (processed = factory->Create(); processed) {
				processed->formEditorID = processed_EDID;
			}
			if (processedOutfit = factory->Create(); processedOutfit) {
				processedOutfit->formEditorID = processedOutfit_EDID;
			}
		}

		if (Forms::GetTotalLeveledEntries() > 0) {
			PlayerLeveledActor::Install();
		}

		logger::info("{:*^50}", "EVENTS");
		Event::Manager::Register();
		PCLevelMult::Manager::Register();

		// Clear logger's buffer to free some memory :)
		buffered_logger::clear();
	}
}

namespace Distribute::Event
{
	void Manager::Register()
	{
		if (const auto scripts = RE::ScriptEventSourceHolder::GetSingleton()) {
			scripts->AddEventSink<RE::TESFormDeleteEvent>(GetSingleton());
			logger::info("Registered for {}", typeid(RE::TESFormDeleteEvent).name());

			if (Forms::deathItems) {
				scripts->AddEventSink<RE::TESDeathEvent>(GetSingleton());
				logger::info("Registered for {}", typeid(RE::TESDeathEvent).name());
			}
		}
	}

	RE::BSEventNotifyControl Manager::ProcessEvent(const RE::TESDeathEvent* a_event, RE::BSTEventSource<RE::TESDeathEvent>*)
	{
		constexpr auto is_NPC = [](auto&& a_ref) {
			return a_ref && !a_ref->IsPlayerRef();
		};

		if (a_event && a_event->dead && is_NPC(a_event->actorDying)) {
			const auto actor = a_event->actorDying->As<RE::Actor>();
			const auto npc = actor ? actor->GetActorBase() : nullptr;
			if (actor && npc) {
				const auto npcData = NPCData(actor, npc);
				const auto input = PCLevelMult::Input{ actor, npc, false };

				for_each_form<RE::TESBoundObject>(npcData, Forms::deathItems, input, [&](auto* a_deathItem, IdxOrCount a_count) {
					detail::add_item(actor, a_deathItem, a_count);
					return true;
				});
			}
		}

		return RE::BSEventNotifyControl::kContinue;
	}

	RE::BSEventNotifyControl Manager::ProcessEvent(const RE::TESFormDeleteEvent* a_event, RE::BSTEventSource<RE::TESFormDeleteEvent>*)
	{
		if (a_event && a_event->formID != 0) {
			PCLevelMult::Manager::GetSingleton()->DeleteNPC(a_event->formID);
		}
		return RE::BSEventNotifyControl::kContinue;
	}
}
