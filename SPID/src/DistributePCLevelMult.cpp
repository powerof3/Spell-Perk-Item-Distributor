#include "DistributePCLevelMult.h"
#include "Distribute.h"
#include "PCLevelMultManager.h"

namespace Distribute::PlayerLeveledActor
{
    struct HandleUpdatePlayerLevel
    {
        static void thunk(RE::Actor* a_actor)
        {
            if (const auto npc = a_actor->GetActorBase(); npc && npc->HasPCLevelMult()) {
                const auto input = npc->IsDynamicForm() ? PCLevelMult::Input{ a_actor, npc, true, false } : // use character formID for permanent storage
                                       PCLevelMult::Input{ npc, true, false };
                Distribute(npc, input);
            }

            func(a_actor);
        }
        static inline REL::Relocation<decltype(thunk)> func;
    };

    struct LoadGame
    {
        static void thunk(RE::Character* a_this, std::uintptr_t a_buf)
        {
            if (const auto actorbase = a_this->GetActorBase(); actorbase && actorbase->HasPCLevelMult()) {
                const auto input = actorbase->IsDynamicForm() ? PCLevelMult::Input{ a_this, actorbase, true, false } : // use character formID for permanent storage
                                       PCLevelMult::Input{ actorbase, true, false };

                if (const auto pcLevelMultManager = PCLevelMult::Manager::GetSingleton(); !pcLevelMultManager->FindDistributedEntry(input)) {
                    //start distribution for first time
                    Distribute(actorbase, input);
                } else {
                    //handle redistribution and removal
                    pcLevelMultManager->ForEachDistributedEntry(input, [&](RE::TESForm& a_form, [[maybe_unused]] IdxOrCount a_count, bool a_isBelowLevel) {
                        switch (a_form.GetFormType()) {
                        case RE::FormType::Keyword:
                        {
                            auto keyword = a_form.As<RE::BGSKeyword>();
                            if (a_isBelowLevel) {
                                actorbase->RemoveKeyword(keyword);
                            } else {
                                actorbase->AddKeyword(keyword);
                            }
                        }
                        break;
                        case RE::FormType::Faction:
                        {
                            auto faction = a_form.As<RE::TESFaction>();
                            auto it = std::ranges::find_if(actorbase->factions, [&](const auto& factionRank) {
                                return factionRank.faction == faction;
                            });
                            if (it != actorbase->factions.end()) {
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
                            auto perk = a_form.As<RE::BGSPerk>();
                            if (a_isBelowLevel) {
                                actorbase->RemovePerk(perk);
                            } else {
                                actorbase->AddPerk(perk, 1);
                            }
                        }
                        break;
                        case RE::FormType::Spell:
                        {
                            auto spell = a_form.As<RE::SpellItem>();
                            if (auto actorEffects = actorbase->GetSpellList()) {
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
                            auto spell = a_form.As<RE::TESLevSpell>();
                            if (auto actorEffects = actorbase->GetSpellList()) {
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
                            auto shout = a_form.As<RE::TESShout>();
                            if (auto actorEffects = actorbase->GetSpellList()) {
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
                                auto boundObject = static_cast<RE::TESBoundObject*>(&a_form);
                                if (a_isBelowLevel) {
                                    actorbase->RemoveObjectFromContainer(boundObject, a_count);
                                } else if (actorbase->CountObjectsInContainer(boundObject) < a_count) {
                                    actorbase->AddObjectToContainer(boundObject, a_count, a_this);
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
        // ProcessLists::HandleUpdate
        // inlined into SetLevel in AE
        REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(40575, 41567), OFFSET(0x86, 0x137) };
        stl::write_thunk_call<HandleUpdatePlayerLevel>(target.address());

        stl::write_vfunc<RE::Character, LoadGame>();
        logger::info("	Hooked npc load save");
    }
}
