#include "LookupFilters.h"
#include "LookupForms.h"

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

	Result passed_string_filters(RE::TESNPC& a_actorbase, const StringFilters& a_stringFilters)
	{
		if (!a_stringFilters.ALL.empty() && !detail::keyword::matches(a_actorbase, a_stringFilters.ALL, true)) {
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

		if (!a_stringFilters.NOT.empty()) {
			bool result = false;
			if (!name.empty() && matches(name, a_stringFilters.NOT)) {
				result = true;
			}
			if (!result && !editorID.empty() && matches(editorID, a_stringFilters.NOT)) {
				result = true;
			}
			if (!result && detail::keyword::matches(a_actorbase, a_stringFilters.NOT)) {
				result = true;
			}
			if (result) {
				return Result::kFail;
			}
		}

		if (!a_stringFilters.MATCH.empty()) {
			bool result = false;
			if (!name.empty() && matches(name, a_stringFilters.MATCH)) {
				result = true;
			}
			if (!result && !editorID.empty() && matches(editorID, a_stringFilters.MATCH)) {
				result = true;
			}
			if (!result && detail::keyword::matches(a_actorbase, a_stringFilters.MATCH)) {
				result = true;
			}
			if (!result) {
				return Result::kFail;
			}
		}

		if (!a_stringFilters.ANY.empty()) {
			bool result = false;
			if (!name.empty() && contains(name, a_stringFilters.ANY)) {
				result = true;
			}
			if (!result && !editorID.empty() && contains(editorID, a_stringFilters.ANY)) {
				result = true;
			}
			if (!result && detail::keyword::contains(a_actorbase, a_stringFilters.ANY)) {
				result = true;
			}
			if (!result) {
				return Result::kFail;
			}
		}

		return Result::kPass;
	}

	Result passed_form_filters(RE::TESNPC& a_actorbase, const FormFilters& a_formFilters)
	{
		if (!a_formFilters.ALL.empty() && !detail::form::matches(a_actorbase, a_formFilters.ALL, true)) {
			return Result::kFail;
		}

		if (!a_formFilters.NOT.empty() && detail::form::matches(a_actorbase, a_formFilters.NOT)) {
			return Result::kFail;
		}

		if (!a_formFilters.MATCH.empty() && !detail::form::matches(a_actorbase, a_formFilters.MATCH)) {
			return Result::kFail;
		}

		return Result::kPass;
	}

	Result passed_secondary_filters(RE::TESNPC& a_actorbase, const LevelFilters& a_levelFilters, const Traits& a_traits, float a_chance, bool a_noPlayerLevelDistribution)
	{
		if (Lookup::detail::has_level_filters(a_levelFilters)) {
			if (a_noPlayerLevelDistribution && a_actorbase.HasPCLevelMult()) {
				return Result::kFail;
			}
		}

		auto& [actorMin, actorMax] = a_levelFilters.first;
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

		for (auto& [skillType, skill] : a_levelFilters.second) {
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

		if (a_traits.sex && a_actorbase.GetSex() != *a_traits.sex) {
			return Result::kFail;
		}
		if (a_traits.unique && a_actorbase.IsUnique() != *a_traits.unique) {
			return Result::kFail;
		}
		if (a_traits.summonable && a_actorbase.IsSummonable() != *a_traits.summonable) {
			return Result::kFail;
		}
		if (a_traits.child && (a_actorbase.race && a_actorbase.race->IsChildRace()) != *a_traits.child) {
			return Result::kFail;
		}

		if (!numeric::essentially_equal(a_chance, 100.0f)) {
			if (const auto rng = staticRNG.Generate<float>(0.0f, 100.0f); rng > a_chance) {
				return Result::kFailRNG;
			}
		}

		return Result::kPass;
	}

	Result PassedFilters(RE::TESNPC& a_actorbase, const FilterData& a_filters, bool a_noPlayerLevelDistribution)
	{
		if (passed_string_filters(a_actorbase, a_filters.strings) == Result::kFail) {
			return Result::kFail;
		}
		if (passed_form_filters(a_actorbase, a_filters.forms) == Result::kFail) {
			return Result::kFail;
		}
		return passed_secondary_filters(a_actorbase, a_filters.level, a_filters.traits, a_filters.chance, a_noPlayerLevelDistribution);
	}
}
