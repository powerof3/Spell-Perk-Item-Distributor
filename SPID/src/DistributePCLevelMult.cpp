#include "DistributePCLevelMult.h"
#include "Distribute.h"
#include "DistributeManager.h"
#include "PCLevelMultManager.h"

namespace Distribute::PlayerLeveledActor
{
	struct HandleUpdatePlayerLevel
	{
		static void thunk(RE::Actor* a_actor)
		{
			if (const auto npc = a_actor->GetActorBase(); npc && npc->HasKeyword(processedKeyword)) {
				const auto npcData = std::make_unique<NPCData>(a_actor, npc);
				Distribute(*npcData, PCLevelMult::Input{ a_actor, npc, true });
			}

			func(a_actor);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	// Reset previous save/character dist. entries
	struct Revert
	{
		static void thunk(RE::Character* a_this, std::uintptr_t a_buf)
		{
			func(a_this, a_buf);

			if (const auto npc = a_this->GetActorBase(); npc && npc->HasPCLevelMult() && npc->HasKeyword(processedKeyword)) {
				const auto pcLevelMultManager = PCLevelMult::Manager::GetSingleton();

				auto input = PCLevelMult::Input{ a_this, npc, true };
				input.playerID = pcLevelMultManager->GetOldPlayerID();

				pcLevelMultManager->ForEachDistributedEntry(input, [&](RE::FormType a_formType, const Set<RE::FormID>& a_formIDSet, bool) {
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
							for (auto& faction : factions) {
								const auto it = std::ranges::find_if(npc->factions, [&](const auto& factionRank) {
									return factionRank.faction == faction;
								});
								if (it != npc->factions.end()) {
									(*it).rank = -1;
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
							if (const auto actorEffects = npc->GetSpellList()) {
								actorEffects->RemoveSpells(spells);
							}
						}
						break;
					case RE::FormType::LeveledSpell:
						{
							const auto spells = detail::set_to_vec<RE::TESLevSpell>(a_formIDSet);
							if (const auto actorEffects = npc->GetSpellList()) {
								actorEffects->RemoveLevSpells(spells);
							}
						}
						break;
					case RE::FormType::Shout:
						{
							const auto shouts = detail::set_to_vec<RE::TESShout>(a_formIDSet);
							if (const auto actorEffects = npc->GetSpellList()) {
								actorEffects->RemoveShouts(shouts);
							}
						}
						break;
					default:
						break;
					}
				});
			}
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static inline size_t index{ 0 };
		static inline size_t size{ 0x12 };
	};

	// Re-add dist entries if level is valid
	struct LoadGame
	{
		static void thunk(RE::Character* a_this, std::uintptr_t a_buf)
		{
			if (const auto npc = a_this->GetActorBase(); npc && npc->HasPCLevelMult() && npc->HasKeyword(processedKeyword)) {
				const auto pcLevelMultManager = PCLevelMult::Manager::GetSingleton();
				const auto input = PCLevelMult::Input{ a_this, npc, true };

				if (!pcLevelMultManager->FindDistributedEntry(input)) {
					//start distribution of leveled entries for first time
					const auto npcData = std::make_unique<NPCData>(a_this, npc);
					Distribute(*npcData, input);
				} else {
					//handle redistribution
					pcLevelMultManager->ForEachDistributedEntry(input, [&](RE::FormType a_formType, const Set<RE::FormID>& a_formIDSet, bool a_isBelowLevel) {
						switch (a_formType) {
						case RE::FormType::Keyword:
							{
								if (!a_isBelowLevel) {
									const auto keywords = detail::set_to_vec<RE::BGSKeyword>(a_formIDSet);
									npc->AddKeywords(keywords);
								}
							}
							break;
						case RE::FormType::Faction:
							{
								if (!a_isBelowLevel) {
									const auto factions = detail::set_to_vec<RE::TESFaction>(a_formIDSet);
									for (auto& faction : factions) {
										const auto it = std::ranges::find_if(npc->factions, [&](const auto& factionRank) {
											return factionRank.faction == faction;
										});
										if (it != npc->factions.end()) {
											(*it).rank = 1;
										}
									}
								}
							}
							break;
						case RE::FormType::Perk:
							{
								if (!a_isBelowLevel) {
									const auto perks = detail::set_to_vec<RE::BGSPerk>(a_formIDSet);
									npc->AddPerks(perks, 1);
								}
							}
							break;
						case RE::FormType::Spell:
							{
								if (!a_isBelowLevel) {
									if (const auto actorEffects = npc->GetSpellList()) {
										const auto spells = detail::set_to_vec<RE::SpellItem>(a_formIDSet);
										actorEffects->AddSpells(spells);
									}
								}
							}
							break;
						case RE::FormType::LeveledSpell:
							{
								if (!a_isBelowLevel) {
									const auto spells = detail::set_to_vec<RE::TESLevSpell>(a_formIDSet);
									if (const auto actorEffects = npc->GetSpellList()) {
										actorEffects->AddLevSpells(spells);
									}
								}
							}
							break;
						case RE::FormType::Shout:
							{
								if (!a_isBelowLevel) {
									const auto shouts = detail::set_to_vec<RE::TESShout>(a_formIDSet);
									if (const auto actorEffects = npc->GetSpellList()) {
										actorEffects->AddShouts(shouts);
									}
								}
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

		static inline size_t index{ 0 };
		static inline size_t size{ 0xF };
	};

	void Install()
	{
		// ProcessLists::HandlePlayerLevelUpdate
		// inlined into SetLevel in AE
		REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(40575, 41567), OFFSET(0x97, 0x137) };
		stl::write_thunk_call<HandleUpdatePlayerLevel>(target.address());

		stl::write_vfunc<RE::Character, Revert>();
		stl::write_vfunc<RE::Character, LoadGame>();

		logger::info("\tInstalled leveled distribution hooks");
	}
}
