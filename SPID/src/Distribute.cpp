#include "Distribute.h"
#include "LookupForms.h"

namespace Distribute
{
	void Distribute(RE::TESNPC* a_actorbase, bool a_onlyPlayerLevelEntries, bool a_noPlayerLevelDistribution)
	{
		const PCLevelMult::Input input{
			a_actorbase,
			a_onlyPlayerLevelEntries,
			a_noPlayerLevelDistribution
		};

		if (a_onlyPlayerLevelEntries && PCLevelMult::Manager::GetSingleton()->HasHitLevelCap(input)) {
			return;
		}

		for_each_form<RE::BGSKeyword>(*a_actorbase, Forms::keywords, input, [&](const auto& a_keywordsPair) {
			const auto keyword = a_keywordsPair.first;
			return a_actorbase->AddKeyword(keyword);
		});

		for_each_form<RE::TESFaction>(*a_actorbase, Forms::factions, input, [&](const auto& a_factionPair) {
			if (!a_actorbase->IsInFaction(a_factionPair.first)) {
				const RE::FACTION_RANK faction{ a_factionPair.first, 1 };
				a_actorbase->factions.push_back(faction);
				return true;
			}
			return false;
		});

		for_each_form<RE::BGSPerk>(*a_actorbase, Forms::perks, input, [&](const auto& a_perkPair) {
			const auto perk = a_perkPair.first;
			return a_actorbase->AddPerk(perk, 1);
		});

		for_each_form<RE::SpellItem>(*a_actorbase, Forms::spells, input, [&](const auto& a_spellPair) {
			const auto spell = a_spellPair.first;
			const auto actorEffects = a_actorbase->GetSpellList();
			return actorEffects && actorEffects->AddSpell(spell);
		});

		for_each_form<RE::TESShout>(*a_actorbase, Forms::shouts, input, [&](const auto& a_shoutPair) {
			const auto shout = a_shoutPair.first;
			const auto actorEffects = a_actorbase->GetSpellList();
			return actorEffects && actorEffects->AddShout(shout);
		});

		for_each_form<RE::TESLevSpell>(*a_actorbase, Forms::levSpells, input, [&](const auto& a_levSpellPair) {
			const auto levSpell = a_levSpellPair.first;
			const auto actorEffects = a_actorbase->GetSpellList();
			return actorEffects && actorEffects->AddLevSpell(levSpell);
		});

		for_each_form<RE::TESBoundObject>(*a_actorbase, Forms::items, input, [&](const auto& a_itemPair) {
			const auto& [item, count] = a_itemPair;
			return a_actorbase->AddObjectToContainer(item, count, a_actorbase);
		});

		for_each_form<RE::BGSOutfit>(*a_actorbase, Forms::outfits, input, [&](const auto& a_outfitPair) {
			if (a_actorbase->defaultOutfit != a_outfitPair.first) {
				a_actorbase->defaultOutfit = a_outfitPair.first;
				return true;
			}
			return false;
		});

		for_each_form<RE::TESForm>(*a_actorbase, Forms::packages, input, [&](const auto& a_packagePair) {
			auto packageForm = a_packagePair.first;
			auto packageIdx = a_packagePair.second;

			if (packageForm->Is(RE::FormType::Package)) {
				auto package = packageForm->As<RE::TESPackage>();

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
			} else if (packageForm->Is(RE::FormType::FormList)) {
				auto packageList = packageForm->As<RE::BGSListForm>();

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

		for_each_form<RE::BGSOutfit>(*a_actorbase, Forms::sleepOutfits, input, [&](const auto& a_outfitPair) {
			if (a_actorbase->sleepOutfit != a_outfitPair.first) {
				a_actorbase->sleepOutfit = a_outfitPair.first;
				return true;
			}
			return false;
		});

		for_each_form<RE::TESObjectARMO>(*a_actorbase, Forms::skins, input, [&](const auto& a_skinPair) {
			if (a_actorbase->skin != a_skinPair.first) {
				a_actorbase->skin = a_skinPair.first;
				return true;
			}
			return false;
		});
	}

	void ApplyToNPCs()
	{
		if (const auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler) {
			std::size_t totalNPCs = 0;
			for (const auto& actorbase : dataHandler->GetFormArray<RE::TESNPC>()) {
				if (actorbase && !actorbase->IsPlayer() && (!actorbase->UsesTemplate() || actorbase->IsUnique())) {
					Distribute(actorbase, false, true);
					totalNPCs++;
				}
			}

			logger::info("{:*^50}", "RESULTS");
			logger::info("{:*^50}", "[unique or non-templated NPCs]");

			const auto list_result = [&totalNPCs]<class Form>(const RECORD::TYPE a_recordType, Forms::FormMap<Form>& a_forms) {
				if (a_forms) {
					const auto& recordName = RECORD::add[a_recordType];
					list_npc_count(recordName, a_forms, totalNPCs);
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

namespace Distribute::DeathItem
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
		if (!Forms::deathItems) {
			return;
		}

		if (auto scripts = RE::ScriptEventSourceHolder::GetSingleton()) {
			scripts->AddEventSink(GetSingleton());
			logger::info("	Registered {}"sv, typeid(Manager).name());
		}
	}

	EventResult Manager::ProcessEvent(const RE::TESDeathEvent* a_event, RE::BSTEventSource<RE::TESDeathEvent>*)
	{
		constexpr auto is_NPC = [](auto&& a_ref) {
			return a_ref && !a_ref->IsPlayerRef();
		};

		if (a_event && a_event->dead && is_NPC(a_event->actorDying)) {
			const auto actor = a_event->actorDying->As<RE::Actor>();
			const auto actorBase = actor ? actor->GetActorBase() : nullptr;
			if (actor && actorBase) {
				PCLevelMult::Input input{
					actorBase,
					false,
					false,
				};
				for_each_form<RE::TESBoundObject>(*actorBase, Forms::deathItems, input, [&](const auto& a_deathItemPair) {
					const auto& [deathItem, count] = a_deathItemPair;
					detail::add_item(actor, deathItem, count, true, 0, RE::BSScript::Internal::VirtualMachine::GetSingleton());
					return true;
				});
			}
		}

		return EventResult::kContinue;
	}
}

namespace Distribute::LeveledActor
{
	struct CopyFromTemplateForms
	{
		static void thunk(RE::TESActorBaseData* a_this, RE::TESActorBase** a_templateForms)
		{
			func(a_this, a_templateForms);

			if (!a_this->baseTemplateForm || !a_templateForms) {
				return;
			}

			if (const auto actorbase = stl::adjust_pointer<RE::TESNPC>(a_this, -0x30); actorbase) {
				Distribute(actorbase, false, false);
			}
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static inline size_t index{ 1 };
		static inline size_t size{ 0x4 };
	};

	void Install()
	{
		stl::write_vfunc<RE::TESNPC, CopyFromTemplateForms>();
		logger::info("	Hooked leveled actor init");
	}
}
