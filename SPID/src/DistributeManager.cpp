#include "DistributeManager.h"
#include "Distribute.h"
#include "DistributePCLevelMult.h"

namespace Distribute
{
	bool detail::should_process_NPC(RE::TESNPC* a_npc, RE::BGSKeyword* a_keyword)
	{
		if (a_npc->IsDeleted() || a_npc->HasKeyword(a_keyword)) {
			return false;
		}

		a_npc->AddKeyword(a_keyword);

		return true;
	}

	void detail::force_equip_outfit(RE::Actor* a_actor, const RE::TESNPC* a_npc)
	{
		if (!a_actor->HasOutfitItems(a_npc->defaultOutfit) && !a_actor->IsDead()) {
			if (const auto invChanges = a_actor->GetInventoryChanges()) {
				invChanges->InitOutfitItems(a_npc->defaultOutfit, a_npc->GetLevel());
			}
		}
		equip_worn_outfit(a_actor, a_npc->defaultOutfit);
	}

	namespace Actor
	{
		// FF actor/outfit distribution
		struct ShouldBackgroundClone
		{
			static bool thunk(RE::Character* a_this)
			{
				if (const auto npc = a_this->GetActorBase()) {
					const auto process = detail::should_process_NPC(npc);
					const auto processOnLoad = detail::should_process_NPC(npc, processedOnLoad);
					if (process || processOnLoad) {
						auto npcData = NPCData(a_this, npc);
						if (process) {
							Distribute(npcData, false, true);
						}
						if (processOnLoad) {
							DistributeItemOutfits(npcData, { a_this, npc, false });
						}
					}
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
					if (npc->HasKeyword(processedOutfit)) {
						detail::force_equip_outfit(a_this, npc);
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
			stl::write_vfunc<RE::Character, Load3D>();
			stl::write_vfunc<RE::Character, InitLoadGame>();

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
			if (processedOutfit = factory->Create(); processedOutfit) {
				processedOutfit->formEditorID = "SPID_ProcessedOutfit";
			}
			if (processedOnLoad = factory->Create(); processedOnLoad) {
				processedOnLoad->formEditorID = "SPID_ProcessedOnLoad";
			}
		}

		if (Forms::GetTotalLeveledEntries() > 0) {
			PlayerLeveledActor::Install();
		}

		logger::info("{:*^50}", "EVENTS");
		Event::Manager::Register();
		PCLevelMult::Manager::Register();

		DoInitialDistribution();

		// Clear logger's buffer to free some memory :)
		buffered_logger::clear();
	}

	void DoInitialDistribution()
	{
		Timer         timer;
		std::uint32_t actorCount = 0;

		if (const auto processLists = RE::ProcessLists::GetSingleton()) {
			timer.start();
			processLists->ForAllActors([&](RE::Actor* a_actor) {
				if (const auto npc = a_actor->GetActorBase(); npc && detail::should_process_NPC(npc)) {
					auto npcData = NPCData(a_actor, npc);
					Distribute(npcData, false, true);
					++actorCount;
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});
			timer.end();
		}

		LogResults(actorCount);

		logger::info("Distribution took {}μs / {}ms", timer.duration_μs(), timer.duration_ms());
	}

	void LogResults(std::uint32_t a_actorCount)
	{
		using namespace Forms;

		logger::info("{:*^50}", "RESULTS");

		ForEachDistributable([&]<typename Form>(Distributables<Form>& a_distributable) {
			if (a_distributable && a_distributable.GetType() != RECORD::kItem && a_distributable.GetType() != RECORD::kOutfit && a_distributable.GetType() != RECORD::kDeathItem) {
				logger::info("{}", RECORD::add[a_distributable.GetType()]);

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
