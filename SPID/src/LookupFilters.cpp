#include "LookupFilters.h"
#include "LookupNPC.h"

namespace Filter
{
	namespace detail
	{
		bool name::contains(const std::string& a_name, const StringVec& a_strings)
		{
			return std::ranges::any_of(a_strings, [&](const auto& str) {
				return string::icontains(a_name, str);
			});
		}

		bool name::matches(const std::string& a_name, const StringVec& a_strings)
		{
			return std::ranges::any_of(a_strings, [&](const auto& str) {
				return string::iequals(a_name, str);
			});
		}

		bool keyword::contains(RE::TESNPC* a_npc, const StringVec& a_strings)
		{
			return std::ranges::any_of(a_strings, [&a_npc](const auto& str) {
				return a_npc->ContainsKeyword(str);
			});
		}

		bool keyword::matches(RE::TESNPC* a_npc, const StringVec& a_strings, bool a_matchesAll)
		{
			const auto has_keyword = [&](const auto& str) {
				return a_npc->HasApplicableKeywordString(str);
			};

			if (a_matchesAll) {
				return std::ranges::all_of(a_strings, has_keyword);
			} else {
				return std::ranges::any_of(a_strings, has_keyword);
			}
		}

		bool form::get_type(RE::TESNPC* a_npc, RE::TESForm* a_filter)
		{
			switch (a_filter->GetFormType()) {
			case RE::FormType::CombatStyle:
				return a_npc->GetCombatStyle() == a_filter;
			case RE::FormType::Class:
				return a_npc->npcClass == a_filter;
			case RE::FormType::Faction:
				{
					const auto faction = a_filter->As<RE::TESFaction>();
					return a_npc->IsInFaction(faction);
				}
			case RE::FormType::Race:
				return a_npc->GetRace() == a_filter;
			case RE::FormType::Outfit:
				return a_npc->defaultOutfit == a_filter;
			case RE::FormType::NPC:
				return a_npc == a_filter;
			case RE::FormType::VoiceType:
				return a_npc->voiceType == a_filter;
			case RE::FormType::Spell:
				{
					const auto spell = a_filter->As<RE::SpellItem>();
					return a_npc->GetSpellList()->GetIndex(spell).has_value();
				}
			case RE::FormType::FormList:
				{
					bool result = false;

					auto list = a_filter->As<RE::BGSListForm>();
					list->ForEachForm([&](RE::TESForm& a_form) {
						if (result = get_type(a_npc, &a_form); result) {
							return RE::BSContainer::ForEachResult::kStop;
						}
						return RE::BSContainer::ForEachResult::kContinue;
					});

					return result;
				}
			default:
				return false;
			}
		}

		bool form::matches(RE::TESNPC* a_npc, RE::FormID a_formID, const FormVec& a_forms, bool a_matchesAll)
		{
			constexpr auto has_form_or_file = [](const std::variant<RE::TESForm*, const RE::TESFile*>& a_formFile, RE::TESNPC* a_npc, RE::FormID a_formID) {
				if (std::holds_alternative<RE::TESForm*>(a_formFile)) {
                    const auto form = std::get<RE::TESForm*>(a_formFile);
					return form && get_type(a_npc, form);
				}
				if (std::holds_alternative<const RE::TESFile*>(a_formFile)) {
                    const auto file = std::get<const RE::TESFile*>(a_formFile);
					return file && file->IsFormInMod(a_formID);
				}
				return false;
			};

			if (a_matchesAll) {
				return std::ranges::all_of(a_forms, [&](const auto& formOrFile) { return has_form_or_file(formOrFile, a_npc, a_formID); });
			} else {
				return std::ranges::any_of(a_forms, [&](const auto& formOrFile) { return has_form_or_file(formOrFile, a_npc, a_formID); });
			}
		}
	}

	Result Data::passed_string_filters(const NPCData& a_npcData) const
	{
		const auto npc = a_npcData.GetNPC();

	    if (!strings.ALL.empty() && !detail::keyword::matches(npc, strings.ALL, true)) {
			return Result::kFail;
		}

	    const auto name = a_npcData.GetName();
		const auto& [originalEDID, templateEDID] = a_npcData.GetEditorID();

		if (!strings.NOT.empty()) {
			bool result = false;
			if (!name.empty() && detail::name::matches(name, strings.NOT)) {
				result = true;
			}
			if (!result && !originalEDID.empty() && detail::name::matches(originalEDID, strings.NOT)) {
				result = true;
			}
			if (!result && !templateEDID.empty() && detail::name::matches(templateEDID, strings.NOT)) {
				result = true;
			}
			if (!result && detail::keyword::matches(npc, strings.NOT)) {
				result = true;
			}
			if (result) {
				return Result::kFail;
			}
		}

		if (!strings.MATCH.empty()) {
			bool result = false;
			if (detail::keyword::matches(npc, strings.MATCH)) {
				result = true;
			}
			if (!result && !name.empty() && detail::name::matches(name, strings.MATCH)) {
				result = true;
			}
			if (!result && !originalEDID.empty() && detail::name::matches(originalEDID, strings.MATCH)) {
				result = true;
			}
			if (!result && !templateEDID.empty() && detail::name::matches(templateEDID, strings.MATCH)) {
				result = true;
			}
			if (!result) {
				return Result::kFail;
			}
		}

		if (!strings.ANY.empty()) {
			bool result = false;
			if (!name.empty() && detail::name::contains(name, strings.ANY)) {
				result = true;
			}
			if (!result && !originalEDID.empty() && detail::name::contains(originalEDID, strings.ANY)) {
				result = true;
			}
			if (!result && !templateEDID.empty() && detail::name::contains(templateEDID, strings.ANY)) {
				result = true;
			}
			if (!result && detail::keyword::contains(npc, strings.ANY)) {
				result = true;
			}
			if (!result) {
				return Result::kFail;
			}
		}

		return Result::kPass;
	}

	Result Data::passed_form_filters(const NPCData& a_npcData) const
	{
		const auto npc = a_npcData.GetNPC();
		const auto formID = a_npcData.GetFormID();

	    if (!forms.ALL.empty() && !detail::form::matches(npc, formID, forms.ALL, true)) {
			return Result::kFail;
		}

		if (!forms.NOT.empty() && detail::form::matches(npc, formID, forms.NOT)) {
			return Result::kFail;
		}

		if (!forms.MATCH.empty() && !detail::form::matches(npc, formID, forms.MATCH)) {
			return Result::kFail;
		}

		return Result::kPass;
	}

	Result Data::passed_secondary_filters(const NPCData& a_npcData) const
	{
	    auto& [actorMin, actorMax] = level.first;
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

		for (auto& [skillType, skill] : level.second) {
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

		if (traits.sex && npc->GetSex() != *traits.sex) {
			return Result::kFail;
		}
		if (traits.unique && npc->IsUnique() != *traits.unique) {
			return Result::kFail;
		}
		if (traits.summonable && npc->IsSummonable() != *traits.summonable) {
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
		const auto& [actorLevelPair, skillLevelPairs] = level;

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
