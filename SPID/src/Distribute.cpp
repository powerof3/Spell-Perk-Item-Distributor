#include "Distribute.h"

#include "DistributeManager.h"
#include "LinkedDistribution.h"

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

		/// <summary>
		/// Performs distribution of all configured forms to NPC described with npcData and input.
		/// </summary>
		/// <param name="npcData">General information about NPC that is being processed.</param>
		/// <param name="input">Leveling information about NPC that is being processed.</param>
		/// <param name="forms">A set of forms that should be distributed to NPC.</param>
		/// <param name="accumulatedForms">An optional pointer to a set that will accumulate all distributed forms.</param>
		void distribute(NPCData& npcData, const PCLevelMult::Input& input, Forms::DistributionSet& forms, std::set<RE::TESForm*>* accumulatedForms)
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

			for_each_form<RE::BGSPerk>(
				npcData, forms.perks, input, [&](const std::vector<RE::BGSPerk*>& a_perks) {
					npc->AddPerks(a_perks, 1);
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
				},
				accumulatedForms);

			for_each_form<RE::BGSOutfit>(
				npcData, forms.outfits, input, [&](auto* a_outfit) {
					if (npc->defaultOutfit != a_outfit && !npc->HasKeyword(processedOutfit)) {
						npc->AddKeyword(processedOutfit);
						npc->defaultOutfit = a_outfit;
						return true;
					}
					return false;
				},
				accumulatedForms);

			for_each_form<RE::BGSOutfit>(
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

			for_each_form<RE::TESObjectARMO>(
				npcData, forms.skins, input, [&](auto* a_skin) {
					if (npc->skin != a_skin) {
						npc->skin = a_skin;
						return true;
					}
					return false;
				},
				accumulatedForms);
		}
	}

	// This only does one-level linking. So that linked entries won't trigger another level of distribution.
	void DistributeLinkedEntries(NPCData& npcData, const PCLevelMult::Input& input, const std::set<RE::TESForm*>& forms)
	{
		LinkedDistribution::Manager::GetSingleton()->ForEachLinkedDistributionSet(forms, [&](Forms::DistributionSet& set) {
			detail::distribute(npcData, input, set, nullptr);  // TODO: Accumulate forms here?
		});
	}

	void Distribute(NPCData& a_npcData, const PCLevelMult::Input& a_input)
	{
		if (a_input.onlyPlayerLevelEntries && PCLevelMult::Manager::GetSingleton()->HasHitLevelCap(a_input)) {
			return;
		}

		// TODO: Figure out how to distribute only death items perhaps?
		Forms::DistributionSet entries{
			Forms::spells.GetForms(a_input.onlyPlayerLevelEntries),
			Forms::perks.GetForms(a_input.onlyPlayerLevelEntries),
			Forms::items.GetForms(a_input.onlyPlayerLevelEntries),
			Forms::shouts.GetForms(a_input.onlyPlayerLevelEntries),
			Forms::levSpells.GetForms(a_input.onlyPlayerLevelEntries),
			Forms::packages.GetForms(a_input.onlyPlayerLevelEntries),
			Forms::outfits.GetForms(a_input.onlyPlayerLevelEntries),
			Forms::keywords.GetForms(a_input.onlyPlayerLevelEntries),
			Forms::DistributionSet::empty<RE::TESBoundObject>(),  // deathItems are only processed on... well, death.
			Forms::factions.GetForms(a_input.onlyPlayerLevelEntries),
			Forms::sleepOutfits.GetForms(a_input.onlyPlayerLevelEntries),
			Forms::skins.GetForms(a_input.onlyPlayerLevelEntries)
		};

		std::set<RE::TESForm*> distributedForms{};

		detail::distribute(a_npcData, a_input, entries, &distributedForms);
		// TODO: We can now log per-NPC distributed forms.

		if (!distributedForms.empty()) {
			DistributeLinkedEntries(a_npcData, a_input, distributedForms);
		}
	}

	void Distribute(NPCData& a_npcData, bool a_onlyLeveledEntries)
	{
		const auto input = PCLevelMult::Input{ a_npcData.GetActor(), a_npcData.GetNPC(), a_onlyLeveledEntries };
		Distribute(a_npcData, input);
	}
}
