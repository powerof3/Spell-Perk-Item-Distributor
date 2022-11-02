#pragma once

#include "MergeMapperPluginAPI.h"

namespace Forms
{
	template <class Form>
	struct Distributables
	{
		FormDataVec<Form> forms{};
		FormDataVec<Form> formsWithLevels{};

		explicit operator bool()
		{
			return !forms.empty();
		}
	};

	inline Distributables<RE::SpellItem> spells;
	inline Distributables<RE::BGSPerk> perks;
	inline Distributables<RE::TESBoundObject> items;
	inline Distributables<RE::TESShout> shouts;
	inline Distributables<RE::TESLevSpell> levSpells;
	inline Distributables<RE::TESForm> packages;
	inline Distributables<RE::BGSOutfit> outfits;
	inline Distributables<RE::BGSKeyword> keywords;
	inline Distributables<RE::TESBoundObject> deathItems;
	inline Distributables<RE::TESFaction> factions;
	inline Distributables<RE::BGSOutfit> sleepOutfits;
	inline Distributables<RE::TESObjectARMO> skins;
}

namespace Lookup
{
	using namespace Forms;

	namespace detail
	{
		inline bool formID_to_form(RE::TESDataHandler* a_dataHandler, FormIDPairVec& a_formIDVec, FormVec& a_formVec, const std::string& a_path)
		{
			if (a_formIDVec.empty()) {
				return true;
			}
			for (auto& [formID, modName] : a_formIDVec) {
				if (g_mergeMapperInterface) {
					const auto [mergedModName, mergedFormID] = g_mergeMapperInterface->GetNewFormID(modName.value_or("").c_str(), formID.value_or(0));
					std::string conversion_log{};
					if (formID.value_or(0) && mergedFormID && formID.value_or(0) != mergedFormID) {
						conversion_log = std::format("0x{:X}->0x{:X}", formID.value_or(0), mergedFormID);
						formID.emplace(mergedFormID);
					}
					const std::string mergedModString{ mergedModName };
					if (!(modName.value_or("").empty()) && !mergedModString.empty() && modName.value_or("") != mergedModString) {
						if (conversion_log.empty())
							conversion_log = std::format("{}->{}", modName.value_or(""), mergedModString);
						else
							conversion_log = std::format("{}~{}->{}", conversion_log, modName.value_or(""), mergedModString);
						modName.emplace(mergedModName);
					}
					if (!conversion_log.empty())
						logger::info("\t\tFound merged: {}", conversion_log);
				}
				if (modName && !formID) {
					if (INI::is_mod_name(*modName)) {
						if (const RE::TESFile* filterMod = a_dataHandler->LookupModByName(*modName); filterMod) {
							logger::info("			Filter ({}) INFO - mod found", filterMod->fileName);
							a_formVec.push_back(filterMod);
						} else {
							logger::error("{}			Filter ({}) SKIP - mod cannot be found", a_path, *modName);
						}
					} else {
						if (auto filterForm = RE::TESForm::LookupByEditorID(*modName); filterForm) {
							const auto formType = filterForm->GetFormType();
							if (Cache::FormType::GetWhitelisted(formType)) {
								a_formVec.push_back(filterForm);
							} else {
								logger::error("{}		Filter ({}) SKIP - invalid formtype ({})", a_path, *modName, formType);
							}
						} else {
							logger::error("{}			Filter ({}) SKIP - form doesn't exist", a_path, *modName);
						}
					}
				} else if (formID) {
					auto filterForm = modName ?
					                      a_dataHandler->LookupForm(*formID, *modName) :
					                      RE::TESForm::LookupByID(*formID);
					if (filterForm) {
						const auto formType = filterForm->GetFormType();
						if (Cache::FormType::GetWhitelisted(formType)) {
							a_formVec.push_back(filterForm);
						} else {
							logger::error("{}			Filter [0x{:X}] ({}) SKIP - invalid formtype ({})", a_path, *formID, modName.value_or(""), formType);
						}
					} else {
						logger::error("{}			Filter [0x{:X}] ({}) SKIP - form doesn't exist", a_path, *formID, modName.value_or(""));
					}
				}
			}
			return !a_formVec.empty();
		}

		inline bool has_level_filters(const LevelFilters& a_levelFilters)
		{
			const auto& [actorLevelPair, skillLevelPairs] = a_levelFilters;

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
	}

