#include "Distribute.h"

#include "DeathDistribution.h"
#include "DistributeManager.h"
#include "LinkedDistribution.h"
#include "Outfits/OutfitManager.h"

namespace Distribute
{
	void Distribute(NPCData& npcData, const PCLevelMult::Input& input, Forms::DistributionSet& forms, DistributedForms* accumulatedForms, OutfitDistributor distributeOutfit)
	{
		const auto npc = npcData.GetNPC();
		const auto actor = npcData.GetActor();

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
			npcData, forms.spells, input, [&](const std::vector<RE::SpellItem*>& spells) {
				npc->GetSpellList()->AddSpells(spells);
			},
			accumulatedForms);
		// Apply abilities that were distributed. This is especially important when distributed to dead actors, since game doesn't do this by default.
		actor->CastPermanentMagic(false, true, false, false);

		for_each_form<RE::TESLevSpell>(
			npcData, forms.levSpells, input, [&](const std::vector<RE::TESLevSpell*>& levSpells) {
				npc->GetSpellList()->AddLevSpells(levSpells);
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

		for_each_form<RE::TESBoundObject>(
			npcData, forms.items, input, [&](std::map<RE::TESBoundObject*, Count>& a_objects) {
				// MAYBE: Per-actor item distribution. Would require similar manager as in outfits :) but would be cool, right?
				// adding objects to actors directly put them in inventory changes, so these items are baked into the save.
				// to mitiage it, we would need to remove such items whenever a new distribution is triggered.
				/*for (auto object : a_objects) {
					actor->AddObjectToContainer(object.first, nullptr, object.second, actor);
				}
				return true;*/
				return npc->AddObjectsToContainer(a_objects, npc);
			},
			accumulatedForms);

		for_first_form<RE::TESObjectARMO>(
			npcData, forms.skins, input, [&](auto* a_skin, bool isFinal) {
				if (npc->skin != a_skin) {
					npc->skin = a_skin;
					return true;
				}
				return false;
			},
			accumulatedForms);

		for_first_form<RE::BGSOutfit>(
			npcData, forms.sleepOutfits, input, [&](auto* a_outfit, bool isFinal) {
				if (npc->sleepOutfit != a_outfit) {
					npc->sleepOutfit = a_outfit;
					return true;
				}
				return false;
			},
			accumulatedForms);

		for_first_form<RE::BGSOutfit>(
			npcData, forms.outfits, input, [&](auto* outfit, bool isFinal) {
				return distributeOutfit(npcData, outfit, isFinal);  // terminate as soon as valid outfit is confirmed.
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
			Forms::DistributionSet::empty<RE::BGSOutfit>(),  // Outfits are distributed separately.
			Forms::keywords.GetForms(input.onlyPlayerLevelEntries),
			Forms::factions.GetForms(input.onlyPlayerLevelEntries),
			Forms::sleepOutfits.GetForms(input.onlyPlayerLevelEntries),
			Forms::skins.GetForms(input.onlyPlayerLevelEntries)
		};

		DistributedForms distributedForms{};

		Distribute(npcData, input, entries, &distributedForms, Outfits::SetDefaultOutfit);

		if (!distributedForms.empty()) {
			// MAYBE: This only does one-level linking. So that linked entries won't trigger another level of distribution.
			LinkedDistribution::Manager::GetSingleton()->ForEachLinkedDistributionSet(LinkedDistribution::kRegular, distributedForms, [&](Forms::DistributionSet& set) {
				Distribute(npcData, input, set, &distributedForms, Outfits::SetDefaultOutfit);
			});
		}

		LogDistribution(distributedForms, npcData);
	}

	void DistributeOutfits(NPCData& npcData, const PCLevelMult::Input& input)
	{
		if (input.onlyPlayerLevelEntries && PCLevelMult::Manager::GetSingleton()->HasHitLevelCap(input))
			return;

		Forms::DistributionSet entries{
			Forms::DistributionSet::empty<RE::SpellItem>(),
			Forms::DistributionSet::empty<RE::BGSPerk>(),
			Forms::DistributionSet::empty<RE::TESBoundObject>(),
			Forms::DistributionSet::empty<RE::TESShout>(),
			Forms::DistributionSet::empty<RE::TESLevSpell>(),
			Forms::DistributionSet::empty<RE::TESForm>(),
			Forms::outfits.GetForms(input.onlyPlayerLevelEntries),
			Forms::DistributionSet::empty<RE::BGSKeyword>(),
			Forms::DistributionSet::empty<RE::TESFaction>(),
			Forms::DistributionSet::empty<RE::BGSOutfit>(),
			Forms::DistributionSet::empty<RE::TESObjectARMO>()
		};

		DistributedForms distributedForms{};

		Distribute(npcData, input, entries, &distributedForms, Outfits::SetDefaultOutfit);

		LogDistribution(distributedForms, npcData, true);
	}

	void Distribute(NPCData& npcData, bool onlyLeveledEntries)
	{
		const auto input = PCLevelMult::Input{ npcData.GetActor(), npcData.GetNPC(), onlyLeveledEntries };

		// We always do the normal distribution even for Dead NPCs,
		// if Distributable Form is only meant to be distributed while NPC is alive, the entry must contain -D filter.
		Distribute(npcData, input);
	}

	void DistributeOutfits(NPCData& npcData, bool onlyLeveledEntries)
	{
		const auto input = PCLevelMult::Input{ npcData.GetActor(), npcData.GetNPC(), onlyLeveledEntries };

		// We always do the normal distribution even for Dead NPCs,
		// if Distributable Form is only meant to be distributed while NPC is alive, the entry must contain -D filter.
		DistributeOutfits(npcData, input);
	}

	void LogDistribution(const DistributedForms& forms, NPCData& npcData, bool append)
	{
		//#ifndef NDEBUG
		std::map<std::string_view, std::vector<DistributedForm>> results;

		for (const auto& form : forms) {
			results[RE::FormTypeToString(form.first->GetFormType())].push_back(form);
		}

		if (!append) {
			logger::info("Distribution for {}", *npcData.GetActor());
		}
		if (results.empty()) {
			if (!append) {
				logger::info("\tNothing");
			}
		} else {
			for (const auto& pair : results) {
				logger::info("\t{}", pair.first);
				for (const auto& form : pair.second) {
					logger::info("\t\t{} @ {}", *form.first, form.second);
				}
			}
		}
		//#endif
	}
}
