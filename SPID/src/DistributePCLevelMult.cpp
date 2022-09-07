#include "DistributePCLevelMult.h"
#include "Distribute.h"

namespace Distribute::PlayerLeveledActor
{
	struct AutoCalcSkillsAttributes
	{
		static void thunk(RE::TESNPC* a_actorbase)
		{
			func(a_actorbase);

			if (!a_actorbase->IsPlayer() && a_actorbase->HasPCLevelMult()) {
				Distribute(a_actorbase, true, false);
			}
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct LoadGame
	{
		static void thunk(RE::Character* a_this, std::uintptr_t a_buf)
		{
			func(a_this, a_buf);

			const auto actorbase = a_this->GetActorBase();
			if (!actorbase || !actorbase->HasPCLevelMult()) {
				return;
			}

			const PCLevelMult::Input input{
				RE::BGSSaveLoadManager::GetSingleton()->currentPlayerID,
				actorbase->GetFormID(),
				actorbase->GetLevel(),
				true,
				false
			};

			pcLevelMultManager.for_each_distributed_entry(input, [&](RE::TESForm& a_form, [[maybe_unused]] IdxOrCount a_idx, bool a_isBelowLevel) {
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
				}
			});
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static inline size_t index{ 0 };
		static inline size_t size{ 0x0F };
	};

	void Install()
	{
		REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(24219, 24723), 0x2D };
		stl::write_thunk_call<AutoCalcSkillsAttributes>(target.address());

		stl::write_vfunc<RE::Character, LoadGame>();
		logger::info("	Hooked leveled actor init");
	}
}
