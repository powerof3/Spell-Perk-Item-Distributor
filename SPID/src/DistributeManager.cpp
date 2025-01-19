#include "DistributeManager.h"
#include "Distribute.h"
#include "DistributePCLevelMult.h"
#include "Hooking.h"

namespace Distribute
{
	bool detail::should_process_NPC(RE::TESNPC* a_npc, RE::BGSKeyword* a_keyword)
	{
		if (a_npc->IsPlayer() || a_npc->IsDeleted()) {
			return false;
		}

		return true;
	}

	void detail::distribute_on_load(RE::Actor* a_actor, RE::TESNPC* a_npc)
	{
		if (should_process_NPC(a_npc)) {
			auto npcData = NPCData(a_actor, a_npc);
			Distribute(npcData, false);
			a_npc->AddKeyword(processed);
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
				logger::debug("Distribute: ShouldBackgroundClone({})", *(actor->As<RE::Actor>()));
				if (const auto npc = actor->GetActorBase()) {
					detail::distribute_on_load(actor, npc);
				}
				return func(actor);
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
						// TODO: Test whether there are some NPCs that are getting in this branch
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
			//stl::install_hook<InitLoadGame>();
			stl::install_hook<ShouldBackgroundClone>();

			logger::info("Installed actor load hooks");
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

		// TODO: No initial distribution. Check Packages distribution and see if those work as intended.
		//DoInitialDistribution();

		// Clear logger's buffer to free some memory :)
		buffered_logger::clear();
	}

	void DoInitialDistribution()
	{
		Timer         timer;
		std::uint32_t actorCount = 0;

		if (const auto processLists = RE::ProcessLists::GetSingleton()) {
			timer.start();

			// This iterate over all actors loaded in-memory.
			// lowActorHandles are the actors that do not require high precision updates.
			// As we are in the main menu this will target most of static unique actors.
			for (auto& actorHandle : processLists->lowActorHandles) {
				if (const auto& actor = actorHandle.get()) {
					if (const auto npc = actor->GetActorBase(); npc && detail::should_process_NPC(npc)) {
						auto npcData = NPCData(actor.get(), npc);
						Distribute(npcData, false);
						++actorCount;
					}
				}
			}
			timer.end();
		}

		LogResults(actorCount);

		logger::info("Distribution took {}μs / {}ms", timer.duration_μs(), timer.duration_ms());
	}

	void LogResults(std::uint32_t a_actorCount)
	{
		using namespace Forms;

		LOG_HEADER("MAIN MENU DISTRIBUTION");

		ForEachDistributable([&]<typename Form>(Distributables<Form>& a_distributable) {
			if (a_distributable) {
				logger::info("{}", RECORD::GetTypeName(a_distributable.GetType()));

				auto& forms = a_distributable.GetForms();

				// Group the same entries together to avoid duplicates in the log.
				std::map<RE::FormID, Data<Form>> sums{};
				for (auto& formData : forms) {
					if (const auto& form = formData.form) {
						auto it = sums.find(form->GetFormID());
						if (it != sums.end()) {
							it->second.npcCount += formData.npcCount;
						} else {
							sums.insert({ form->GetFormID(), formData });
						}
					}
				}

				for (auto& entry : sums) {
					auto& formData = entry.second;
					if (const auto& form = formData.form) {
						if (auto file = form->GetFile(0)) {
							logger::info("\t\t{} [0x{:X}~{}] added to {}/{} NPCs", editorID::get_editorID(form), form->GetLocalFormID(), file->GetFilename(), formData.npcCount, a_actorCount);
						} else {
							logger::info("\t\t{} [0x{:X}] added to {}/{} NPCs", editorID::get_editorID(form), form->GetFormID(), formData.npcCount, a_actorCount);
						}
					}
				}

				sums.clear();
			}
		});
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
