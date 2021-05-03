#pragma once

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include <SimpleIni.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace logger = SKSE::log;
using namespace SKSE::util;

#define DLLEXPORT __declspec(dllexport)

using namespace std::string_view_literals;

using FormIDPair = std::pair<RE::FormID, std::string>;
using FormIDVec = std::vector<RE::FormID>;
using StringVec = std::vector<std::string>;
using FormVec = std::vector<RE::TESForm*>;

using ActorLevel = std::pair<std::uint16_t, std::uint16_t>;
using SkillLevel = std::pair<std::uint32_t, std::pair<std::uint8_t, std::uint8_t>>;
using ItemCount = std::int32_t;
using Chance = float;
using NPCCount = std::uint32_t;

using INIData = std::tuple<
	std::variant<FormIDPair, std::string>,
	std::pair<StringVec, StringVec>,
	std::pair<FormIDVec, FormIDVec>,
	std::pair<ActorLevel, SkillLevel>,
	RE::SEX,
	ItemCount,
	Chance>;
using INIDataVec = std::vector<INIData>;

template <class Form>
using FormCountPair = std::pair<Form*, ItemCount>;
template <class Form>
using FormData = std::tuple<
	FormCountPair<Form>,
	std::pair<StringVec, StringVec>,
	std::pair<FormVec, FormVec>,
	std::pair<ActorLevel, SkillLevel>,
	RE::SEX,
	Chance,
	NPCCount>;
template <class Form>
using FormDataVec = std::vector<FormData<Form>>;

using namespace SKSE::STRING;
using FLAG = RE::ACTOR_BASE_DATA::TEMPLATE_USE_FLAG;

namespace INI
{
	enum TYPE : std::uint32_t
	{
		kFormIDPair = 0,
		kFormID = kFormIDPair,
		kStrings,
		kESP = kStrings,
		kFilterIDs,
		kLevel,
		kGender,
		kItemCount,
		kChance
	};
}
using INI_TYPE = INI::TYPE;

namespace DATA
{
	enum TYPE : std::uint32_t
	{
		kForm = 0,
		kItemCount,
		kStrings = kItemCount,
		kFilterForms,
		kLevel,
		kGender,
		kChance,
		kNPCCount
	};
}
using DATA_TYPE = DATA::TYPE;

namespace ACTOR_LEVEL
{
	enum TYPE : std::uint32_t
	{
		kMin = 0,
		kMax
	};

	inline constexpr auto MAX = std::numeric_limits<std::uint16_t>::max();
}
using A_LEVEL = ACTOR_LEVEL::TYPE;

namespace SKILL_LEVEL
{
	enum TYPE : std::uint32_t
	{
		kType = 0,
		kMin,
		kMax
	};

	inline constexpr auto TYPE_MAX = std::numeric_limits<std::uint32_t>::max();
	inline constexpr auto VALUE_MAX = std::numeric_limits<std::uint8_t>::max();
}
using S_LEVEL = SKILL_LEVEL::TYPE;

namespace UTIL
{
	auto splitTrimmedString(const std::string& a_str, bool a_trim, const std::string& a_delimiter = " , ") -> std::vector<std::string>;

	auto LookupFormType(RE::FormType a_type) -> std::string;

	bool FormIDToForm(const FormIDVec& a_formIDVec, FormVec& a_formVec, const std::string& a_type);

	auto GetNameMatch(const std::string& a_name, const StringVec& a_strings) -> bool;

	auto GetFilterFormMatch(RE::TESNPC& a_actorbase, const FormVec& a_forms) -> bool;

	auto GetKeywordsMatch(RE::TESNPC& a_actorbase, const StringVec& a_strings) -> bool;
}


