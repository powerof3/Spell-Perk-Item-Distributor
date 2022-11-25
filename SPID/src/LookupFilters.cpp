#include "LookupFilters.h"
#include "LookupNPC.h"

namespace Filter
{
	Result Data::passed_string_filters(const NPCData& a_npcData) const
	{
		if (!strings.ALL.empty() && !a_npcData.HasStringFilter(strings.ALL, true)) {
			return Result::kFail;
		}

		if (!strings.NOT.empty() && a_npcData.HasStringFilter(strings.NOT)) {
			return Result::kFail;
		}

		if (!strings.MATCH.empty() && !a_npcData.HasStringFilter(strings.MATCH)) {
			return Result::kFail;
		}

		if (!strings.ANY.empty() && !a_npcData.ContainsStringFilter(strings.ANY)) {
			return Result::kFail;
		}

		return Result::kPass;
	}

	Result Data::passed_form_filters(const NPCData& a_npcData) const
	{
		if (!forms.ALL.empty() && !a_npcData.HasFormFilter(forms.ALL, true)) {
			return Result::kFail;
		}

		if (!forms.NOT.empty() && a_npcData.HasFormFilter(forms.NOT)) {
			return Result::kFail;
		}

		if (!forms.MATCH.empty() && !a_npcData.HasFormFilter(forms.MATCH)) {
			return Result::kFail;
		}

		return Result::kPass;
	}

	Result Data::passed_secondary_filters(const NPCData& a_npcData) const
	{
		// Actor Level
		auto& [actorMin, actorMax] = std::get<0>(level);
		const auto actorLevel = a_npcData.GetLevel();

		if (actorMin < UINT16_MAX && actorMax < UINT16_MAX) {
			if (actorLevel < actorMin || actorLevel > actorMax) {
				return Result::kFail;
			}
		} else if (actorMin < UINT16_MAX && actorLevel < actorMin) {
			return Result::kFail;
		} else if (actorMax < UINT16_MAX && actorLevel > actorMax) {
			return Result::kFail;
		}

		const auto npc = a_npcData.GetNPC();

		// Skill Level
		for (auto& [skillType, skill] : std::get<1>(level)) {
			auto& [skillMin, skillMax] = skill;

			const auto skillLevel = npc->playerSkills.values[skillType];

			if (skillMin < UINT8_MAX && skillMax < UINT8_MAX) {
				if (skillLevel < skillMin || skillLevel > skillMax) {
					return Result::kFail;
				}
			} else if (skillMin < UINT8_MAX && skillLevel < skillMin) {
				return Result::kFail;
			} else if (skillMax < UINT8_MAX && skillLevel > skillMax) {
				return Result::kFail;
			}
		}

		const auto& skillWeights = npc->npcClass->data.skillWeights;

		// Skill Weight
		for (auto& [skillType, skill] : std::get<2>(level)) {
			auto& [skillMin, skillMax] = skill;

			std::uint8_t skillWeight = skillWeights.oneHanded;
			using Skill = RE::TESNPC::Skills;
			switch (skillType) {
			case Skill::kOneHanded:
				skillWeight = skillWeights.oneHanded;
				break;
			case Skill::kTwoHanded:
				skillWeight = skillWeights.twoHanded;
				break;
			case Skill::kMarksman:
				skillWeight = skillWeights.archery;
				break;
			case Skill::kBlock:
				skillWeight = skillWeights.block;
				break;
			case Skill::kSmithing:
				skillWeight = skillWeights.smithing;
				break;
			case Skill::kHeavyArmor:
				skillWeight = skillWeights.heavyArmor;
				break;
			case Skill::kLightArmor:
				skillWeight = skillWeights.lightArmor;
				break;
			case Skill::kPickpocket:
				skillWeight = skillWeights.pickpocket;
				break;
			case Skill::kLockpicking:
				skillWeight = skillWeights.lockpicking;
				break;
			case Skill::kSneak:
				skillWeight = skillWeights.sneak;
				break;
			case Skill::kAlchemy:
				skillWeight = skillWeights.alchemy;
				break;
			case Skill::kSpecchcraft:
				skillWeight = skillWeights.speech;
				break;
			case Skill::kAlteration:
				skillWeight = skillWeights.alteration;
				break;
			case Skill::kConjuration:
				skillWeight = skillWeights.conjuration;
				break;
			case Skill::kDestruction:
				skillWeight = skillWeights.destruction;
				break;
			case Skill::kIllusion:
				skillWeight = skillWeights.illusion;
				break;
			case Skill::kRestoration:
				skillWeight = skillWeights.restoration;
				break;
			case Skill::kEnchanting:
				skillWeight = skillWeights.enchanting;
				break;
			default:
				continue;
			}

			if (skillMin < UINT8_MAX && skillMax < UINT8_MAX) {
				if (skillWeight < skillMin || skillWeight > skillMax) {
					return Result::kFail;
				}
			} else if (skillMin < UINT8_MAX && skillWeight < skillMin) {
				return Result::kFail;
			} else if (skillMax < UINT8_MAX && skillWeight > skillMax) {
				return Result::kFail;
			}
		}

		if (traits.sex && a_npcData.GetSex() != *traits.sex) {
			return Result::kFail;
		}
		if (traits.unique && a_npcData.IsUnique() != *traits.unique) {
			return Result::kFail;
		}
		if (traits.summonable && a_npcData.IsSummonable() != *traits.summonable) {
			return Result::kFail;
		}
		if (traits.child && a_npcData.IsChild() != *traits.child) {
			return Result::kFail;
		}

		if (chance != 100) {
			if (const auto rng = staticRNG.Generate<Chance>(0, 100); rng > chance) {
				return Result::kFailRNG;
			}
		}

		return Result::kPass;
	}

	bool Data::HasLevelFilters() const
	{
		const auto& [actorLevelPair, skillLevelPairs, _] = level;

		auto& [actorMin, actorMax] = actorLevelPair;
		if (actorMin < UINT16_MAX || actorMax < UINT16_MAX) {
			return true;
		}

		return std::ranges::any_of(skillLevelPairs, [](const auto& skillPair) {
			auto& [skillType, skill] = skillPair;
			auto& [skillMin, skillMax] = skill;

			return skillMin < UINT8_MAX || skillMax < UINT8_MAX;
		});
	}

	Result Data::PassedFilters(const NPCData& a_npcData, bool a_noPlayerLevelDistribution) const
	{
		if (passed_string_filters(a_npcData) == Result::kFail) {
			return Result::kFail;
		}

		if (passed_form_filters(a_npcData) == Result::kFail) {
			return Result::kFail;
		}

		const auto npc = a_npcData.GetNPC();

		if (a_noPlayerLevelDistribution && HasLevelFilters() && npc->HasPCLevelMult()) {
			return Result::kFail;
		}

		return passed_secondary_filters(a_npcData);
	}
}
