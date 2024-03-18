#include "Distribute.h"

#include "DistributeManager.h"

namespace Distribute
{
	namespace detail
	{
		void equip_worn_outfit(RE::Actor* actor, const RE::BGSOutfit* a_outfit)
		{
			if (!actor || !a_outfit) {
				return;
			}

			if (const auto invChanges = actor->GetInventoryChanges()) {
				if (const auto entryLists = invChanges->entryList) {
					const auto formID = a_outfit->GetFormID();

					for (const auto& entryList : *entryLists) {
						if (entryList && entryList->object && entryList->extraLists) {
							for (const auto& xList : *entryList->extraLists) {
								const auto outfitItem = xList ? xList->GetByType<RE::ExtraOutfitItem>() : nullptr;
								if (outfitItem && outfitItem->id == formID) {
									RE::ActorEquipManager::GetSingleton()->EquipObject(actor, entryList->object, xList, 1, nullptr, true);
								}
							}
						}
					}
				}
			}
		}

		void add_item(RE::Actor* a_actor, RE::TESBoundObject* a_item, std::uint32_t a_itemCount)
		{
			using func_t = void (*)(RE::Actor*, RE::TESBoundObject*, std::uint32_t, bool, std::uint32_t, RE::BSScript::Internal::VirtualMachine*);
			REL::Relocation<func_t> func{ RELOCATION_ID(55945, 56489) };
			return func(a_actor, a_item, a_itemCount, true, 0, RE::BSScript::Internal::VirtualMachine::GetSingleton());
		}

		void init_leveled_items(RE::Actor* a_actor)
		{
			if (const auto invChanges = a_actor->GetInventoryChanges(true)) {
				invChanges->InitLeveledItems();
			}
		}

		bool can_equip_outfit(const RE::TESNPC* a_npc, RE::BGSOutfit* a_outfit)
		{
			if (a_npc->HasKeyword(processedOutfit) || a_npc->defaultOutfit == a_outfit) {
				return false;
			}

			return true;
		}