template <class Form>
void LookupForms(const std::string& a_type, const INIDataVec& a_INIDataVec, FormDataVec<Form>& a_formDataVec)
{
	logger::info("	Starting {} lookup", a_type);

	for (auto& [formIDPair_ini, strings_ini, filterIDs_ini, level_ini, gender_ini, itemCount_ini, chance_ini] : a_INIDataVec) {
		Form* form;

		if (std::holds_alternative<FormIDPair>(formIDPair_ini)) {
			auto [formID, mod] = std::get<FormIDPair>(formIDPair_ini);

			form = RE::TESDataHandler::GetSingleton()->LookupForm<Form>(formID, mod);
			if (!form) {
				auto base = RE::TESDataHandler::GetSingleton()->LookupForm(formID, mod);
				form = base ? static_cast<Form*>(base) : nullptr;
			}

			if (form) {
				auto name = std::string_view();
				if constexpr (std::is_same_v<Form, RE::BGSKeyword>) {
					name = form->formEditorID;
				} else {
					name = form->GetName();
				}
				logger::info("		{} [0x{:08X}] ({}) SUCCESS", name, formID, mod);
			} else {
				logger::error("		{} [0x{:08X}] ({}) FAIL - missing or wrong form type", a_type, formID, mod);
				continue;
			}

		} else if constexpr (std::is_same_v<Form, RE::BGSKeyword>) {
			if (!std::holds_alternative<std::string>(formIDPair_ini)) {
				continue;
			}

			if (auto keywordName = std::get<std::string>(formIDPair_ini); !keywordName.empty()) {
				auto& keywordArray = RE::TESDataHandler::GetSingleton()->GetFormArray<RE::BGSKeyword>();

				auto result = std::find_if(keywordArray.begin(), keywordArray.end(), [&](const auto& keyword) {
					return keyword && insenstiveStringCompare(keyword->formEditorID, keywordName);
				});

				if (result == keywordArray.end()) {
					if (auto keyword = RE::BGSKeyword::CreateKeyword(keywordName); keyword) {
						keywordArray.push_back(keyword);
						logger::info("		{} [0x{:08X}] SUCCESS", keywordName, keyword->GetFormID());
						form = keyword;
					} else {
						logger::error("		{} FAIL - couldn't create keyword", keywordName);
						continue;
					}
				} else {
					if (auto keyword = *result; keyword) {
						logger::info("		{} [0x{:08X}] INFO - using existing keyword", keywordName, keyword->GetFormID());
						form = keyword;
					}
				}
			}
		}

		FormVec tempFilterForms;
		FormVec tempFilterForms_NOT;

		auto& [filterIDs, filterIDs_NOT] = filterIDs_ini;

		if (!UTIL::FormIDToForm(filterIDs, tempFilterForms, "Filter Form")) {
			continue;
		}
		if (!UTIL::FormIDToForm(filterIDs_NOT, tempFilterForms_NOT, "NOT Filter Form")) {
			continue;
		}

		FormCountPair<Form> formCountPair = { form, itemCount_ini };
		std::pair<FormVec, FormVec> filterForms = { tempFilterForms, tempFilterForms_NOT };
		std::uint32_t npcCount = 0;

		FormData<Form> formData = { formCountPair, strings_ini, filterForms, level_ini, gender_ini, chance_ini, npcCount };

		a_formDataVec.emplace_back(formData);
	}
}


template <class Form>
auto PassedPrimaryFilters(RE::TESNPC& a_actorbase, const FormData<Form>& a_formData) -> bool
{
	auto& [strings, strings_NOT] = std::get<DATA_TYPE::kStrings>(a_formData);
	auto& [filterForms, filterForms_NOT] = std::get<DATA_TYPE::kFilterForms>(a_formData);

	if (strings.empty() && strings_NOT.empty() && filterForms.empty() && filterForms_NOT.empty()) {
		return true;
	}

	std::string name(a_actorbase.GetName());
	if (!name.empty()) {
		if (!strings_NOT.empty() && UTIL::GetNameMatch(name, strings_NOT)) {
			return false;
		}
		if (!strings.empty() && UTIL::GetNameMatch(name, strings)) {
			return true;
		}
	}

	if (!filterForms_NOT.empty() && UTIL::GetFilterFormMatch(a_actorbase, filterForms_NOT)) {
		return false;
	}

	if (!filterForms.empty() && UTIL::GetFilterFormMatch(a_actorbase, filterForms)) {
		return true;
	}

	if (!strings_NOT.empty() && UTIL::GetKeywordsMatch(a_actorbase, strings_NOT)) {
		return false;
	}

	if (!strings.empty() && UTIL::GetKeywordsMatch(a_actorbase, strings)) {
		return true;
	}

	return false;
}