	template <class Form>
	void get_forms(RE::TESDataHandler* a_dataHandler, std::string_view a_type, INIDataVec& a_INIDataVec, FormDataVec<Form>& a_formDataVec)
	{
		if (a_INIDataVec.empty()) {
			return;
		}

		logger::info("	Starting {} lookup", a_type);

		for (auto& [formOrEditorID, strings, filterIDs, level, traits, idxOrCount, chance, path] : a_INIDataVec) {
			Form* form = nullptr;

			if (std::holds_alternative<FormIDPair>(formOrEditorID)) {
				if (auto [formID, modName] = std::get<FormIDPair>(formOrEditorID); formID) {
					if (g_mergeMapperInterface) {
						const auto [mergedModName, mergedFormID] = g_mergeMapperInterface->GetNewFormID(modName.value_or("").c_str(), formID.value_or(0));
						std::string conversion_log = "";
						if (formID.value_or(0) && mergedFormID && formID.value_or(0) != mergedFormID) {
							conversion_log = std::format("0x{:X}->0x{:X}", formID.value_or(0), mergedFormID);
							formID.emplace(mergedFormID);
						}
						const std::string mergedModString{ mergedModName };
						if (!(modName.value_or("").empty()) && !mergedModString.empty() && modName.value_or("") != mergedModString) {
							if (conversion_log.empty())
								conversion_log = std::format("{}->{}", modName.value_or(""), mergedModString);
							else
								conversion_log = std::format("{}~{}->{}", conversion_log, modName.value_or(""), mergedModString);
							modName.emplace(mergedModName);
						}
						if (!conversion_log.empty())
							logger::info("\t\tFound merged: {}", conversion_log);
					}
					if (modName) {
						form = a_dataHandler->LookupForm<Form>(*formID, *modName);
						if constexpr (std::is_same_v<Form, RE::TESBoundObject>) {
							if (!form) {
								const auto base = a_dataHandler->LookupForm(*formID, *modName);
								form = base ? static_cast<Form*>(base) : nullptr;
							}
						}
					} else {
						form = RE::TESForm::LookupByID<Form>(*formID);
						if constexpr (std::is_same_v<Form, RE::TESBoundObject>) {
							if (!form) {
								const auto base = RE::TESForm::LookupByID(*formID);
								form = base ? static_cast<Form*>(base) : nullptr;
							}
						}
					}
					if (!form) {
						logger::error("		[0x{:X}] ({}) FAIL - formID doesn't exist", *formID, modName.value_or(""));
					} else {
						if constexpr (std::is_same_v<Form, RE::BGSKeyword>) {
							if (string::is_empty(form->GetFormEditorID())) {
								form = nullptr;
								logger::error("		[0x{:X}] ({}) FAIL - keyword does not have a valid editorID", *formID, modName.value_or(""));
							}
						}
					}
				}
			} else if constexpr (std::is_same_v<Form, RE::BGSKeyword>) {
				if (!std::holds_alternative<std::string>(formOrEditorID)) {
					continue;
				}

				if (auto keywordName = std::get<std::string>(formOrEditorID); !keywordName.empty()) {
					auto& keywordArray = a_dataHandler->GetFormArray<RE::BGSKeyword>();

					auto result = std::find_if(keywordArray.begin(), keywordArray.end(), [&](const auto& keyword) {
						return keyword && string::iequals(keyword->formEditorID, keywordName);
					});

					if (result != keywordArray.end()) {
						if (const auto keyword = *result; keyword) {
							if (!keyword->IsDynamicForm()) {
								logger::info("		{} [0x{:X}] INFO - using existing keyword", keywordName, keyword->GetFormID());
							}
							form = keyword;
						} else {
							logger::critical("		{} FAIL - couldn't get existing keyword", keywordName);
							continue;
						}
					} else {
						const auto factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSKeyword>();
						if (auto keyword = factory ? factory->Create() : nullptr; keyword) {
							keyword->formEditorID = keywordName;
							keywordArray.push_back(keyword);

							logger::info("		{} [0x{:X}] INFO - creating keyword", keywordName, keyword->GetFormID());

							form = keyword;
						} else {
							logger::critical("		{} FAIL - couldn't create keyword", keywordName);
						}
					}
				}
			} else if (std::holds_alternative<std::string>(formOrEditorID)) {
				if (auto editorID = std::get<std::string>(formOrEditorID); !editorID.empty()) {
					form = RE::TESForm::LookupByEditorID<Form>(editorID);
					if constexpr (std::is_same_v<Form, RE::TESBoundObject>) {
						if (!form) {
							const auto base = RE::TESForm::LookupByEditorID(editorID);
							form = base ? static_cast<Form*>(base) : nullptr;
						}
					}
					if (!form) {
						logger::error("		{} FAIL - editorID doesn't exist", editorID);
					}
				}
			}

			if (!form) {
				continue;
			}

			bool invalidEntry = false;

			FormFilters filterForms{};
			for (std::uint32_t i = 0; i < 3; i++) {
				if (!detail::formID_to_form(a_dataHandler, filterIDs[i], filterForms[i], path)) {
					invalidEntry = true;
					break;
				}
			}

			if (invalidEntry) {
				continue;
			}

			FormData<Form> formData{ form, idxOrCount, strings, filterForms, level, traits, chance };
			a_formDataVec.emplace_back(formData);
		}
	}

	template <class Form>
	void get_forms_with_level_filters(Distributables<Form>& a_distributables)
	{
		if (a_distributables.forms.empty()) {
			return;
		}

		a_distributables.formsWithLevels.reserve(a_distributables.forms.size());
		for (auto& formData : a_distributables.forms) {
			if (detail::has_level_filters(formData.levelFilters)) {
				a_distributables.formsWithLevels.emplace_back(formData);
			}
		}
	}

	bool GetForms();
}
