#include "LookupFilters.h"
#include "LookupForms.h"

namespace Filter
{
	namespace detail
	{
		namespace name
		{
			bool contains(const std::string& a_name, const StringVec& a_strings)
			{
				return std::ranges::any_of(a_strings, [&](const auto& str) {
					return string::icontains(a_name, str);
				});
			}

			bool matches(const std::string& a_name, const StringVec& a_strings)
			{
				return std::ranges::any_of(a_strings, [&](const auto& str) {
					return string::iequals(a_name, str);
				});
			}
		}

		namespace form
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

		namespace keyword
		{
			bool contains(RE::TESNPC& a_actorbase, const StringVec& a_strings)
			{
				return std::ranges::any_of(a_strings, [&a_actorbase](const auto& str) {
					return a_actorbase.ContainsKeyword(str);
				});
			}

			bool matches(RE::TESNPC& a_actorbase, const StringVec& a_strings, bool a_matchesAll = false)
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
	}

	bool strings(RE::TESNPC& a_actorbase, const FormData& a_formData)
	{
		auto& [strings_ALL, strings_NOT, strings_MATCH, strings_ANY] = std::get<DATA::TYPE::kStrings>(a_formData);

		if (!strings_ALL.empty() && !detail::keyword::matches(a_actorbase, strings_ALL, true)) {
			return false;
		}

		const std::string name = a_actorbase.GetName();
		const std::string editorID = Cache::EditorID::GetEditorID(a_actorbase.GetFormID());

		if (!strings_NOT.empty()) {
			bool result = false;
			if (!name.empty() && detail::name::matches(name, strings_NOT)) {
				result = true;
			}
			if (!result && !editorID.empty() && detail::name::matches(editorID, strings_NOT)) {
				result = true;
			}
			if (!result && detail::keyword::matches(a_actorbase, strings_NOT)) {
				result = true;
			}
			if (result) {
				return false;
			}
		}

		if (!strings_MATCH.empty()) {
			bool result = false;
			if (!name.empty() && detail::name::matches(name, strings_MATCH)) {
				result = true;
			}
			if (!result && !editorID.empty() && detail::name::matches(editorID, strings_MATCH)) {
				result = true;
			}
			if (!result && detail::keyword::matches(a_actorbase, strings_MATCH)) {
				result = true;
			}
			if (!result) {
				return false;
			}
		}

		if (!strings_ANY.empty()) {
			bool result = false;
			if (!name.empty() && detail::name::contains(name, strings_ANY)) {
				result = true;
			}
			if (!result && !editorID.empty() && detail::name::contains(editorID, strings_ANY)) {
				result = true;
			}
			if (!result && detail::keyword::contains(a_actorbase, strings_ANY)) {
				result = true;
			}
			if (!result) {
				return false;
			}
		}

		return true;
	}

	bool forms(RE::TESNPC& a_actorbase, const FormData& a_formData)
	{
		auto& [filterForms_ALL, filterForms_NOT, filterForms_MATCH] = std::get<DATA::TYPE::kFilterForms>(a_formData);

		if (!filterForms_ALL.empty() && !detail::form::matches(a_actorbase, filterForms_ALL, true)) {
			return false;
		}

		if (!filterForms_NOT.empty() && detail::form::matches(a_actorbase, filterForms_NOT)) {
			return false;
		}

		if (!filterForms_MATCH.empty() && !detail::form::matches(a_actorbase, filterForms_MATCH)) {
			return false;
		}

		return true;
	}

	SECONDARY_RESULT secondary(const RE::TESNPC& a_actorbase, const FormData& a_formData, bool a_noPlayerLevelDistribution)
	{
		const auto failed = SECONDARY_RESULT::kFail;

		const auto& [sex, isUnique, isSummonable] = std::get<DATA::TYPE::kTraits>(a_formData);
		if (sex && a_actorbase.GetSex() != *sex) {
			return failed;
		}
		if (isUnique && a_actorbase.IsUnique() != *isUnique) {
			return failed;
		}
		if (isSummonable && a_actorbase.IsSummonable() != *isSummonable) {
			return failed;
		}

		auto& levelEntry = std::get<DATA::TYPE::kLevel>(a_formData);

		if (Lookup::detail::has_level_filters(levelEntry)) {
			if (a_noPlayerLevelDistribution && a_actorbase.HasPCLevelMult()) {
				return failed;
			}
		}

		auto& [actorMin, actorMax] = levelEntry.first;
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

		for (auto& [skillType, skill] : levelEntry.second) {
			auto& [skillMin, skillMax] = skill;

			if (skillType < 18) {
				auto const skillLevel = a_actorbase.playerSkills.values[skillType];

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
		}

		const auto chance = std::get<DATA::TYPE::kChance>(a_formData);
		if (!numeric::essentially_equal(chance, 100.0f)) {
			if (auto rng = RNG::GetSingleton()->Generate<float>(0.0f, 100.0f); rng > chance) {
				return SECONDARY_RESULT::kFailDueToRNG;
			}
		}

		return SECONDARY_RESULT::kPass;
	}
}