template <class Form>
auto PassedSecondaryFilters(RE::TESNPC& a_actorbase, const FormData<Form>& a_formData) -> bool
{
	auto const levelPair = std::get<DATA_TYPE::kLevel>(a_formData);
	auto const& [actorLevelPair, skillLevelPair] = levelPair;

	auto const gender = std::get<DATA_TYPE::kGender>(a_formData);
	auto const chance = std::get<DATA_TYPE::kChance>(a_formData);

	if (gender != RE::SEX::kNone && a_actorbase.GetSex() != gender) {
		return false;
	}

	auto [actorMin, actorMax] = actorLevelPair;
	auto actorLevel = a_actorbase.actorData.level;

	if (actorMin < ACTOR_LEVEL::MAX && actorMax < ACTOR_LEVEL::MAX) {
		if (!(actorMin < actorLevel && actorMax > actorLevel)) {
			return false;
		}
	} else if (actorMin < ACTOR_LEVEL::MAX && actorLevel < actorMin) {
		return false;
	} else if (actorMax < ACTOR_LEVEL::MAX && actorLevel > actorMax) {
		return false;
	}

	auto skillType = skillLevelPair.first;
	auto [skillMin, skillMax] = skillLevelPair.second;

	if (skillType >= 0 && skillType < 18) {
		auto const skillLevel = a_actorbase.playerSkills.values[skillType];

		if (skillMin < SKILL_LEVEL::VALUE_MAX && skillMax < SKILL_LEVEL::VALUE_MAX) {
			if (!(skillMin < skillLevel && skillMax > skillLevel)) {
				return false;
			}
		} else if (skillMin < SKILL_LEVEL::VALUE_MAX && skillLevel < skillMin) {
			return false;
		} else if (skillMax < SKILL_LEVEL::VALUE_MAX && skillLevel > skillMax) {
			return false;
		}
	}

	if (chance != 100) {
		auto rng = SKSE::RNG::GetSingleton()->Generate<float>(0.0, 100.0);
		if (rng > chance) {
			return false;
		}
	}

	return true;
}


template <class Form>
void ForEachForm(RE::TESNPC& a_actorbase, FormDataVec<Form>& a_formDataVec, std::function<bool(FormCountPair<Form>)> a_fn)
{
	for (auto& formData : a_formDataVec) {
		if (!PassedPrimaryFilters(a_actorbase, formData) || !PassedSecondaryFilters(a_actorbase, formData)) {
			continue;
		}
		auto const formCountPair = std::get<DATA_TYPE::kForm>(formData);
		if (a_fn(formCountPair)) {
			++std::get<DATA_TYPE::kNPCCount>(formData);
		}
	}
}


template <class Form>
void ListNPCCount(const std::string& a_type, FormDataVec<Form>& a_formDataVec, const size_t a_totalNPCCount)
{
	if (!a_formDataVec.empty()) {
		logger::info("	{}", a_type);

		for (auto& formData : a_formDataVec) {
			auto [form, itemCount] = std::get<DATA_TYPE::kForm>(formData);
			auto npcCount = std::get<DATA_TYPE::kNPCCount>(formData);

			if (form) {
				auto name = std::string_view();
				if constexpr (std::is_same_v<Form, RE::BGSKeyword>) {
					name = form->formEditorID;
				} else {
					name = form->GetName();
				}
				logger::info("		{} [0x{:08X}] added to {}/{} NPCs", name, form->GetFormID(), npcCount, a_totalNPCCount);
			}
		}
	}
}
