#include "DistributePCLevelMult.h"
#include "Distribute.h"
#include "PCLevelMultManager.h"

namespace Distribute::PlayerLeveledActor
{
	struct HandleUpdatePlayerLevel
	{
		static void thunk(RE::Actor* a_actor)
		{
			if (const auto npc = a_actor->GetActorBase()) {
				auto npcData = std::make_unique<NPCData>(a_actor, npc);
				Distribute(*npcData, PCLevelMult::Input{ a_actor, npc, true, false });
			}

			func(a_actor);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct LoadGame
	{
		static void thunk(RE::Character* a_this, std::uintptr_t a_buf)
		{
			if (const auto npc = a_this->GetActorBase(); npc && npc->HasPCLevelMult()) {
				const auto input = PCLevelMult::Input{ a_this, npc, true, false };

				if (const auto pcLevelMultManager = PCLevelMult::Manager::GetSingleton(); !pcLevelMultManager->FindDistributedEntry(input)) {
					//start distribution for first time
					const auto npcData = std::make_unique<NPCData>(a_this, npc);
					Distribute(*npcData, input);
				} else {
					//handle redistribution and removal
					pcLevelMultManager->ForEachDistributedEntry(input, [&](RE::FormType a_formType, const Set<RE::FormID>& a_formIDSet, bool a_isBelowLevel) {
						switch (a_formType) {
						case RE::FormType::Keyword:
							{
                                const auto keywords = detail::set_to_vec<RE::BGSKeyword>(a_formIDSet);

								if (a_isBelowLevel) {
									npc->RemoveKeywords(keywords);
								} else {
									npc->AddKeywords(keywords);
								}
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
										if (a_isBelowLevel) {
											(*it).rank = -1;
										} else {
											(*it).rank = 1;
										}
									}
								}
							}
							break;
						case RE::FormType::Perk:
							{
								const auto perks = detail::set_to_vec<RE::BGSPerk>(a_formIDSet);
		
								if (a_isBelowLevel) {
									npc->RemovePerks(perks);
								} else {
									npc->AddPerks(perks, 1);
								}
							}
							break;
						case RE::FormType::Spell:
							{
								const auto spells = detail::set_to_vec<RE::SpellItem>(a_formIDSet);

								if (const auto actorEffects = npc->GetSpellList()) {
									if (a_isBelowLevel) {
										actorEffects->RemoveSpells(spells);
									} else {
										actorEffects->AddSpells(spells);
									}
								}
							}
							break;
						case RE::FormType::LeveledSpell:
							{
								const auto spells = detail::set_to_vec<RE::TESLevSpell>(a_formIDSet);

								if (const auto actorEffects = npc->GetSpellList()) {
									if (a_isBelowLevel) {
										actorEffects->RemoveLevSpells(spells);
									} else {
										actorEffects->AddLevSpells(spells);
									}
								}
							}
							break;
						case RE::FormType::Shout:
							{
								const auto shouts = detail::set_to_vec<RE::TESShout>(a_formIDSet);

								if (const auto actorEffects = npc->GetSpellList()) {
									if (a_isBelowLevel) {
										actorEffects->RemoveShouts(shouts);
									} else {
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

		stl::write_vfunc<RE::Character, LoadGame>();
		logger::info("	Hooked npc load save");
	}
}
