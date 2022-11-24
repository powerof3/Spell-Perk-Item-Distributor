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
				Distribute(NPCData{ a_actor, npc }, PCLevelMult::Input{ a_actor, npc, true, false });
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
				const auto input = npc->IsDynamicForm() ? PCLevelMult::Input{ a_this, npc, true, false } :  // use character formID for permanent storage
				                                          PCLevelMult::Input{ npc, true, false };

				if (const auto pcLevelMultManager = PCLevelMult::Manager::GetSingleton(); !pcLevelMultManager->FindDistributedEntry(input)) {
					//start distribution for first time
					Distribute(NPCData{ a_this, npc }, input);
				} else {
					//handle redistribution and removal
					pcLevelMultManager->ForEachDistributedEntry(input, [&](RE::TESForm& a_form, [[maybe_unused]] IdxOrCount a_count, bool a_isBelowLevel) {
						switch (a_form.GetFormType()) {
						case RE::FormType::Keyword:
							{
                                const auto keyword = a_form.As<RE::BGSKeyword>();
								if (a_isBelowLevel) {
									npc->RemoveKeyword(keyword);
								} else {
									npc->AddKeyword(keyword);
								}
							}
							break;
						case RE::FormType::Faction:
							{
								auto faction = a_form.As<RE::TESFaction>();
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
							break;
						case RE::FormType::Perk:
							{
                                const auto perk = a_form.As<RE::BGSPerk>();
								if (a_isBelowLevel) {
									npc->RemovePerk(perk);
								} else {
									npc->AddPerk(perk, 1);
								}
							}
							break;
						case RE::FormType::Spell:
							{
                                const auto spell = a_form.As<RE::SpellItem>();
								if (const auto actorEffects = npc->GetSpellList()) {
									if (a_isBelowLevel) {
										actorEffects->RemoveSpell(spell);
									} else if (!actorEffects->GetIndex(spell)) {
										actorEffects->AddSpell(spell);
									}
								}
							}
							break;
						case RE::FormType::LeveledSpell:
							{
                                const auto spell = a_form.As<RE::TESLevSpell>();
								if (const auto actorEffects = npc->GetSpellList()) {
									if (a_isBelowLevel) {
										actorEffects->RemoveLevSpell(spell);
									} else {
										actorEffects->AddLevSpell(spell);
									}
								}
							}
							break;
						case RE::FormType::Shout:
							{
                                const auto shout = a_form.As<RE::TESShout>();
								if (const auto actorEffects = npc->GetSpellList()) {
									if (a_isBelowLevel) {
										actorEffects->RemoveShout(shout);
									} else {
										actorEffects->AddShout(shout);
									}
								}
							}
							break;
						default:
							{
								if (a_form.IsInventoryObject()) {
                                    const auto boundObject = static_cast<RE::TESBoundObject*>(&a_form);
									if (a_isBelowLevel) {
										npc->RemoveObjectFromContainer(boundObject, a_count);
									} else if (npc->CountObjectsInContainer(boundObject) < a_count) {
										npc->AddObjectToContainer(boundObject, a_count, a_this);
									}
								}
							}
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
