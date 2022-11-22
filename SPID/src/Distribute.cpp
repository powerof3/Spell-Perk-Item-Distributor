#include "Distribute.h"
#include "LookupForms.h"

namespace Distribute
{
	bool detail::uses_template(const RE::TESNPC* a_npc)
	{
		return a_npc->UsesTemplate() || a_npc->baseTemplateForm || a_npc->templateForms;
	}

	void Distribute(const NPCData& a_npcData, const PCLevelMult::Input& a_input)
	{
		if (a_input.onlyPlayerLevelEntries && PCLevelMult::Manager::GetSingleton()->HasHitLevelCap(a_input)) {
			return;
		}

		for_each_form<RE::BGSKeyword>(a_npcData, Forms::keywords, a_input, [&](auto* a_keyword, [[maybe_unused]] IdxOrCount a_count) {
			return a_npcData.GetNPC()->AddKeyword(a_keyword);
		});

		for_each_form<RE::TESFaction>(a_npcData, Forms::factions, a_input, [&](auto* a_faction, [[maybe_unused]] IdxOrCount a_count) {
			if (!a_npcData.GetNPC()->IsInFaction(a_faction)) {
				const RE::FACTION_RANK faction{ a_faction, 1 };
				a_npcData.GetNPC()->factions.push_back(faction);
				return true;
			}
			return false;
		});

		for_each_form<RE::BGSPerk>(a_npcData, Forms::perks, a_input, [&](auto* a_perk, [[maybe_unused]] IdxOrCount a_count) {
			return a_npcData.GetNPC()->AddPerk(a_perk, 1);
		});

		for_each_form<RE::SpellItem>(a_npcData, Forms::spells, a_input, [&](auto* a_spell, [[maybe_unused]] IdxOrCount a_count) {
			const auto actorEffects = a_npcData.GetNPC()->GetSpellList();
			return actorEffects && actorEffects->AddSpell(a_spell);
		});

		for_each_form<RE::TESShout>(a_npcData, Forms::shouts, a_input, [&](auto* a_shout, [[maybe_unused]] IdxOrCount a_count) {
			const auto actorEffects = a_npcData.GetNPC()->GetSpellList();
			return actorEffects && actorEffects->AddShout(a_shout);
		});

		for_each_form<RE::TESLevSpell>(a_npcData, Forms::levSpells, a_input, [&](auto* a_levSpell, [[maybe_unused]] IdxOrCount a_count) {
			const auto actorEffects = a_npcData.GetNPC()->GetSpellList();
			return actorEffects && actorEffects->AddLevSpell(a_levSpell);
		});

		for_each_form<RE::TESBoundObject>(a_npcData, Forms::items, a_input, [&](auto* a_item, IdxOrCount a_count) {
			return a_npcData.GetNPC()->AddObjectToContainer(a_item, a_count, a_npcData.GetNPC());
		});

		for_each_form<RE::BGSOutfit>(a_npcData, Forms::outfits, a_input, [&](auto* a_outfit, [[maybe_unused]] IdxOrCount a_count) {
			if (a_npcData.GetNPC()->defaultOutfit != a_outfit) {
				a_npcData.GetNPC()->defaultOutfit = a_outfit;
				return true;
			}
			return false;
		});

		for_each_form<RE::TESForm>(a_npcData, Forms::packages, a_input, [&](auto* a_packageOrList, [[maybe_unused]] IdxOrCount a_idx) {
			auto packageIdx = a_idx;

			if (a_packageOrList->Is(RE::FormType::Package)) {
				auto package = a_packageOrList->As<RE::TESPackage>();

				if (packageIdx > 0) {
					--packageIdx;  //get actual position we want to insert at
				}

				auto& packageList = a_npcData.GetNPC()->aiPackages.packages;
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
					a_npcData.GetNPC()->defaultPackList = packageList;
					break;
				case 1:
					a_npcData.GetNPC()->spectatorOverRidePackList = packageList;
					break;
				case 2:
					a_npcData.GetNPC()->observeCorpseOverRidePackList = packageList;
					break;
				case 3:
					a_npcData.GetNPC()->guardWarnOverRidePackList = packageList;
					break;
				case 4:
					a_npcData.GetNPC()->enterCombatOverRidePackList = packageList;
					break;
				default:
					break;
				}

				return true;
			}

			return false;
		});

		for_each_form<RE::BGSOutfit>(a_npcData, Forms::sleepOutfits, a_input, [&](auto* a_outfit, [[maybe_unused]] IdxOrCount a_count) {
			if (a_npcData.GetNPC()->sleepOutfit != a_outfit) {
				a_npcData.GetNPC()->sleepOutfit = a_outfit;
				return true;
			}
			return false;
		});

		for_each_form<RE::TESObjectARMO>(a_npcData, Forms::skins, a_input, [&](auto* a_skin, [[maybe_unused]] IdxOrCount a_count) {
			if (a_npcData.GetNPC()->skin != a_skin) {
				a_npcData.GetNPC()->skin = a_skin;
				return true;
			}
			return false;
		});
	}

	void ApplyToNPCs()
	{
		if (const auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler) {
			std::size_t totalNPCs = 0;
			for (const auto& npc : dataHandler->GetFormArray<RE::TESNPC>()) {
				if (npc && !npc->IsPlayer() && (!detail::uses_template(npc) || npc->IsUnique())) {
					Distribute(NPCData{ npc }, PCLevelMult::Input{ npc, false, true });
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
