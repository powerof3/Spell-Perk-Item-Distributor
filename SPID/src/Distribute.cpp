#include "Distribute.h"

#include "DeathDistribution.h"
#include "DistributeManager.h"
#include "LinkedDistribution.h"
#include "OutfitManager.h"

namespace Distribute
{
	void Distribute(NPCData& npcData, const PCLevelMult::Input& input, Forms::DistributionSet& forms, DistributedForms* accumulatedForms)
	{
		const auto npc = npcData.GetNPC();

		for_each_form<RE::BGSKeyword>(
			npcData, forms.keywords, input, [&](const std::vector<RE::BGSKeyword*>& a_keywords) {
				npc->AddKeywords(a_keywords);
			},
			accumulatedForms);

		for_each_form<RE::TESFaction>(
			npcData, forms.factions, input, [&](const std::vector<RE::TESFaction*>& a_factions) {
				npc->factions.reserve(static_cast<std::uint32_t>(a_factions.size()));
				for (auto& faction : a_factions) {
					npc->factions.emplace_back(RE::FACTION_RANK{ faction, 1 });
				}
			},
			accumulatedForms);

		for_each_form<RE::BGSPerk>(
			npcData, forms.perks, input, [&](const std::vector<RE::BGSPerk*>& a_perks) {
				npc->AddPerks(a_perks, 1);
			},
			accumulatedForms);

		for_each_form<RE::SpellItem>(
			npcData, forms.spells, input, [&](const std::vector<RE::SpellItem*>& a_spells) {
				npc->GetSpellList()->AddSpells(a_spells);
			},
			accumulatedForms);

		for_each_form<RE::TESLevSpell>(
			npcData, forms.levSpells, input, [&](const std::vector<RE::TESLevSpell*>& a_levSpells) {
				npc->GetSpellList()->AddLevSpells(a_levSpells);
			},
			accumulatedForms);

		for_each_form<RE::TESShout>(
			npcData, forms.shouts, input, [&](const std::vector<RE::TESShout*>& a_shouts) {
				npc->GetSpellList()->AddShouts(a_shouts);
			},
			accumulatedForms);

		for_each_form<RE::TESForm>(
			npcData, forms.packages, input, [&](auto* a_packageOrList, [[maybe_unused]] IndexOrCount a_idx) {
				auto packageIdx = std::get<Index>(a_idx);

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
				}
			},
			accumulatedForms);

		for_first_form<RE::BGSOutfit>(
			npcData, forms.outfits, input, [&](auto* a_outfit) {
				return Outfits::Manager::GetSingleton()->SetDefaultOutfit(npcData.GetActor(), a_outfit);  // terminate as soon as valid outfit is confirmed.
			},
			accumulatedForms);

		for_first_form<RE::BGSOutfit>(
			npcData, forms.sleepOutfits, input, [&](auto* a_outfit) {
				if (npc->sleepOutfit != a_outfit) {
					npc->sleepOutfit = a_outfit;
					return true;
				}
				return false;
			},
			accumulatedForms);

		for_each_form<RE::TESBoundObject>(
			npcData, forms.items, input, [&](std::map<RE::TESBoundObject*, Count>& a_objects) {
				return npc->AddObjectsToContainer(a_objects, npc);
			},
			accumulatedForms);

		for_first_form<RE::TESObjectARMO>(
			npcData, forms.skins, input, [&](auto* a_skin) {
				if (npc->skin != a_skin) {
					npc->skin = a_skin;
					return true;
				}
				return false;
			},
			accumulatedForms);
	}

	void Distribute(NPCData& npcData, const PCLevelMult::Input& input)
	{
		if (input.onlyPlayerLevelEntries && PCLevelMult::Manager::GetSingleton()->HasHitLevelCap(input))
			return;

		Forms::DistributionSet entries{
			Forms::spells.GetForms(input.onlyPlayerLevelEntries),
			Forms::perks.GetForms(input.onlyPlayerLevelEntries),
			Forms::items.GetForms(input.onlyPlayerLevelEntries),
			Forms::shouts.GetForms(input.onlyPlayerLevelEntries),
			Forms::levSpells.GetForms(input.onlyPlayerLevelEntries),
			Forms::packages.GetForms(input.onlyPlayerLevelEntries),
			Forms::outfits.GetForms(input.onlyPlayerLevelEntries),
			Forms::keywords.GetForms(input.onlyPlayerLevelEntries),
			Forms::factions.GetForms(input.onlyPlayerLevelEntries),
			Forms::sleepOutfits.GetForms(input.onlyPlayerLevelEntries),
			Forms::skins.GetForms(input.onlyPlayerLevelEntries)
		};

		DistributedForms distributedForms{};

		Distribute(npcData, input, entries, &distributedForms);

		if (!distributedForms.empty()) {
			// TODO: This only does one-level linking. So that linked entries won't trigger another level of distribution.
			LinkedDistribution::Manager::GetSingleton()->ForEachLinkedDistributionSet(LinkedDistribution::kRegular, distributedForms, [&](Forms::DistributionSet& set) {
				Distribute(npcData, input, set, &distributedForms);
			});
		}

		LogDistribution(distributedForms, npcData);
	}

	void Distribute(NPCData& npcData, bool onlyLeveledEntries)
	{
		const auto input = PCLevelMult::Input{ npcData.GetActor(), npcData.GetNPC(), onlyLeveledEntries };
		Distribute(npcData, input);

		if (npcData.IsDead()) {  // If NPC is already dead, perform the On Death Distribution.
			DeathDistribution::Manager::GetSingleton()->Distribute(npcData);
		}
	}

	void LogDistribution(const DistributedForms& forms, NPCData& npcData)
	{
#ifndef NDEBUG
		std::map<std::string_view, std::vector<DistributedForm>> results;

		for (const auto& form : forms) {
			results[RE::FormTypeToString(form.first->GetFormType())].push_back(form);
		}

		logger::info("Distribution for {}", *npcData.GetActor());
		for (const auto& pair : results) {
			logger::info("\t{}", pair.first);
			for (const auto& form : pair.second) {
				logger::info("\t\t{} @ {}", *form.first, form.second);
			}
		}
#endif
	}
}
