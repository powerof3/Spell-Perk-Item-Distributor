#pragma once

#include "enum.h"

extern std::unordered_set<RE::TESBoundObject*> equipOnLoadForms;


namespace UTIL
{
	std::vector<std::string> splitTrimmedString(const std::string& a_str, bool a_trim, const std::string& a_delimiter = " , ");

	std::string LookupFormType(RE::FormType a_type);
}


template <typename T>
T to_int(const std::string& a_str)
{
	if constexpr (!std::is_unsigned_v<T>) {
		return static_cast<T>(std::stoi(a_str));
	} else if constexpr (std::is_unsigned_v<T>) {
		return static_cast<T>(std::stoul(a_str));
	}
}


template <class Form>
void LookupForms(const std::string& a_type, const INIDataVec& a_INIDataVec, FormDataVec<Form>& a_formDataVec)
{
	logger::info("	Starting {} lookup", a_type);

	for (auto& [formIDPair_ini, strings_ini, filterIDs, level_ini, gender_ini, itemCount, chance_ini] : a_INIDataVec) {
		
		auto [formID, mod] = formIDPair_ini;	
		auto form = RE::TESDataHandler::GetSingleton()->LookupForm<Form>(formID, mod);
		if (!form) {
			auto base = RE::TESDataHandler::GetSingleton()->LookupForm(formID, mod);
			form = base ? static_cast<Form*>(base) : nullptr;
		}
		if (form) {
			logger::info("		{} [0x{:08X}] ({}) SUCCESS", form->GetName(), formID, mod);
		} else {
			logger::error("		{} [0x{:08X}] ({}) FAIL - missing or wrong form type", a_type, formID, mod);
			continue;
		}

		FormVec tempFilterForms;
		if (!filterIDs.empty()) {
			logger::info("			Starting filter form lookup");
			for (auto filterID : filterIDs) {
				auto filterForm = RE::TESForm::LookupByID(filterID);
				if (filterForm) {
					auto formType = filterForm->GetFormType();
					auto const type = UTIL::LookupFormType(formType);
					if (!type.empty()) {
						logger::info("				{} [0x{:08X}] SUCCESS", type, filterID);
						tempFilterForms.push_back(filterForm);
					} else {
						logger::error("				[0x{:08X}]) FAIL - wrong formtype ({})", filterID, formType);
					}
				} else {
					logger::error("					[0x{:08X}] FAIL - form doesn't exist", filterID);
				}
			}
		}

		FormData<Form> formData;
		auto& [formCountPair, strings, filterForms, level, gender, chance, count] = formData;

		formCountPair = { form, itemCount.first };
		if (itemCount.second) {
			auto boundObject = form->As<RE::TESBoundObject>();
			if (boundObject) {
				equipOnLoadForms.insert(boundObject);
			}
		}
		strings = strings_ini;
		filterForms = tempFilterForms;
		level = level_ini;
		gender = gender_ini;
		chance = chance_ini;
		count = 0;

		a_formDataVec.push_back(formData);
	} 
}


template <class Form>
bool PassedPrimaryFilters(RE::TESNPC* a_actorbase, const FormData<Form>& a_formData)
{
	auto strings = std::get<DATA_TYPE::kStrings>(a_formData);
	auto filterForms = std::get<DATA_TYPE::kFilterForms>(a_formData);

	if (strings.empty() && filterForms.empty()) {
		return true;
	}

	if (!strings.empty()) {
		std::string name = a_actorbase->GetName();
		if (!name.empty() && std::any_of(strings.begin(), strings.end(), [&name](const std::string& str) {
				return SKSE::UTIL::STRING::insenstiveStringCompare(str, name);
			})) {
			return true;
		}
	}

	if (!filterForms.empty()) {
		for (auto& filterForm : filterForms) {
			if (filterForm) {
				switch (filterForm->GetFormType()) {
				case RE::FormType::CombatStyle:
					{
						auto combatStyle = filterForm->As<RE::TESCombatStyle>();
						if (combatStyle && a_actorbase->GetCombatStyle() == combatStyle) {
							return true;
						}
					}
					break;
				case RE::FormType::Class:
					{
						auto npcClass = filterForm->As<RE::TESClass>();
						if (npcClass && a_actorbase->IsInClass(npcClass)) {
							return true;
						}
					}
					break;
				case RE::FormType::Faction:
					{
						auto faction = filterForm->As<RE::TESFaction>();
						if (faction && a_actorbase->IsInFaction(faction)) {
							return true;
						}
					}
					break;
				case RE::FormType::Race:
					{
						auto race = filterForm->As<RE::TESRace>();
						if (race && a_actorbase->GetRace() == race) {
							return true;
						}
					}
					break;
				case RE::FormType::Outfit:
					{
						auto outfit = filterForm->As<RE::BGSOutfit>();
						if (outfit && a_actorbase->defaultOutfit == outfit) {
							return true;
						}
					}
					break;
				}
			}
		}
	}

	if (!strings.empty()) {
		if (std::any_of(strings.begin(), strings.end(), [&a_actorbase](const std::string& str) {
				return a_actorbase->HasKeyword(str.c_str());
			})) {
			return true;
		}
	}

	return false;
}


