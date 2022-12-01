#pragma once

#include "LookupConfigs.h"
#include "LookupFilters.h"

namespace Forms
{
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
				buffered_logger::info("\t\tFound merged: {}", conversion_log);
			}
		}

		inline bool formID_to_form(RE::TESDataHandler* a_dataHandler, RawFormVec& a_rawFormVec, FormVec& a_formVec, const std::string& a_path)
		{
			if (a_rawFormVec.empty()) {
				return true;
			}
			for (auto& formOrEditorID : a_rawFormVec) {
				if (const auto formModPair(std::get_if<FormModPair>(&formOrEditorID)); formModPair) {
					auto& [formID, modName] = *formModPair;
					if (g_mergeMapperInterface) {
						get_merged_IDs(formID, modName);
					}
					if (modName && !formID) {
						if (const RE::TESFile* filterMod = a_dataHandler->LookupModByName(*modName); filterMod) {
							buffered_logger::info("\t\t\t[{}] Filter ({}) INFO - mod found", a_path, filterMod->fileName);
							a_formVec.push_back(filterMod);
						} else {
							buffered_logger::error("\t\t\t[{}] Filter ({}) SKIP - mod cannot be found", a_path, *modName);
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
								buffered_logger::error("\t\t\t[{}] Filter [0x{:X}] ({}) SKIP - invalid formtype ({})", a_path, *formID, modName.value_or(""), formType);
							}
						} else {
							buffered_logger::error("\t\t\t[{}] Filter [0x{:X}] ({}) SKIP - form doesn't exist", a_path, *formID, modName.value_or(""));
						}
					}
				} else if (std::holds_alternative<std::string>(formOrEditorID)) {
					if (auto editorID = std::get<std::string>(formOrEditorID); !editorID.empty()) {
						if (auto filterForm = RE::TESForm::LookupByEditorID(editorID); filterForm) {
							const auto formType = filterForm->GetFormType();
							if (Cache::FormType::GetWhitelisted(formType)) {
								a_formVec.push_back(filterForm);
							} else {
								buffered_logger::error("\t\t\t[{}] Filter ({}) SKIP - invalid formtype ({})", a_path, editorID, formType);
							}
						} else {
							buffered_logger::error("\t\t\t[{}] Filter ({}) SKIP - form doesn't exist", a_path, editorID);
						}
					}
				}
			}
			return !a_formVec.empty();
		}
	}

	/// Custom ordering for keywords that ensures that dependent keywords are distributed after the keywords that they depend on.
	struct KeywordDependencySorter
	{
		static bool sort(RE::BGSKeyword* a, RE::BGSKeyword* b);
	};

	template <class Form>
	struct Data
	{
		Form* form{ nullptr };
		IdxOrCount idxOrCount{ 1 };
		FilterData filters{};

		bool operator<(const Data& a_rhs) const
		{
			if constexpr (std::is_same_v<RE::BGSKeyword, Form>) {
				return KeywordDependencySorter::sort(form, a_rhs.form);
			} else {
				return true;
			}
		}

		bool operator==(const Data& a_rhs) const
		{
			if (!form || !a_rhs.form) {
				return false;
			}
			return form->GetFormID() == a_rhs.form->GetFormID();
		}
	};

	template <class Form>
	using DataVec = std::vector<Data<Form>>;

	template <class Form>
	struct Distributables
	{
		explicit operator bool();

		std::size_t GetSize();
		std::size_t GetLeveledSize();

		const DataVec<Form>& GetForms(bool a_onlyLevelEntries, bool a_noLevelDistribution);
		DataVec<Form>& GetForms();

		void LookupForms(RE::TESDataHandler* a_dataHandler, std::string_view a_type, INI::DataVec& a_INIDataVec);

		// Init formsWithLevels and formsNoLevels
		void FinishLookupForms();

	private:
		DataVec<Form> forms{};
		DataVec<Form> formsWithLevels{};
		DataVec<Form> formsNoLevels{};

		Map<Form*, std::uint32_t> results{};
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

	std::size_t GetTotalEntries();
	std::size_t GetTotalLeveledEntries();
}

template <class Form>
Forms::Distributables<Form>::operator bool()
{
	return !forms.empty();
}

template <class Form>
std::size_t Forms::Distributables<Form>::GetSize()
{
	return forms.size();
}

