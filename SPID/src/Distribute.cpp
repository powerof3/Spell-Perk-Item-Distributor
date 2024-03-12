#include "Distribute.h"

#include "DistributeManager.h"

namespace Distribute
{
	namespace detail
	{
		void add_item(RE::Actor* a_actor, RE::TESBoundObject* a_item, std::uint32_t a_itemCount)
		{
			using func_t = void (*)(RE::Actor*, RE::TESBoundObject*, std::uint32_t, bool, std::uint32_t, RE::BSScript::Internal::VirtualMachine*);
			REL::Relocation<func_t> func{ RELOCATION_ID(55945, 56489) };
			return func(a_actor, a_item, a_itemCount, true, 0, RE::BSScript::Internal::VirtualMachine::GetSingleton());
		}
	}

	void Distribute(NPCData& a_npcData, const PCLevelMult::Input& a_input)
	{
		if (a_input.onlyPlayerLevelEntries && PCLevelMult::Manager::GetSingleton()->HasHitLevelCap(a_input)) {
			return;
		}

		const auto npc = a_npcData.GetNPC();

		for_each_form<RE::BGSKeyword>(a_npcData, Forms::keywords, a_input, [&](const std::vector<RE::BGSKeyword*>& a_keywords) {
			npc->AddKeywords(a_keywords);
		});

		for_each_form<RE::TESFaction>(a_npcData, Forms::factions, a_input, [&](const std::vector<RE::TESFaction*>& a_factions) {
			npc->factions.reserve(static_cast<std::uint32_t>(a_factions.size()));
			for (auto& faction : a_factions) {
				npc->factions.emplace_back(RE::FACTION_RANK{ faction, 1 });
			}
		});

		for_each_form<RE::BGSPerk>(a_npcData, Forms::perks, a_input, [&](const std::vector<RE::BGSPerk*>& a_perks) {
			npc->AddPerks(a_perks, 1);
		});

		for_each_form<RE::SpellItem>(a_npcData, Forms::spells, a_input, [&](const std::vector<RE::SpellItem*>& a_spells) {
			npc->GetSpellList()->AddSpells(a_spells);
		});

		for_each_form<RE::TESLevSpell>(a_npcData, Forms::levSpells, a_input, [&](const std::vector<RE::TESLevSpell*>& a_levSpells) {
			npc->GetSpellList()->AddLevSpells(a_levSpells);
		});

		for_each_form<RE::TESShout>(a_npcData, Forms::shouts, a_input, [&](const std::vector<RE::TESShout*>& a_shouts) {
			npc->GetSpellList()->AddShouts(a_shouts);
		});

		for_each_form<RE::TESForm>(a_npcData, Forms::packages, a_input, [&](auto* a_packageOrList, [[maybe_unused]] IdxOrCount a_idx) {
			auto packageIdx = a_idx;

			if (a_packageOrList->Is(RE::FormType::Package)) {
				auto package = a_packageOrList->As<RE::TESPackage>();

				if (packageIdx > 0) {
					--packageIdx;  //get actual position we want to insert at
				}

				auto& packageList = npc->aiPackages.packages;
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
					npc->defaultPackList = packageList;
					break;
				case 1:
					npc->spectatorOverRidePackList = packageList;
					break;
				case 2:
					npc->observeCorpseOverRidePackList = packageList;
					break;
				case 3:
					npc->guardWarnOverRidePackList = packageList;
					break;
				case 4:
					npc->enterCombatOverRidePackList = packageList;
					break;
				default:
					break;
				}

				return true;
			}

			return false;
		});

		for_each_form<RE::TESObjectARMO>(a_npcData, Forms::skins, a_input, [&](auto* a_skin) {
			if (npc->skin != a_skin) {
				npc->skin = a_skin;
				return true;
			}
			return false;
		});

		for_each_form<RE::BGSOutfit>(a_npcData, Forms::outfits, a_input, [&](auto* a_outfit) {
			if (npc->defaultOutfit != a_outfit) {
				npc->defaultOutfit = a_outfit;
				return true;
			}
			return false;
		});

		for_each_form<RE::BGSOutfit>(a_npcData, Forms::sleepOutfits, a_input, [&](auto* a_outfit) {
			if (npc->sleepOutfit != a_outfit) {
				npc->sleepOutfit = a_outfit;
				return true;
			}
			return false;
		});
	}

	void DistributeItems(NPCData& a_npcData, const PCLevelMult::Input& a_input)
	{
		if (a_input.onlyPlayerLevelEntries && PCLevelMult::Manager::GetSingleton()->HasHitLevelCap(a_input)) {
			return;
		}

		const auto npc = a_npcData.GetNPC();
		const auto actor = a_npcData.GetActor();

		auto inv_before = actor->GetInventory([](auto& item) {
			return item.Is(RE::FormType::Weapon, RE::FormType::Armor);
		});

		bool recalcInventory = false;
		for_each_form<RE::TESBoundObject>(a_npcData, Forms::items, a_input, [&](std::map<RE::TESBoundObject*, IdxOrCount>& a_objects, const bool a_hasLvlItem) {
			if (npc->AddObjectsToContainer(a_objects, npc)) {
				if (a_hasLvlItem) {
					recalcInventory = true;
					if (const auto invChanges = actor->GetInventoryChanges(true)) {
						invChanges->InitLeveledItems();
					}
				} else {
					for (auto& [item, count] : a_objects) {
						if (item->Is(RE::FormType::Weapon, RE::FormType::Armor)) {
							RE::ActorEquipManager::GetSingleton()->EquipObject(actor, item);
						}
					}
				}
				return true;
			}
			return false;
		});

		if (recalcInventory) {
			auto inv_after = actor->GetInventory([](auto& item) {
				return item.Is(RE::FormType::Weapon, RE::FormType::Armor);
			});
			for (auto& [item, data] : inv_after) {
				auto& [count, extra] = data;
				if (!inv_before.contains(item) && count > 0 && !extra->IsWorn()) {
					RE::ActorEquipManager::GetSingleton()->EquipObject(actor, item);
				}
			}
		}
	}

	void Distribute(NPCData& a_npcData, bool a_onlyLeveledEntries, bool a_noItems)
	{
		const auto input = PCLevelMult::Input{ a_npcData.GetActor(), a_npcData.GetNPC(), a_onlyLeveledEntries };

		Distribute(a_npcData, input);
		if (!a_noItems) {
			DistributeItems(a_npcData, input);
		}
	}
}
