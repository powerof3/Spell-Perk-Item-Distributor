#include "LookupFilters.h"
#include "LookupNPC.h"

namespace Filter
{
	Data::Data(StringFilters a_strings, FormFilters a_formFilters, LevelFilters a_level, Traits a_traits, Chance a_chance) :
		strings(std::move(a_strings)),
		forms(std::move(a_formFilters)),
		levels(std::move(a_level)),
		traits(a_traits),
		chance(a_chance)
	{
		hasLeveledFilters = HasLevelFiltersImpl();
	}

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

	Result Data::passed_level_filters(const NPC::Data& a_npcData) const
	{
		// Actor Level
		if (!levels.actorLevel.IsInRange(a_npcData.GetLevel())) {
			return Result::kFail;
		}

		const auto npc = a_npcData.GetNPC();

		// Skill Level
		for (auto& [skillType, skillRange] : levels.skillLevels) {
			if (!skillRange.IsInRange(npc->playerSkills.values[skillType])) {
				return Result::kFail;
			}
		}

		if (const auto npcClass = npc->npcClass) {
			const auto& skillWeights = npcClass->data.skillWeights;

			// Skill Weight
			for (auto& [skillType, skillRange] : levels.skillWeights) {
				std::uint8_t skillWeight;

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
				case Skill::kSpeechcraft:
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

				if (!skillRange.IsInRange(skillWeight)) {
					return Result::kFail;
				}
			}
		}

		return Result::kPass;
	}

	Result Data::passed_trait_filters(const NPCData& a_npcData) const
	{
		// Traits
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
		if (traits.leveled && a_npcData.IsLeveled() != *traits.leveled) {
			return Result::kFail;
		}
		if (traits.teammate && a_npcData.IsTeammate() != *traits.teammate) {
			return Result::kFail;
		}

		return Result::kPass;
	}

	bool Data::HasLevelFilters() const
	{
		return hasLeveledFilters;
	}

	bool Data::HasLevelFiltersImpl() const
	{
		const auto& [actorLevel, skillLevels, _] = levels;

		if (actorLevel.IsValid()) {
			return true;
		}

		return std::ranges::any_of(skillLevels, [](const auto& skillPair) {
			auto& [skillType, skillRange] = skillPair;
			return skillRange.IsValid();
		});
	}

	Result Data::PassedFilters(const NPCData& a_npcData, const RE::TESForm* a_distributeForm) const
	{
		// Fail chance first to avoid running unnecessary checks
		if (chance < 100) {
			// create unique seed based on actor formID and item formID/edid
			const auto seed = hash::szudzik_pair(
				a_npcData.GetActor()->GetFormID(), a_distributeForm->IsDynamicForm() ?
                                                       hash::fnv1a_32<std::string_view>(a_distributeForm->GetFormEditorID()) :
                                                       a_distributeForm->GetFormID());

			const auto randNum = RNG(seed).Generate<Chance>(0, 100);
			if (randNum > chance) {
				return Result::kFailRNG;
			}
		}

		if (passed_string_filters(a_npcData) == Result::kFail) {
			return Result::kFail;
		}

		if (passed_form_filters(a_npcData) == Result::kFail) {
			return Result::kFail;
		}

		if (passed_level_filters(a_npcData) == Result::kFail) {
			return Result::kFail;
		}

		return passed_trait_filters(a_npcData);
	}
}
