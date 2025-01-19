#include "DistributePCLevelMult.h"
#include "Distribute.h"
#include "DistributeManager.h"
#include "Hooking.h"
#include "PCLevelMultManager.h"

namespace Distribute::PlayerLeveledActor
{
	struct HandleUpdatePlayerLevel
	{
		// ProcessLists::HandlePlayerLevelUpdate
		// inlined into SetLevel in AE
		static inline constexpr REL::ID     relocation = RELOCATION_ID(40575, 41567);
		static inline constexpr std::size_t offset = OFFSET(0x97, 0x137);

		static void thunk(RE::Actor* a_actor)
		{
			if (const auto npc = a_actor->GetActorBase(); npc && npc->HasKeyword(processed)) {
				auto npcData = NPCData(a_actor, npc);
				Distribute(npcData, true);
			}

			func(a_actor);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	// Reset previous save/character dist. entries
	struct Revert
	{
		using Target = RE::Character;
		static inline constexpr size_t index{ 0x12 };

		static void thunk(RE::Character* a_this, RE::BGSLoadFormBuffer* a_buf)
		{
			func(a_this, a_buf);

			if (const auto npc = a_this->GetActorBase(); npc && npc->HasPCLevelMult() && npc->HasKeyword(processed)) {
				const auto pcLevelMultManager = PCLevelMult::Manager::GetSingleton();

				auto input = PCLevelMult::Input{ a_this, npc, true };
				input.playerID = pcLevelMultManager->GetOldPlayerID();

				pcLevelMultManager->ForEachDistributedEntry(input, false, [&](RE::FormType a_formType, const Set<RE::FormID>& a_formIDSet) {
					switch (a_formType) {
					case RE::FormType::Keyword:
						{
							const auto keywords = detail::set_to_vec<RE::BGSKeyword>(a_formIDSet);
							npc->RemoveKeywords(keywords);
						}
						break;
					case RE::FormType::Faction:
						{
							const auto factions = detail::set_to_vec<RE::TESFaction>(a_formIDSet);
							for (auto& factionRank : npc->factions) {
								if (std::ranges::find(factions, factionRank.faction) != factions.end()) {
									factionRank.rank = -1;
								}
							}
						}
						break;
					case RE::FormType::Perk:
						{
							const auto perks = detail::set_to_vec<RE::BGSPerk>(a_formIDSet);
							npc->RemovePerks(perks);
						}
						break;
					case RE::FormType::Spell:
						{
							const auto spells = detail::set_to_vec<RE::SpellItem>(a_formIDSet);
							npc->GetSpellList()->RemoveSpells(spells);
						}
						break;
					case RE::FormType::LeveledSpell:
						{
							const auto spells = detail::set_to_vec<RE::TESLevSpell>(a_formIDSet);
							npc->GetSpellList()->RemoveLevSpells(spells);
						}
						break;
					case RE::FormType::Shout:
						{
							const auto shouts = detail::set_to_vec<RE::TESShout>(a_formIDSet);
							npc->GetSpellList()->RemoveShouts(shouts);
						}
						break;
					default:
						break;
					}
				});
			}
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	// Re-add dist entries if level is valid
	struct LoadGame
	{
		using Target = RE::Character;
		static inline constexpr size_t index{ 0xF };

		static void thunk(RE::Character* a_this, RE::BGSLoadFormBuffer* a_buf)
		{
			if (const auto npc = a_this->GetActorBase(); npc && npc->HasPCLevelMult() && npc->HasKeyword(processed)) {
				const auto pcLevelMultManager = PCLevelMult::Manager::GetSingleton();
				const auto input = PCLevelMult::Input{ a_this, npc, true };

				if (!pcLevelMultManager->FindDistributedEntry(input)) {
					//start distribution of leveled entries for first time
					auto npcData = NPCData(a_this, npc);
					Distribute(npcData, true);
				} else {
					//handle redistribution
					pcLevelMultManager->ForEachDistributedEntry(input, true, [&](RE::FormType a_formType, const Set<RE::FormID>& a_formIDSet) {
						switch (a_formType) {
						case RE::FormType::Keyword:
							{
								const auto keywords = detail::set_to_vec<RE::BGSKeyword>(a_formIDSet);
								npc->AddKeywords(keywords);
							}
							break;
						case RE::FormType::Faction:
							{
								const auto factions = detail::set_to_vec<RE::TESFaction>(a_formIDSet);
								for (auto& factionRank : npc->factions) {
									if (std::ranges::find(factions, factionRank.faction) != factions.end()) {
										factionRank.rank = 1;
									}
								}
							}
							break;
						case RE::FormType::Perk:
							{
								const auto perks = detail::set_to_vec<RE::BGSPerk>(a_formIDSet);
								npc->AddPerks(perks, 1);
							}
							break;
						case RE::FormType::Spell:
							{
								const auto spells = detail::set_to_vec<RE::SpellItem>(a_formIDSet);
								npc->GetSpellList()->AddSpells(spells);
							}
							break;
						case RE::FormType::LeveledSpell:
							{
								const auto spells = detail::set_to_vec<RE::TESLevSpell>(a_formIDSet);
								npc->GetSpellList()->AddLevSpells(spells);
							}
							break;
						case RE::FormType::Shout:
							{
								const auto shouts = detail::set_to_vec<RE::TESShout>(a_formIDSet);
								npc->GetSpellList()->AddShouts(shouts);
							}
							break;
						default:
							break;
						}
					});
				}
			}

			func(a_this, a_buf);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	void Install()
	{
		stl::install_hook<HandleUpdatePlayerLevel>();

		stl::install_hook<Revert>();
		stl::install_hook<LoadGame>();

		LOG_HEADER("HOOKS");
		logger::info("Installed leveled distribution hooks");
	}
}
