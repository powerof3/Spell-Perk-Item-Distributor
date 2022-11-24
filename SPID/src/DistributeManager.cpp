#include "DistributeManager.h"
#include "Distribute.h"

namespace Distribute
{
	bool detail::uses_template(const RE::TESNPC* a_npc)
	{
		return a_npc->UsesTemplate() || a_npc->baseTemplateForm || a_npc->templateForms;
	}

	void OnInit()
	{
		if (const auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler) {
			std::size_t totalNPCs = 0;

			const auto startTime = std::chrono::system_clock::now();
			for (const auto& npc : dataHandler->GetFormArray<RE::TESNPC>()) {
				if (npc && !npc->IsPlayer() && (!detail::uses_template(npc) || npc->IsUnique())) {
					Distribute(NPCData{ npc }, PCLevelMult::Input{ npc, false, true });
					totalNPCs++;
				}
			}
			const auto endTime = std::chrono::system_clock::now();

			logger::info("{:*^50}", "RESULTS");
			logger::info("{:*^50}", "[unique or non-templated NPCs]");

			logger::info("Distribution took {}ms", static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count()));

			const auto list_result = [&totalNPCs]<class Form>(const RECORD::TYPE a_recordType, Forms::Distributables<Form>& a_distributables) {
				if (a_distributables) {
					const auto& recordName = RECORD::add[a_recordType];
					list_npc_count(recordName, a_distributables, totalNPCs);
				}
			};

			list_result(RECORD::kKeyword, Forms::keywords);
			list_result(RECORD::kSpell, Forms::spells);
			list_result(RECORD::kPerk, Forms::perks);
			list_result(RECORD::kItem, Forms::items);
			list_result(RECORD::kShout, Forms::shouts);
			list_result(RECORD::kLevSpell, Forms::levSpells);
			list_result(RECORD::kPackage, Forms::packages);
			list_result(RECORD::kOutfit, Forms::outfits);
			list_result(RECORD::kDeathItem, Forms::deathItems);
			list_result(RECORD::kFaction, Forms::factions);
			list_result(RECORD::kSleepOutfit, Forms::sleepOutfits);
			list_result(RECORD::kSkin, Forms::skins);
		}
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
			logger::info("\tRegistered for {}", typeid(RE::TESFormDeleteEvent).name());
			if (Forms::deathItems) {
				scripts->AddEventSink<RE::TESDeathEvent>(GetSingleton());
				logger::info("\tRegistered for {}", typeid(RE::TESDeathEvent).name());
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
				const PCLevelMult::Input input{
					npc,
					false,
					false,
				};
				const NPCData npcData{
					actor,
					npc
				};
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

namespace Distribute::LeveledActor
{
	struct SetObjectReference
	{
		static void thunk(RE::Character* a_this, RE::TESNPC* a_npc)
		{
			func(a_this, a_npc);

			if (a_npc && (a_npc->IsDynamicForm() || detail::uses_template(a_npc))) {
				Distribute(NPCData{ a_this, a_npc }, PCLevelMult::Input{ a_this, a_npc, false, false });
			}
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static inline size_t index{ 0 };
		static inline size_t size{ 0x84 };
	};

	void Install()
	{
		stl::write_vfunc<RE::Character, SetObjectReference>();
		logger::info("\tHooked leveled actor init");
	}
}