		/// <summary>
		/// Performs distribution of all configured forms to NPC described with a_npcData and a_input.
		/// </summary>
		/// <param name="a_npcData">General information about NPC that is being processed.</param>
		/// <param name="a_input">Leveling information about NPC that is being processed.</param>
		/// <param name="forms">A set of forms that should be distributed to NPC.</param>
		/// <param name="accumulatedForms">An optional pointer to a set that will accumulate all distributed forms.</param>
		void distribute(NPCData& a_npcData, const PCLevelMult::Input& a_input, Forms::DistributionSet& forms, std::set<RE::TESForm*>* accumulatedForms)
		{
			const auto npc = a_npcData.GetNPC();

			for_each_form<RE::BGSKeyword>(
				a_npcData, forms.keywords, a_input, [&](const std::vector<RE::BGSKeyword*>& a_keywords) {
					npc->AddKeywords(a_keywords);
				},
				accumulatedForms);

			for_each_form<RE::TESFaction>(
				a_npcData, forms.factions, a_input, [&](const std::vector<RE::TESFaction*>& a_factions) {
					npc->factions.reserve(static_cast<std::uint32_t>(a_factions.size()));
					for (auto& faction : a_factions) {
						npc->factions.emplace_back(RE::FACTION_RANK{ faction, 1 });
					}
				},
				accumulatedForms);

			for_each_form<RE::SpellItem>(
				a_npcData, forms.spells, a_input, [&](const std::vector<RE::SpellItem*>& a_spells) {
					npc->GetSpellList()->AddSpells(a_spells);
				},
				accumulatedForms);

			for_each_form<RE::TESLevSpell>(
				a_npcData, forms.levSpells, a_input, [&](const std::vector<RE::TESLevSpell*>& a_levSpells) {
					npc->GetSpellList()->AddLevSpells(a_levSpells);
				},
				accumulatedForms);

			for_each_form<RE::BGSPerk>(
				a_npcData, forms.perks, a_input, [&](const std::vector<RE::BGSPerk*>& a_perks) {
					npc->AddPerks(a_perks, 1);
				},
				accumulatedForms);

			for_each_form<RE::TESShout>(
				a_npcData, forms.shouts, a_input, [&](const std::vector<RE::TESShout*>& a_shouts) {
					npc->GetSpellList()->AddShouts(a_shouts);
				},
				accumulatedForms);

			for_each_form<RE::TESForm>(
				a_npcData, forms.packages, a_input, [&](auto* a_packageOrList, [[maybe_unused]] IndexOrCount a_idx) {
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

			for_each_form<RE::TESObjectARMO>(
				a_npcData, forms.skins, a_input, [&](auto* a_skin) {
					if (npc->skin != a_skin) {
						npc->skin = a_skin;
						return true;
					}
					return false;
				},
				accumulatedForms);

			for_each_form<RE::BGSOutfit>(
				a_npcData, forms.sleepOutfits, a_input, [&](auto* a_outfit) {
					if (npc->sleepOutfit != a_outfit) {
						npc->sleepOutfit = a_outfit;
						return true;
					}
					return false;
				},
				accumulatedForms);
		}

	}

	// This only does one-level linking. So that linked entries won't trigger another level of distribution.
	void DistributeLinkedEntries(NPCData& npcData, const PCLevelMult::Input& a_input, const std::set<RE::TESForm*>& forms)
	{
		// TODO: Get linked entries and repeat distribution for them.

		Forms::DistributionSet entries{};
		detail::distribute(npcData, a_input, entries, nullptr);
	}

	void Distribute(NPCData& a_npcData, const PCLevelMult::Input& a_input)
	{
		if (a_input.onlyPlayerLevelEntries && PCLevelMult::Manager::GetSingleton()->HasHitLevelCap(a_input)) {
			return;
		}

		Forms::DistributionSet entries{
			Forms::spells.GetForms(a_input.onlyPlayerLevelEntries),
			Forms::perks.GetForms(a_input.onlyPlayerLevelEntries),
			{},  // items are processed separately
			Forms::shouts.GetForms(a_input.onlyPlayerLevelEntries),
			Forms::levSpells.GetForms(a_input.onlyPlayerLevelEntries),
			Forms::packages.GetForms(a_input.onlyPlayerLevelEntries),
			{},  // outfits are processed along with items.
			Forms::keywords.GetForms(a_input.onlyPlayerLevelEntries),
			{},  // deathItems are only processed on... well, death.
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

	void DistributeItemOutfits(NPCData& a_npcData, const PCLevelMult::Input& a_input)
	{
		if (a_input.onlyPlayerLevelEntries && PCLevelMult::Manager::GetSingleton()->HasHitLevelCap(a_input)) {
			return;
		}

		const auto npc = a_npcData.GetNPC();
		const auto actor = a_npcData.GetActor();

		for_each_form<RE::TESBoundObject>(a_npcData, Forms::items.GetForms(a_input.onlyPlayerLevelEntries), a_input, [&](std::map<RE::TESBoundObject*, Count>& a_objects, const bool a_hasLvlItem) {
			if (npc->AddObjectsToContainer(a_objects, npc)) {
				if (a_hasLvlItem) {
					detail::init_leveled_items(actor);
				}
				return true;
			}
			return false;
		});

		for_each_form<RE::BGSOutfit>(a_npcData, Forms::outfits.GetForms(a_input.onlyPlayerLevelEntries), a_input, [&](auto* a_outfit) {
			if (detail::can_equip_outfit(npc, a_outfit)) {
				actor->RemoveOutfitItems(npc->defaultOutfit);
				npc->defaultOutfit = a_outfit;
				npc->AddKeyword(processedOutfit);
				return true;
			}
			return false;
		});
	}

	void Distribute(NPCData& a_npcData, bool a_onlyLeveledEntries, bool a_noItemOutfits)
	{
		const auto input = PCLevelMult::Input{ a_npcData.GetActor(), a_npcData.GetNPC(), a_onlyLeveledEntries };

		Distribute(a_npcData, input);
		if (!a_noItemOutfits) {
			DistributeItemOutfits(a_npcData, input);
		}
	}
}
