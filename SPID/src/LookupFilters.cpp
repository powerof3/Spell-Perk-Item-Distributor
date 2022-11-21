#include "LookupFilters.h"

namespace Filter
{
	namespace detail
	{
		bool form::get_type(RE::TESNPC& a_actorbase, RE::TESForm* a_filter)
		{
			switch (a_filter->GetFormType()) {
			case RE::FormType::CombatStyle:
				return a_actorbase.GetCombatStyle() == a_filter;
			case RE::FormType::Class:
				return a_actorbase.npcClass == a_filter;
			case RE::FormType::Faction:
				{
					const auto faction = a_filter->As<RE::TESFaction>();
					return a_actorbase.IsInFaction(faction);
				}
			case RE::FormType::Race:
				return a_actorbase.GetRace() == a_filter;
			case RE::FormType::Outfit:
				return a_actorbase.defaultOutfit == a_filter;
			case RE::FormType::NPC:
				return &a_actorbase == a_filter;
			case RE::FormType::VoiceType:
				return a_actorbase.voiceType == a_filter;
			case RE::FormType::FormList:
				{
					bool result = false;

					auto list = a_filter->As<RE::BGSListForm>();
					list->ForEachForm([&](RE::TESForm& a_form) {
						if (result = get_type(a_actorbase, &a_form); result) {
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

		bool form::matches(RE::TESNPC& a_actorbase, const FormVec& a_forms, bool a_matchesAll)
		{
			const auto has_form_or_file = [&](const std::variant<RE::TESForm*, const RE::TESFile*>& a_formFile) {
				if (std::holds_alternative<RE::TESForm*>(a_formFile)) {
					auto form = std::get<RE::TESForm*>(a_formFile);
					return form && get_type(a_actorbase, form);
				}
				if (std::holds_alternative<const RE::TESFile*>(a_formFile)) {
					auto file = std::get<const RE::TESFile*>(a_formFile);
					return file && file->IsFormInMod(a_actorbase.GetFormID());
				}
				return false;
			};

			if (a_matchesAll) {
				return std::ranges::all_of(a_forms, has_form_or_file);
			} else {
				return std::ranges::any_of(a_forms, has_form_or_file);
			}
		}

		bool keyword::contains(RE::TESNPC& a_actorbase, const StringVec& a_strings)
		{
			return std::ranges::any_of(a_strings, [&a_actorbase](const auto& str) {
				return a_actorbase.ContainsKeyword(str);
			});
		}

		bool keyword::matches(RE::TESNPC& a_actorbase, const StringVec& a_strings, bool a_matchesAll)
		{
			const auto has_keyword = [&](const auto& str) {
				return a_actorbase.HasApplicableKeywordString(str);
			};

			if (a_matchesAll) {
				return std::ranges::all_of(a_strings, has_keyword);
			} else {
				return std::ranges::any_of(a_strings, has_keyword);
			}
		}
	}

	Result Data::passed_string_filters(RE::TESNPC& a_actorbase) const
	{
		if (!strings.ALL.empty() && !detail::keyword::matches(a_actorbase, strings.ALL, true)) {
			return Result::kFail;
		}

		const std::string name = a_actorbase.GetName();
		const std::string editorID = Cache::EditorID::GetEditorID(a_actorbase.GetFormID());

		constexpr auto matches = [](const std::string& a_name, const StringVec& a_strings) {
			return std::ranges::any_of(a_strings, [&](const auto& str) {
				return string::icontains(a_name, str);
			});
		};

		constexpr auto contains = [](const std::string& a_name, const StringVec& a_strings) {
			return std::ranges::any_of(a_strings, [&](const auto& str) {
				return string::icontains(a_name, str);
			});
		};

		if (!strings.NOT.empty()) {
			bool result = false;
			if (!name.empty() && matches(name, strings.NOT)) {
				result = true;
			}
			if (!result && !editorID.empty() && matches(editorID, strings.NOT)) {
				result = true;
			}
			if (!result && detail::keyword::matches(a_actorbase, strings.NOT)) {
				result = true;
			}
			if (result) {
				return Result::kFail;
			}
		}

		if (!strings.MATCH.empty()) {
			bool result = false;
			if (!name.empty() && matches(name, strings.MATCH)) {
				result = true;
			}
			if (!result && !editorID.empty() && matches(editorID, strings.MATCH)) {
				result = true;
			}
			if (!result && detail::keyword::matches(a_actorbase, strings.MATCH)) {
				result = true;
			}
			if (!result) {
				return Result::kFail;
			}
		}

		if (!strings.ANY.empty()) {
			bool result = false;
			if (!name.empty() && contains(name, strings.ANY)) {
				result = true;
			}
			if (!result && !editorID.empty() && contains(editorID, strings.ANY)) {
				result = true;
			}
			if (!result && detail::keyword::contains(a_actorbase, strings.ANY)) {
				result = true;
			}
			if (!result) {
				return Result::kFail;
			}
		}

		return Result::kPass;
	}

	Result Data::passed_form_filters(RE::TESNPC& a_actorbase) const
	{
		if (!forms.ALL.empty() && !detail::form::matches(a_actorbase, forms.ALL, true)) {
			return Result::kFail;
		}

		if (!forms.NOT.empty() && detail::form::matches(a_actorbase, forms.NOT)) {
			return Result::kFail;
		}

		if (!forms.MATCH.empty() && !detail::form::matches(a_actorbase, forms.MATCH)) {
			return Result::kFail;
		}

		return Result::kPass;
	}

	Result Data::passed_secondary_filters(RE::TESNPC& a_actorbase) const
	{
		auto& [actorMin, actorMax] = level.first;
		const auto actorLevel = a_actorbase.GetLevel();

		if (actorMin < UINT16_MAX && actorMax < UINT16_MAX) {
			if (actorLevel < actorMin || actorLevel > actorMax) {
				return Result::kFail;
			}
		} else if (actorMin < UINT16_MAX && actorLevel < actorMin) {
			return Result::kFail;
		} else if (actorMax < UINT16_MAX && actorLevel > actorMax) {
			return Result::kFail;
		}

		for (auto& [skillType, skill] : level.second) {
			auto& [skillMin, skillMax] = skill;

			if (skillType < 18) {
				const auto skillLevel = a_actorbase.playerSkills.values[skillType];

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
		}

		if (traits.sex && a_actorbase.GetSex() != *traits.sex) {
			return Result::kFail;
		}
		if (traits.unique && a_actorbase.IsUnique() != *traits.unique) {
			return Result::kFail;
		}
		if (traits.summonable && a_actorbase.IsSummonable() != *traits.summonable) {
			return Result::kFail;
		}
		if (traits.child && (a_actorbase.race && a_actorbase.race->IsChildRace()) != *traits.child) {
			return Result::kFail;
		}

		if (!numeric::essentially_equal(chance, 100.0f)) {
			if (const auto rng = staticRNG.Generate<float>(0.0f, 100.0f); rng > chance) {
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

			return skillType < 18 && (skillMin < UINT8_MAX || skillMax < UINT8_MAX);
		});
	}

	Result Data::PassedFilters(RE::TESNPC& a_actorbase, bool a_noPlayerLevelDistribution) const
	{
		if (passed_string_filters(a_actorbase) == Result::kFail) {
			return Result::kFail;
		}

		if (passed_form_filters(a_actorbase) == Result::kFail) {
			return Result::kFail;
		}

		if (HasLevelFilters()) {
			if (a_noPlayerLevelDistribution && a_actorbase.HasPCLevelMult()) {
				return Result::kFail;
			}
		}

		return passed_secondary_filters(a_actorbase);
	}
}
