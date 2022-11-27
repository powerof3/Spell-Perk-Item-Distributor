#include "LookupFilters.h"
#include "LookupForms.h"

namespace Filter
{
	namespace detail
	{
		namespace stringF
		{
			bool contains(RE::TESNPC& a_actorbase, const std::string& a_name, const std::string& a_edid, const StringVec& a_strings)
			{
				return std::ranges::any_of(a_strings, [&](const auto& str) {
					return a_actorbase.ContainsKeyword(str) || string::icontains(a_name, str) || string::icontains(a_edid, str);
				});
			}

			bool matches(RE::TESNPC& a_actorbase, const std::string& a_name, const std::string& a_edid, const StringVec& a_strings)
			{
				return std::ranges::any_of(a_strings, [&](const auto& str) {
					return a_actorbase.HasApplicableKeywordString(str) || string::iequals(a_name, str) || string::iequals(a_edid, str);
				});
			}

			bool matches(RE::TESNPC& a_actorbase, const StringVec& a_strings)
			{
				const auto matches = [&](const auto& str) {
					return a_actorbase.HasApplicableKeywordString(str);
				};

				return std::ranges::all_of(a_strings, matches);
			}
		}

		namespace formF
		{
			bool get_type(RE::TESNPC& a_actorbase, RE::TESForm* a_filter)
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

			bool matches(RE::TESNPC& a_actorbase, const FormVec& a_forms, bool a_matchesAll = false)
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
		}
	}

	bool strings(RE::TESNPC& a_actorbase, const StringFilters& a_stringFilters)
	{
		auto& [strings_ALL, strings_NOT, strings_MATCH, strings_ANY] = a_stringFilters;

		if (!strings_ALL.empty() && !detail::stringF::matches(a_actorbase, strings_ALL)) {
			return false;
		}

		const std::string name = a_actorbase.GetName();
		const std::string editorID = Cache::EditorID::GetEditorID(a_actorbase.GetFormID());

		if (!strings_NOT.empty() && detail::stringF::matches(a_actorbase, name, editorID, strings_NOT)) {
			return false;
		}

		if (!strings_MATCH.empty() && !detail::stringF::matches(a_actorbase, name, editorID, strings_MATCH)) {
			return false;
		}

		if (!strings_ANY.empty() && !detail::stringF::contains(a_actorbase, name, editorID, strings_ANY)) {
			return false;
		}

		return true;
	}

	bool forms(RE::TESNPC& a_actorbase, const FormFilters& a_formFilters)
	{
		auto& [filterForms_ALL, filterForms_NOT, filterForms_MATCH] = a_formFilters;

		if (!filterForms_ALL.empty() && !detail::formF::matches(a_actorbase, filterForms_ALL, true)) {
			return false;
		}

		if (!filterForms_NOT.empty() && detail::formF::matches(a_actorbase, filterForms_NOT)) {
			return false;
		}

		if (!filterForms_MATCH.empty() && !detail::formF::matches(a_actorbase, filterForms_MATCH)) {
			return false;
		}

		return true;
	}

	SECONDARY_RESULT secondary(const RE::TESNPC& a_actorbase, const LevelFilters& a_levelFilters, const Traits& a_traits, float a_chance, bool a_noPlayerLevelDistribution)
	{
		constexpr auto failed = SECONDARY_RESULT::kFail;

		if (Lookup::detail::has_level_filters(a_levelFilters)) {
			if (a_noPlayerLevelDistribution && a_actorbase.HasPCLevelMult()) {
				return failed;
			}
		}

		auto& [actorMin, actorMax] = a_levelFilters.first;
		const auto actorLevel = a_actorbase.GetLevel();

		if (actorMin < UINT16_MAX && actorMax < UINT16_MAX) {
			if (actorLevel < actorMin || actorLevel > actorMax) {
				return failed;
			}
		} else if (actorMin < UINT16_MAX && actorLevel < actorMin) {
			return failed;
		} else if (actorMax < UINT16_MAX && actorLevel > actorMax) {
			return failed;
		}

		for (auto& [skillType, skill] : a_levelFilters.second) {
			auto& [skillMin, skillMax] = skill;

			const auto skillLevel = a_actorbase.playerSkills.values[skillType];

			if (skillMin < UINT8_MAX && skillMax < UINT8_MAX) {
				if (skillLevel < skillMin || skillLevel > skillMax) {
					return failed;
				}
			} else if (skillMin < UINT8_MAX && skillLevel < skillMin) {
				return failed;
			} else if (skillMax < UINT8_MAX && skillLevel > skillMax) {
				return failed;
			}
		}

		const auto& [sex, isUnique, isSummonable, isChild] = a_traits;
		if (sex && a_actorbase.GetSex() != *sex) {
			return failed;
		}
		if (isUnique && a_actorbase.IsUnique() != *isUnique) {
			return failed;
		}
		if (isSummonable && a_actorbase.IsSummonable() != *isSummonable) {
			return failed;
		}
		if (isChild && (a_actorbase.race && a_actorbase.race->IsChildRace()) != *isChild) {
			return failed;
		}

		if (!numeric::essentially_equal(a_chance, 100.0f)) {
			if (const auto rng = staticRNG.Generate<float>(0.0f, 100.0f); rng > a_chance) {
				return SECONDARY_RESULT::kFailRNG;
			}
		}

		return SECONDARY_RESULT::kPass;
	}
}
