#include "Distribute.h"
#include "LookupForms.h"

namespace Distribute
{
	void Distribute(RE::TESNPC* a_actorbase, const PCLevelMult::Input& a_input)
	{
		if (a_input.onlyPlayerLevelEntries && PCLevelMult::Manager::GetSingleton()->HasHitLevelCap(a_input)) {
			return;
		}

		for_each_form<RE::BGSKeyword>(*a_actorbase, Forms::keywords, a_input, [&](auto* a_keyword, [[maybe_unused]] IdxOrCount a_count) {
			return a_actorbase->AddKeyword(a_keyword);
		});

		for_each_form<RE::TESFaction>(*a_actorbase, Forms::factions, a_input, [&](auto* a_faction, [[maybe_unused]] IdxOrCount a_count) {
			if (!a_actorbase->IsInFaction(a_faction)) {
				const RE::FACTION_RANK faction{ a_faction, 1 };
				a_actorbase->factions.push_back(faction);
				return true;
			}
			return false;
		});

		for_each_form<RE::BGSPerk>(*a_actorbase, Forms::perks, a_input, [&](auto* a_perk, [[maybe_unused]] IdxOrCount a_count) {
			return a_actorbase->AddPerk(a_perk, 1);
		});

		for_each_form<RE::SpellItem>(*a_actorbase, Forms::spells, a_input, [&](auto* a_spell, [[maybe_unused]] IdxOrCount a_count) {
			const auto actorEffects = a_actorbase->GetSpellList();
			return actorEffects && actorEffects->AddSpell(a_spell);
		});

		for_each_form<RE::TESShout>(*a_actorbase, Forms::shouts, a_input, [&](auto* a_shout, [[maybe_unused]] IdxOrCount a_count) {
			const auto actorEffects = a_actorbase->GetSpellList();
			return actorEffects && actorEffects->AddShout(a_shout);
		});

		for_each_form<RE::TESLevSpell>(*a_actorbase, Forms::levSpells, a_input, [&](auto* a_levSpell, [[maybe_unused]] IdxOrCount a_count) {
			const auto actorEffects = a_actorbase->GetSpellList();
			return actorEffects && actorEffects->AddLevSpell(a_levSpell);
		});

		for_each_form<RE::TESBoundObject>(*a_actorbase, Forms::items, a_input, [&](auto* a_item, IdxOrCount a_count) {
			return a_actorbase->AddObjectToContainer(a_item, a_count, a_actorbase);
		});

		for_each_form<RE::BGSOutfit>(*a_actorbase, Forms::outfits, a_input, [&](auto* a_outfit, [[maybe_unused]] IdxOrCount a_count) {
			if (a_actorbase->defaultOutfit != a_outfit) {
				a_actorbase->defaultOutfit = a_outfit;
				return true;
			}
			return false;
		});

		for_each_form<RE::TESForm>(*a_actorbase, Forms::packages, a_input, [&](auto* a_packageOrList, [[maybe_unused]] IdxOrCount a_idx) {
			auto packageIdx = a_idx;

			if (a_packageOrList->Is(RE::FormType::Package)) {
				auto package = a_packageOrList->As<RE::TESPackage>();

				if (packageIdx > 0) {
					--packageIdx;  //get actual position we want to insert at
				}

				auto& packageList = a_actorbase->aiPackages.packages;
				if (std::ranges::find(packageList, package) == packageList.end()) {
					if (packageList.empty() || packageIdx == 0) {
						packageList.push_front(package);
					} else {
						auto idxIt = packageList.begin();
						for (idxIt; idxIt != packageList.end(); ++idxIt) {
							auto idx = std::distance(packageList.begin(), idxIt);
							if (packageIdx == idx) {
								break;
							}
						}
						if (idxIt != packageList.end()) {
							packageList.insert_after(idxIt, package);
						}
					}
					return true;
				}
			} else if (a_packageOrList->Is(RE::FormType::FormList)) {
				auto packageList = a_packageOrList->As<RE::BGSListForm>();

				switch (packageIdx) {
				case 0:
					a_actorbase->defaultPackList = packageList;
					break;
				case 1:
					a_actorbase->spectatorOverRidePackList = packageList;
					break;
				case 2:
					a_actorbase->observeCorpseOverRidePackList = packageList;
					break;
				case 3:
					a_actorbase->guardWarnOverRidePackList = packageList;
					break;
				case 4:
					a_actorbase->enterCombatOverRidePackList = packageList;
					break;
				default:
					break;
				}

				return true;
			}

			return false;
		});

		for_each_form<RE::BGSOutfit>(*a_actorbase, Forms::sleepOutfits, a_input, [&](auto* a_outfit, [[maybe_unused]] IdxOrCount a_count) {
			if (a_actorbase->sleepOutfit != a_outfit) {
				a_actorbase->sleepOutfit = a_outfit;
				return true;
			}
			return false;
		});

		for_each_form<RE::TESObjectARMO>(*a_actorbase, Forms::skins, a_input, [&](auto* a_skin, [[maybe_unused]] IdxOrCount a_count) {
			if (a_actorbase->skin != a_skin) {
				a_actorbase->skin = a_skin;
				return true;
			}
			return false;
		});
	}

	void ApplyToNPCs()
	{
		constexpr auto uses_template = [](const RE::TESNPC* a_actorbase) -> bool {
			return a_actorbase->UsesTemplate() || a_actorbase->baseTemplateForm || a_actorbase->templateForms;
		};

		if (const auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler) {
			std::size_t totalNPCs = 0;
			for (const auto& actorbase : dataHandler->GetFormArray<RE::TESNPC>()) {
				if (actorbase && !actorbase->IsPlayer() && (!uses_template(actorbase) || actorbase->IsUnique())) {
					Distribute(actorbase, PCLevelMult::Input{ actorbase, false, true });
					totalNPCs++;
				}
			}

			logger::info("{:*^50}", "RESULTS");
			logger::info("{:*^50}", "[unique or non-templated NPCs]");

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
			const auto actorbase = actor ? actor->GetActorBase() : nullptr;
			if (actor && actorbase) {
				const PCLevelMult::Input input{
					actorbase,
					false,
					false,
				};
				for_each_form<RE::TESBoundObject>(*actorbase, Forms::deathItems, input, [&](auto* a_deathItem, IdxOrCount a_count) {
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

			if (!a_npc || !a_npc->IsDynamicForm()) {
				return;
			}

			Distribute(a_npc, PCLevelMult::Input{ a_this, a_npc, false, false });
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
