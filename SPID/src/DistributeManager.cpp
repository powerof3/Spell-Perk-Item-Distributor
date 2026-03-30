#include "DistributeManager.h"
#include "Distribute.h"
#include "DistributePCLevelMult.h"
#include "Hooking.h"

namespace Distribute
{
	bool detail::should_process_NPC(RE::TESNPC* a_npc, RE::BGSKeyword* a_keyword)
	{
		return !a_npc->IsPlayer() && !a_npc->IsDeleted();
	}

	void detail::distribute_on_load(RE::Actor* actor, RE::TESNPC* npc)
	{
		auto npcData = NPCData(actor, npc);
		if (should_process_NPC(npc)) {
			if (!npc->HasKeyword(processed)) {
				Distribute(npcData, false);
				npc->AddKeyword(processed);
			}
		}
	}

	namespace Actor
	{
		// General distribution
		// FF actors distribution
		struct ShouldBackgroundClone
		{
			using Target = RE::Character;
			static inline constexpr std::size_t index{ 0x6D };

			static bool thunk(RE::Character* actor)
			{
				//	logger::debug("Distribute: ShouldBackgroundClone({})", *(actor->As<RE::Actor>()));
				if (const auto npc = actor->GetActorBase()) {
					detail::distribute_on_load(actor, npc);
				}
				return func(actor);
			}

			static inline void post_hook()
			{
				logger::info("\t\t🪝Installed ShouldBackgroundClone hook.");
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};

		// Post distribution
		// Fixes weird behavior with leveled npcs?
		struct InitLoadGame
		{
			using Target = RE::Character;
			static inline constexpr std::size_t index{ 0x10 };

			static void thunk(RE::Character* a_this, RE::BGSLoadFormBuffer* a_buf)
			{
				func(a_this, a_buf);

				logger::debug("Distribute: InitLoadGame({})", *(a_this->As<RE::Actor>()));
				if (const auto npc = a_this->GetActorBase()) {
					// some leveled npcs are completely reset upon loading
					if (a_this->Is3DLoaded()) {
						// MAYBE: Test whether there are some NPCs that are getting in this branch
						// I haven't experienced issues with ShouldBackgroundClone hook.
						//logger::info("InitLoadGame({})", *a_this);
						detail::distribute_on_load(a_this, npc);
					}
				}
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		void Install()
		{
			logger::info("🧝Actors");
			//stl::install_hook<InitLoadGame>();
			stl::install_hook<ShouldBackgroundClone>();
		}
	}

	void Setup()
	{
		// Create tag keywords
		if (const auto factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSKeyword>()) {
			if (processed = factory->Create(); processed) {
				processed->formEditorID = "SPID_Processed";
			}
		}

		if (Forms::GetTotalLeveledEntries() > 0) {
			PlayerLeveledActor::Install();
		}

		LOG_HEADER("EVENTS");
		Event::Manager::Register();
		PCLevelMult::Manager::Register();
		logger::info("Registered event handlers");

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
			logger::info("Registered Distribution Manager for {}", typeid(RE::TESFormDeleteEvent).name());
		}
	}

	RE::BSEventNotifyControl Manager::ProcessEvent(const RE::TESFormDeleteEvent* a_event, RE::BSTEventSource<RE::TESFormDeleteEvent>*)
	{
		if (a_event && a_event->formID != 0) {
			PCLevelMult::Manager::GetSingleton()->DeleteNPC(a_event->formID);
		}
		return RE::BSEventNotifyControl::kContinue;
	}
}
