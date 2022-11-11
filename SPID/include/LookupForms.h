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
		inline void get_merged_IDs(std::optional<RE::FormID>& a_formID, std::optional<std::string>& a_modName)
		{
			const auto [mergedModName, mergedFormID] = g_mergeMapperInterface->GetNewFormID(a_modName.value_or("").c_str(), a_formID.value_or(0));
			std::string conversion_log{};
			if (a_formID.value_or(0) && mergedFormID && a_formID.value_or(0) != mergedFormID) {
				conversion_log = std::format("0x{:X}->0x{:X}", a_formID.value_or(0), mergedFormID);
				a_formID.emplace(mergedFormID);
			}
			const std::string mergedModString{ mergedModName };
			if (!a_modName.value_or("").empty() && !mergedModString.empty() && a_modName.value_or("") != mergedModString) {
				if (conversion_log.empty()) {
					conversion_log = std::format("{}->{}", a_modName.value_or(""), mergedModString);
				} else {
					conversion_log = std::format("{}~{}->{}", conversion_log, a_modName.value_or(""), mergedModString);
				}
				a_modName.emplace(mergedModName);
			}
			if (!conversion_log.empty()) {
				logger::info("\t\tFound merged: {}", conversion_log);
			}
		}

		inline bool formID_to_form(RE::TESDataHandler* a_dataHandler, FormIDVec& a_formIDVec, FormVec& a_formVec, const std::string& a_path)
		{
			if (a_formIDVec.empty()) {
				return true;
			}
			for (auto& formOrEditorID : a_formIDVec) {
				if (const auto formIDPair(std::get_if<FormIDPair>(&formOrEditorID)); formIDPair) {
					auto& [formID, modName] = *formIDPair;
					if (g_mergeMapperInterface) {
						get_merged_IDs(formID, modName);
					}
					if (modName && !formID) {
						if (const RE::TESFile* filterMod = a_dataHandler->LookupModByName(*modName); filterMod) {
							logger::info("\t\t\t[{}] Filter ({}) INFO - mod found", a_path, filterMod->fileName);
							a_formVec.push_back(filterMod);
						} else {
							logger::error("\t\t\t[{}] Filter ({}) SKIP - mod cannot be found", a_path, *modName);
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
								logger::error("\t\t\t[{}] Filter [0x{:X}] ({}) SKIP - invalid formtype ({})", a_path, *formID, modName.value_or(""), formType);
							}
						} else {
							logger::error("\t\t\t[{}] Filter [0x{:X}] ({}) SKIP - form doesn't exist", a_path, *formID, modName.value_or(""));
						}
					}
				} else if (std::holds_alternative<std::string>(formOrEditorID)) {
					if (auto editorID = std::get<std::string>(formOrEditorID); !editorID.empty()) {
						if (auto filterForm = RE::TESForm::LookupByEditorID(editorID); filterForm) {
							const auto formType = filterForm->GetFormType();
							if (Cache::FormType::GetWhitelisted(formType)) {
								a_formVec.push_back(filterForm);
							} else {
								logger::error("			[{}] Filter ({}) SKIP - invalid formtype ({})", a_path, editorID, formType);
							}
						} else {
							logger::error("			[{}] Filter ({}) SKIP - form doesn't exist", a_path, editorID);
						}
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

		logger::info("\tStarting {} lookup", a_type);

		for (auto& [formOrEditorID, strings, filterIDs, level, traits, idxOrCount, chance, path] : a_INIDataVec) {
			Form* form = nullptr;

			if (std::holds_alternative<FormIDPair>(formOrEditorID)) {
				if (auto [formID, modName] = std::get<FormIDPair>(formOrEditorID); formID) {
					if (g_mergeMapperInterface) {
						detail::get_merged_IDs(formID, modName);
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
						logger::error("		[{}] [0x{:X}] ({}) FAIL - formID doesn't exist", path, *formID, modName.value_or(""));
					} else {
						if constexpr (std::is_same_v<Form, RE::BGSKeyword>) {
							if (string::is_empty(form->GetFormEditorID())) {
								form = nullptr;
								logger::error("		[{}] [0x{:X}] ({}) FAIL - keyword does not have a valid editorID", path, *formID, modName.value_or(""));
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
								logger::info("\t\t[{}] {} [0x{:X}] INFO - using existing keyword", path, keywordName, keyword->GetFormID());
							}
							form = keyword;
						} else {
							logger::critical("\t\t[{}] {} FAIL - couldn't get existing keyword", path, keywordName);
							continue;
						}
					} else {
						const auto factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSKeyword>();
						if (auto keyword = factory ? factory->Create() : nullptr; keyword) {
							keyword->formEditorID = keywordName;
							keywordArray.push_back(keyword);
							logger::info("\t\t[{}] {} [0x{:X}] INFO - creating keyword", path, keywordName, keyword->GetFormID());

							form = keyword;
						} else {
							logger::critical("\t\t[{}] {} FAIL - couldn't create keyword", path, keywordName);
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
						logger::error("		[{}] {} FAIL - editorID doesn't exist", path, editorID);
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
