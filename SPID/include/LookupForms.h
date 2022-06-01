#pragma once

namespace Forms
{
	template <class T>
	struct FormMap
	{
		FormDataMap<T> forms;

		explicit operator bool()
		{
			return !forms.empty();
		}
	};

	inline FormMap<RE::SpellItem> spells;
	inline FormMap<RE::BGSPerk> perks;
	inline FormMap<RE::TESBoundObject> items;
	inline FormMap<RE::TESShout> shouts;
	inline FormMap<RE::TESLevSpell> levSpells;
	inline FormMap<RE::TESPackage> packages;
	inline FormMap<RE::BGSOutfit> outfits;
	inline FormMap<RE::BGSKeyword> keywords;
	inline FormMap<RE::TESBoundObject> deathItems;
	inline FormMap<RE::TESFaction> factions;
}

namespace Lookup
{
	using namespace Forms;

	namespace detail
	{
		inline bool formID_to_form(RE::TESDataHandler* a_dataHandler, const FormIDPairVec& a_formIDVec, FormVec& a_formVec)
		{
			if (a_formIDVec.empty()) {
				return true;
			}
			for (auto& [formID, modName] : a_formIDVec) {
				if (modName && !formID) {
					if (INI::is_mod_name(*modName)) {
						if (const RE::TESFile* filterMod = a_dataHandler->LookupModByName(*modName); filterMod) {
							logger::info("			Filter ({}) INFO - mod found", filterMod->fileName);
							a_formVec.push_back(filterMod);
						} else {
							logger::error("			Filter ({}) SKIP - mod cannot be found", *modName);
						}
					} else {
						auto filterForm = RE::TESForm::LookupByEditorID(*modName);
						if (filterForm) {
							const auto formType = filterForm->GetFormType();
							if (const auto type = Cache::FormType::GetWhitelistFormString(formType); !type.empty()) {
								a_formVec.push_back(filterForm);
							} else {
								logger::error("			Filter ({}) SKIP - invalid formtype ({})", *modName, Cache::FormType::GetBlacklistFormString(formType));
							}
						} else {
							logger::error("			Filter ({}) SKIP - form doesn't exist", *modName);
						}
					}
				} else if (formID) {
					auto filterForm = modName ?
                                          a_dataHandler->LookupForm(*formID, *modName) :
                                          RE::TESForm::LookupByID(*formID);
					if (filterForm) {
						const auto formType = filterForm->GetFormType();
						if (const auto type = Cache::FormType::GetWhitelistFormString(formType); !type.empty()) {
							a_formVec.push_back(filterForm);
						} else {
							logger::error("			Filter [0x{:X}] ({}) SKIP - invalid formtype ({})", *formID, modName.value_or(""), Cache::FormType::GetBlacklistFormString(formType));
						}
					} else {
						logger::error("			Filter [0x{:X}] ({}) SKIP - form doesn't exist", *formID, modName.value_or(""));
					}
				}
			}
			return !a_formVec.empty();
		}
	}

	template <class Form>
	void get_forms(RE::TESDataHandler* a_dataHandler, const std::string& a_type, const INIDataMap& a_INIDataMap, FormMap<Form>& a_FormDataMap)
	{
		if (a_INIDataMap.empty()) {
			return;
		}

		logger::info("	Starting {} lookup", a_type);

		for (auto& [formOrEditorID, INIDataVec] : a_INIDataMap) {
			Form* form = nullptr;

			if (std::holds_alternative<FormIDPair>(formOrEditorID)) {
				if (auto [formID, modName] = std::get<FormIDPair>(formOrEditorID); formID) {
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
						logger::error("		{} [0x{:X}] ({}) FAIL - formID doesn't exist", a_type, *formID, modName.value_or(""));
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
							logger::info("		{} [0x{:X}] INFO - creating keyword", keywordName, keyword->GetFormID());

							keywordArray.push_back(keyword);

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
						logger::error("		{} {} FAIL - editorID doesn't exist", a_type, editorID);
					}
				}
			}

			if (!form) {
				continue;
			}

			std::vector<FormData> formDataVec;
			for (auto& [strings, filterIDs, level, gender, itemCount, chance] : INIDataVec) {
				bool invalidEntry = false;

				std::array<FormVec, 3> filterForms;
				for (std::uint32_t i = 0; i < 3; i++) {
					if (!detail::formID_to_form(a_dataHandler, filterIDs[i], filterForms[i])) {
						invalidEntry = true;
						break;
					}
				}

				if (!invalidEntry) {
					FormData formData{ strings, filterForms, level, gender, chance, itemCount };
					formDataVec.emplace_back(formData);
				}
			}

			a_FormDataMap.forms[form] = { 0, formDataVec };
		}
	}

	bool GetForms();
}