template <class Form>
std::size_t Forms::Distributables<Form>::GetLeveledSize()
{
	return formsWithLevels.size();
}

template <class Form>
Forms::DataVec<Form>& Forms::Distributables<Form>::GetForms()
{
	return forms;
}

template <class Form>
const Forms::DataVec<Form>& Forms::Distributables<Form>::GetForms(bool a_onlyLevelEntries, bool a_noLevelDistribution)
{
	if (a_onlyLevelEntries) {
		return formsWithLevels;
	}
	if (a_noLevelDistribution) {
		return formsNoLevels;
	}
	return forms;
}

template <class Form>
void Forms::Distributables<Form>::LookupForms(RE::TESDataHandler* a_dataHandler, std::string_view a_type, INI::DataVec& a_INIDataVec)
{
	if (a_INIDataVec.empty()) {
		return;
	}

	logger::info("\tStarting {} lookup", a_type);

	forms.reserve(a_INIDataVec.size());

	for (auto& [formOrEditorID, strings, filterIDs, level, traits, idxOrCount, chance, path] : a_INIDataVec) {
		Form* form = nullptr;

		if (std::holds_alternative<FormModPair>(formOrEditorID)) {
			if (auto [formID, modName] = std::get<FormModPair>(formOrEditorID); formID) {
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
					buffered_logger::error("\t\t[{}] [0x{:X}] ({}) FAIL - formID doesn't exist", path, *formID, modName.value_or(""));
				} else {
					if constexpr (std::is_same_v<Form, RE::BGSKeyword>) {
						if (string::is_empty(form->GetFormEditorID())) {
							form = nullptr;
							buffered_logger::error("\t\t[{}] [0x{:X}] ({}) FAIL - keyword does not have a valid editorID", path, *formID, modName.value_or(""));
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
					return keyword && keyword->formEditorID == keywordName.c_str();
				});

				if (result != keywordArray.end()) {
					if (const auto keyword = *result; keyword) {
						if (!keyword->IsDynamicForm()) {
							buffered_logger::info("\t\t[{}] {} [0x{:X}] INFO - using existing keyword", path, keywordName, keyword->GetFormID());
						}
						form = keyword;
					} else {
						buffered_logger::critical("\t\t[{}] {} FAIL - couldn't get existing keyword", path, keywordName);
						continue;
					}
				} else {
					const auto factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSKeyword>();
					if (auto keyword = factory ? factory->Create() : nullptr; keyword) {
						keyword->formEditorID = keywordName;
						keywordArray.push_back(keyword);
						buffered_logger::info("\t\t[{}] {} [0x{:X}] INFO - creating keyword", path, keywordName, keyword->GetFormID());

						form = keyword;
					} else {
						buffered_logger::critical("\t\t[{}] {} FAIL - couldn't create keyword", path, keywordName);
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
					buffered_logger::error("		[{}] {} FAIL - editorID doesn't exist", path, editorID);
				}
			}
		}

		if (!form) {
			continue;
		}

		FormFilters filterForms{};

		bool validEntry = detail::formID_to_form(a_dataHandler, filterIDs.ALL, filterForms.ALL, path);
		if (validEntry) {
			validEntry = detail::formID_to_form(a_dataHandler, filterIDs.NOT, filterForms.NOT, path);
		}
		if (validEntry) {
			validEntry = detail::formID_to_form(a_dataHandler, filterIDs.MATCH, filterForms.MATCH, path);
		}

		if (!validEntry) {
			continue;
		}

		Data<Form> formData{ form, idxOrCount, FilterData{ strings, filterForms, level, traits, chance } };
		forms.emplace_back(formData);
	}
}

template <class Form>
void Forms::Distributables<Form>::FinishLookupForms()
{
	if (forms.empty()) {
		return;
	}

	formsWithLevels.reserve(forms.size());

	std::copy_if(forms.begin(), forms.end(),
		std::back_inserter(formsWithLevels),
		[](const auto& formData) { return formData.filters.HasLevelFilters(); });

	formsNoLevels.reserve(forms.size());

	std::remove_copy_if(forms.begin(), forms.end(),
		std::back_inserter(formsNoLevels),
		[](const auto& formData) { return formData.filters.HasLevelFilters(); });
}