template <class Form>
bool PassedSecondaryFilters(RE::TESNPC* a_actorbase, const FormData<Form>& a_formData)
{
	auto const levelPair = std::get<DATA_TYPE::kLevel>(a_formData);
	auto const& [actorLevelPair, skillLevelPair] = levelPair;

	auto const gender = std::get<DATA_TYPE::kGender>(a_formData);
	auto const chance = std::get<DATA_TYPE::kChance>(a_formData);

	if (gender != RE::SEX::kNone) {
		if (a_actorbase->GetSex() != gender) {
			return false;
		}
	}

	auto [actorMin, actorMax] = actorLevelPair;
	auto actorLevel = a_actorbase->actorData.level;

	if (actorMin < ACTOR_LEVEL::MAX && actorMax < ACTOR_LEVEL::MAX) {
		if (!((actorMin < actorLevel) && (actorMax > actorLevel))) {
			return false;
		}
	} else if (actorMin < ACTOR_LEVEL::MAX) {
		if (actorLevel < actorMin) {
			return false;
		}
	} else if (actorMax < ACTOR_LEVEL::MAX) {
		if (actorLevel > actorMax) {
			return false;
		}
	}

	auto skillType = skillLevelPair.first;
	auto [skillMin, skillMax] = skillLevelPair.second;

	if (skillType >= 0 && skillType < 18) {
		auto const skillLevel = a_actorbase->playerSkills.values[skillType];

		if (skillMin < SKILL_LEVEL::VALUE_MAX && skillMax < SKILL_LEVEL::VALUE_MAX) {
			if (!((skillMin < skillLevel) && (skillMax > skillLevel))) {
				return false;
			}
		} else if (skillMin < SKILL_LEVEL::VALUE_MAX) {
			if (skillLevel < skillMin) {
				return false;
			}
		} else if (skillMax < SKILL_LEVEL::VALUE_MAX) {
			if (skillLevel > skillMax) {
				return false;
			}
		}
	}

	if (chance != 100) {
		auto rand = SKSE::RNG::GetSingleton();
		auto rng = rand->GenerateRandomNumber<std::uint32_t>(0, 100);
		if (rng > chance) {
			return false;
		}
	}

	return true;
}


template <class Form>
void ForEachForm(RE::TESNPC* a_actorbase, FormDataVec<Form>& a_formDataVec, std::function<bool(const FormCountPair<Form>)> a_fn)
{
	for (auto& formData : a_formDataVec) {
		if (!PassedPrimaryFilters(a_actorbase, formData) || !PassedSecondaryFilters(a_actorbase, formData)) {
			continue;
		}
		auto const formCountPair = std::get<DATA_TYPE::kForm>(formData);
		if (a_fn(formCountPair)) {
			std::get<DATA_TYPE::kNPCCount>(formData)++;
		}
	}
}


template <class Form>
void ListNPCCount(const std::string& a_type, FormDataVec<Form>& a_formDataVec, const size_t a_totalCount)
{
	if (!a_formDataVec.empty()) {
		logger::info("	{}", a_type);

		for (auto& formData : a_formDataVec) {
			std::pair formPair = std::get<DATA_TYPE::kForm>(formData);
			std::uint32_t count = std::get<DATA_TYPE::kNPCCount>(formData);

			if (formPair.first) {
				logger::info("		{} [0x{:08X}] added to {}/{} NPCs", formPair.first->GetName(), formPair.first->GetFormID(), count, a_totalCount);
			}
		}
	}
}
