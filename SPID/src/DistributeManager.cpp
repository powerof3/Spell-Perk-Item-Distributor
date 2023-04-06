#include "DistributeManager.h"
#include "Distribute.h"
#include "DistributePCLevelMult.h"

namespace Distribute
{
	bool detail::should_process_NPC(RE::TESNPC* a_npc)
	{
		if (a_npc->HasKeyword(processed)) {
			return false;
		}

		a_npc->AddKeyword(processed);

		return true;
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
					if (npc->HasKeyword(processedOutfit) && !a_this->HasOutfitItems(npc->defaultOutfit)) {
						a_this->InitInventoryIfRequired();
						detail::equip_worn_outfit(a_this, npc->defaultOutfit);
					}
				}
			}
			static inline REL::Relocation<decltype(thunk)> func;

			static inline constexpr std::size_t index{ 0 };
			static inline constexpr std::size_t size{ 0x10 };
		};

		void Install()
		{
			stl::write_vfunc<RE::Character, ShouldBackgroundClone>();
			stl::write_vfunc<RE::Character, InitLoadGame>();

			logger::info("Installed actor load hooks");
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
	struct detail  //AddObjectToContainer doesn't work with leveled items :s
	{
		static void add_item(RE::Actor* a_actor, RE::TESBoundObject* a_item, std::uint32_t a_itemCount, bool a_silent, std::uint32_t a_stackID, RE::BSScript::Internal::VirtualMachine* a_vm)
		{
			using func_t = decltype(&detail::add_item);
			REL::Relocation<func_t> func{ RELOCATION_ID(55945, 56489) };
			return func(a_actor, a_item, a_itemCount, a_silent, a_stackID, a_vm);
		}
	};

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
				auto       npcData = NPCData(actor, npc);
				const auto input = PCLevelMult::Input{ actor, npc, false };

				for_each_form<RE::TESBoundObject>(npcData, Forms::deathItems, input, [&](auto* a_deathItem, IdxOrCount a_count) {
					detail::add_item(actor, a_deathItem, a_count, true, 0, RE::BSScript::Internal::VirtualMachine::GetSingleton());
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
