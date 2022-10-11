#include "DistributePCLevelMult.h"
#include "Distribute.h"
#include "PCLevelMultManager.h"

namespace Distribute
{
	namespace PlayerLeveledActor
	{
		struct UpdateAutoCalcNPCs
		{
			static void thunk(RE::TESDataHandler* a_dataHandler)
			{
				func(a_dataHandler);

				ApplyToPCLevelMultNPCs(a_dataHandler);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct LoadGame
		{
			static void thunk(RE::TESNPC* a_this, std::uintptr_t a_buf)
			{
				func(a_this, a_buf);

				if (!a_this->HasPCLevelMult()) {
					return;
				}

				const PCLevelMult::Input input{
					a_this,
					true,
					false
				};

				const auto pcLevelMultManager = PCLevelMult::Manager::GetSingleton();

				if (!pcLevelMultManager->FindDistributedEntry(input)) {  //start distribution for first time
					Distribute(a_this, true, false);
				} else {
					pcLevelMultManager->ForEachDistributedEntry(input, [&](RE::TESForm& a_form, [[maybe_unused]] IdxOrCount a_count, bool a_isBelowLevel) {  //handle redistribution and removal
						switch (a_form.GetFormType()) {
						case RE::FormType::Keyword:
							{
								auto keyword = a_form.As<RE::BGSKeyword>();
								if (a_isBelowLevel) {
									a_this->RemoveKeyword(keyword);
								} else {
									a_this->AddKeyword(keyword);
								}
							}
							break;
						case RE::FormType::Faction:
							{
								auto faction = a_form.As<RE::TESFaction>();
								auto it = std::ranges::find_if(a_this->factions, [&](const auto& factionRank) {
									return factionRank.faction == faction;
								});
								if (it != a_this->factions.end()) {
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
									a_this->RemovePerk(perk);
								} else {
									a_this->AddPerk(perk, 1);
								}
							}
							break;
						case RE::FormType::Spell:
							{
								auto spell = a_form.As<RE::SpellItem>();
								if (auto actorEffects = a_this->GetSpellList()) {
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
								if (auto actorEffects = a_this->GetSpellList()) {
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
								if (auto actorEffects = a_this->GetSpellList()) {
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
										a_this->RemoveObjectFromContainer(boundObject, a_count);
									} else if (a_this->CountObjectsInContainer(boundObject) < a_count) {
										a_this->AddObjectToContainer(boundObject, a_count, a_this);
									}
								}
							}
							break;
						}
					});
				}
			}
			static inline REL::Relocation<decltype(thunk)> func;

			static inline size_t index{ 0 };
			static inline size_t size{ 0xF };
		};

		void Install()
		{
			REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(40560, 41567), OFFSET(0xB2, 0xC2) };  // SetLevel
			stl::write_thunk_call<UpdateAutoCalcNPCs>(target.address());

			stl::write_vfunc<RE::TESNPC, LoadGame>();
			logger::info("	Hooked npc load save");
		}
	}

	void ApplyToPCLevelMultNPCs(RE::TESDataHandler* a_dataHandler)
	{
		for (const auto& actorbase : a_dataHandler->GetFormArray<RE::TESNPC>()) {
			if (actorbase && actorbase->HasPCLevelMult()) {
				Distribute(actorbase, true, false);
			}
		}
	}
}
