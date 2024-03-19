#pragma once

#include "LookupConfigs.h"
#include "LookupFilters.h"

namespace Forms
{
	namespace Lookup
	{
		struct UnknownPluginException : std::exception
		{
			const std::string modName;
			const std::string path;

			UnknownPluginException(const std::string& modName, const std::string& path) :
				modName(modName),
				path(path)
			{}
		};

		struct UnknownFormIDException : std::exception
		{
			const RE::FormID                 formID;
			const std::optional<std::string> modName;
			const std::string                path;

			UnknownFormIDException(RE::FormID formID, const std::string& path, std::optional<std::string> modName = std::nullopt) :
				formID(formID),
				path(path),
				modName(modName)
			{}
		};

		struct InvalidKeywordException : std::exception
		{
			const RE::FormID                 formID;
			const std::optional<std::string> modName;
			const std::string                path;

			InvalidKeywordException(RE::FormID formID, const std::string& path, std::optional<std::string> modName = std::nullopt) :
				formID(formID),
				modName(modName),
				path(path)
			{}
		};

		struct KeywordNotFoundException : std::exception
		{
			const std::string editorID;
			const bool        isDynamic;
			const std::string path;

			KeywordNotFoundException(const std::string& editorID, bool isDynamic, const std::string& path) :
				editorID(editorID),
				isDynamic(isDynamic),
				path(path)
			{}
		};

		struct UnknownEditorIDException : std::exception
		{
			const std::string editorID;
			const std::string path;

			UnknownEditorIDException(const std::string& editorID, const std::string& path) :
				editorID(editorID),
				path(path)
			{}
		};

		struct InvalidFormTypeException : std::exception
		{
			const RE::FormType   formType;
			const FormOrEditorID formOrEditorID;
			const std::string    path;

			InvalidFormTypeException(RE::FormType formType, const FormOrEditorID& formOrEditorID, const std::string& path) :
				formType(formType),
				formOrEditorID(formOrEditorID),
				path(path)
			{}
		};

		struct MalformedEditorIDException : std::exception
		{
			const std::string path;

			MalformedEditorIDException(const std::string& path) :
				path(path)
			{}
		};
	}

	namespace detail
	{
		using namespace Lookup;

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

		template <class Form = RE::TESForm>
		std::variant<Form*, const RE::TESFile*> get_form_or_mod(RE::TESDataHandler* dataHandler, const FormOrEditorID& formOrEditorID, const std::string& path, bool whitelistedOnly = false)
		{
			Form*              form = nullptr;
			const RE::TESFile* mod = nullptr;

			constexpr auto as_form = [](RE::TESForm* anyForm) -> Form* {
				if (!anyForm) {
					return nullptr;
				}
				if constexpr (std::is_same_v<Form, RE::TESForm>) {
					return anyForm;
				} else if constexpr (std::is_same_v<Form, RE::TESBoundObject>) {
					return static_cast<Form*>(anyForm);
				} else {
					return anyForm->As<Form>();
				}
			};

			auto find_or_create_keyword = [&](const std::string& editorID) -> RE::BGSKeyword* {
				auto& keywordArray = dataHandler->GetFormArray<RE::BGSKeyword>();

				auto result = std::find_if(keywordArray.begin(), keywordArray.end(), [&](const auto& keyword) {
					return keyword && keyword->formEditorID == editorID.c_str();
				});

				if (result != keywordArray.end()) {
					if (const auto keyword = *result; keyword) {
						return keyword;
					} else {
						throw KeywordNotFoundException(editorID, false, path);
					}
				} else {
					const auto factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSKeyword>();
					if (auto keyword = factory ? factory->Create() : nullptr; keyword) {
						keyword->formEditorID = editorID;
						keywordArray.push_back(keyword);

						return keyword;
					} else {
						throw KeywordNotFoundException(editorID, true, path);
					}
				}
			};

			std::visit(overload{
						   [&](const FormModPair& formMod) {
							   auto [formID, modName] = formMod;

							   // Only MyPlugin.esp
							   if (modName && !formID) {
								   if (const RE::TESFile* filterMod = dataHandler->LookupModByName(*modName); filterMod) {
									   mod = filterMod;
									   return;
								   } else {
									   throw UnknownPluginException(*modName, path);
								   }
							   }

							   if (formID && g_mergeMapperInterface) {
								   get_merged_IDs(formID, modName);
							   }

							   // Either 0x1235 or 0x1235~MyPlugin.esp
							   if (formID) {
								   if (modName) {
									   form = as_form(dataHandler->LookupForm(*formID, *modName));
								   } else {
									   form = as_form(RE::TESForm::LookupByID(*formID));
								   }
								   if (!form) {
									   throw UnknownFormIDException(*formID, path, modName);
								   }

								   if constexpr (std::is_same_v<Form, RE::BGSKeyword>) {
									   if (string::is_empty(form->GetFormEditorID())) {
										   // Keywords with empty EditorIDs cause game to crash.
										   throw InvalidKeywordException(*formID, path, modName);
									   }
								   }
							   }
						   },
						   [&](const std::string& editorID) {
							   if (editorID.empty()) {
								   throw MalformedEditorIDException(path);
							   }
							   //
							   if constexpr (std::is_same_v<Form, RE::BGSKeyword>) {
								   form = find_or_create_keyword(editorID);
							   } else {
								   form = as_form(RE::TESForm::LookupByEditorID(editorID));
								   if (!form) {
									   if constexpr (std::is_same_v<Form, RE::TESForm>) {
										   form = find_or_create_keyword(editorID);
									   } else {
										   throw UnknownEditorIDException(editorID, path);
									   }
								   }
							   }
						   } },
				formOrEditorID);

			if (mod) {
				return mod;
			}

			if (whitelistedOnly && form) {
				const auto formType = form->GetFormType();
				if (Cache::FormType::GetWhitelisted(formType)) {
					return form;
				} else {
					throw InvalidFormTypeException(formType, formOrEditorID, path);
				}
			}

			return form;
		}

		inline const RE::TESFile* get_file(RE::TESDataHandler* dataHandler, const FormOrEditorID& formOrEditorID, const std::string& path)
		{
			auto formOrMod = get_form_or_mod(dataHandler, formOrEditorID, path);

			if (std::holds_alternative<const RE::TESFile*>(formOrMod)) {
				return std::get<const RE::TESFile*>(formOrMod);
			}

			return nullptr;
		}

		template <class Form = RE::TESForm>
		Form* get_form(RE::TESDataHandler* dataHandler, const FormOrEditorID& formOrEditorID, const std::string& path, bool whitelistedOnly = false)
		{
			auto formOrMod = get_form_or_mod<Form>(dataHandler, formOrEditorID, path, whitelistedOnly);

			if (std::holds_alternative<Form*>(formOrMod)) {
				return std::get<Form*>(formOrMod);
			}

			return nullptr;
		}

		inline bool formID_to_form(RE::TESDataHandler* a_dataHandler, RawFormVec& a_rawFormVec, FormVec& a_formVec, const std::string& a_path, bool a_all = false, bool whitelistedOnly = true)
		{
			if (a_rawFormVec.empty()) {
				return true;
			}

			for (auto& formOrEditorID : a_rawFormVec) {
				try {
					auto form = get_form_or_mod(a_dataHandler, formOrEditorID, a_path, whitelistedOnly);
					a_formVec.emplace_back(form);
				} catch (const UnknownFormIDException& e) {
					buffered_logger::error("\t\t[{}] Filter [0x{:X}] ({}) SKIP - formID doesn't exist", e.path, e.formID, e.modName.value_or(""));
				} catch (const UnknownPluginException& e) {
					buffered_logger::error("\t\t[{}] Filter ({}) SKIP - mod cannot be found", e.path, e.modName);
				} catch (const InvalidKeywordException& e) {
					buffered_logger::error("\t\t[{}] Filter [0x{:X}] ({}) SKIP - keyword does not have a valid editorID", e.path, e.formID, e.modName.value_or(""));
				} catch (const KeywordNotFoundException& e) {
					if (e.isDynamic) {
						buffered_logger::critical("\t\t[{}] {} FAIL - couldn't create keyword", e.path, e.editorID);
					} else {
						buffered_logger::critical("\t\t[{}] {} FAIL - couldn't get existing keyword", e.path, e.editorID);
					}
					return false;
				} catch (const UnknownEditorIDException& e) {
					buffered_logger::error("\t\t[{}] Filter ({}) SKIP - editorID doesn't exist", e.path, e.editorID);
				} catch (const MalformedEditorIDException& e) {
					buffered_logger::error("\t\t[{}] Filter (\"\") SKIP - malformed editorID", e.path);
				} catch (const InvalidFormTypeException& e) {
					std::visit(overload{
								   [&](const FormModPair& formMod) {
									   auto& [formID, modName] = formMod;
									   buffered_logger::error("\t\t[{}] Filter [0x{:X}] ({}) SKIP - invalid formtype ({})", e.path, *formID, modName.value_or(""), RE::FormTypeToString(e.formType));
								   },
								   [&](std::string editorID) {
									   buffered_logger::error("\t\t[{}] Filter ({}) SKIP - invalid formtype ({})", e.path, editorID, RE::FormTypeToString(e.formType));
								   } },
						e.formOrEditorID);
				}
			}

			return !a_all && !a_formVec.empty() || a_formVec.size() == a_rawFormVec.size();
		}
	}

	template <class Form>
	struct Data
	{
		std::uint32_t index{ 0 };

		Form*        form{ nullptr };
		IndexOrCount idxOrCount{ RandomCount(1, 1) };
		FilterData   filters{};

		std::string   path{};
		std::uint32_t npcCount{ 0 };

		bool operator==(const Data& a_rhs) const;
	};

	template <class Form>
	using DataVec = std::vector<Data<Form>>;

	/// <summary>
	/// A set of distributable forms that should be processed.
	///
	/// DistributionSet is used to conveniently pack all distributable forms into one structure.
	/// Note that all entries store references so they are not owned by this structure. 
	/// If you want to omit certain type of entries, you can use static empty() method to get a reference to an empty container.
	/// </summary>
	struct DistributionSet
	{
		DataVec<RE::SpellItem>&      spells;
		DataVec<RE::BGSPerk>&        perks;
		DataVec<RE::TESBoundObject>& items;
		DataVec<RE::TESShout>&       shouts;
		DataVec<RE::TESLevSpell>&    levSpells;
		DataVec<RE::TESForm>&        packages;
		DataVec<RE::BGSOutfit>&      outfits;
		DataVec<RE::BGSKeyword>&     keywords;
		DataVec<RE::TESBoundObject>& deathItems;
		DataVec<RE::TESFaction>&     factions;
		DataVec<RE::BGSOutfit>&      sleepOutfits;
		DataVec<RE::TESObjectARMO>&  skins;

		bool IsEmpty() const;

		template<typename Form>
		static DataVec<Form>& empty()
		{
			static DataVec<Form> empty{};
			return empty;
		}
	};

	/// <summary>
	/// A container that holds distributable entries for a single form type.
	///
	/// Note that this container tracks separately leveled (those using level in their filters) entries.
	/// </summary>
	/// <typeparam name="Form">Type of the forms to store.</typeparam>
	template <class Form>
	struct Distributables
	{
		Distributables(RECORD::TYPE a_type) :
			type(a_type)
		{}

		explicit operator bool();

		std::size_t GetSize() const;
		std::size_t GetLeveledSize() const;

		RECORD::TYPE GetType() const;

		DataVec<Form>& GetForms(bool a_onlyLevelEntries);
		DataVec<Form>& GetForms();

		void LookupForms(RE::TESDataHandler* a_dataHandler, std::string_view a_type, INI::DataVec& a_INIDataVec);

		// Init formsWithLevels and formsNoLevels
		void FinishLookupForms();

	private:
		RECORD::TYPE  type;
		DataVec<Form> forms{};
		DataVec<Form> formsWithLevels{};
	};

	inline Distributables<RE::SpellItem>      spells{ RECORD::kSpell };
	inline Distributables<RE::BGSPerk>        perks{ RECORD::kPerk };
	inline Distributables<RE::TESBoundObject> items{ RECORD::kItem };
	inline Distributables<RE::TESShout>       shouts{ RECORD::kShout };
	inline Distributables<RE::TESLevSpell>    levSpells{ RECORD::kLevSpell };
	inline Distributables<RE::TESForm>        packages{ RECORD::kPackage };
	inline Distributables<RE::BGSOutfit>      outfits{ RECORD::kOutfit };
	inline Distributables<RE::BGSKeyword>     keywords{ RECORD::kKeyword };
	inline Distributables<RE::TESBoundObject> deathItems{ RECORD::kDeathItem };
	inline Distributables<RE::TESFaction>     factions{ RECORD::kFaction };
	inline Distributables<RE::BGSOutfit>      sleepOutfits{ RECORD::kSleepOutfit };
	inline Distributables<RE::TESObjectARMO>  skins{ RECORD::kSkin };

	std::size_t GetTotalEntries();
	std::size_t GetTotalLeveledEntries();

	template <typename Func, typename... Args>
	void ForEachDistributable(Func&& a_func, Args&&... args)
	{
		a_func(keywords, std::forward<Args>(args)...);
		a_func(spells, std::forward<Args>(args)...);
		a_func(levSpells, std::forward<Args>(args)...);
		a_func(perks, std::forward<Args>(args)...);
		a_func(shouts, std::forward<Args>(args)...);
		a_func(items, std::forward<Args>(args)...);
		a_func(deathItems, std::forward<Args>(args)...);
		a_func(outfits, std::forward<Args>(args)...);
		a_func(sleepOutfits, std::forward<Args>(args)...);
		a_func(factions, std::forward<Args>(args)...);
		a_func(packages, std::forward<Args>(args)...);
		a_func(skins, std::forward<Args>(args)...);
	}
}

template <class Form>
bool Forms::Data<Form>::operator==(const Data& a_rhs) const
{
	if (!form || !a_rhs.form) {
		return false;
	}
	return form->GetFormID() == a_rhs.form->GetFormID();
}

template <class Form>
Forms::Distributables<Form>::operator bool()
{
	return !forms.empty();
}

template <class Form>
std::size_t Forms::Distributables<Form>::GetSize() const
{
	return forms.size();
}

template <class Form>
std::size_t Forms::Distributables<Form>::GetLeveledSize() const
{
	return formsWithLevels.size();
}

template <class Form>
RECORD::TYPE Forms::Distributables<Form>::GetType() const
{
	return type;
}

template <class Form>
Forms::DataVec<Form>& Forms::Distributables<Form>::GetForms()
{
	return forms;
}

template <class Form>
Forms::DataVec<Form>& Forms::Distributables<Form>::GetForms(bool a_onlyLevelEntries)
{
	if (a_onlyLevelEntries) {
		return formsWithLevels;
	}
	return forms;
}

template <class Form>
void Forms::Distributables<Form>::LookupForms(RE::TESDataHandler* a_dataHandler, std::string_view a_type, INI::DataVec& a_INIDataVec)
{
	if (a_INIDataVec.empty()) {
		return;
	}

	logger::info("{}", a_type);

	forms.reserve(a_INIDataVec.size());
	std::uint32_t index = 0;

	for (auto& [formOrEditorID, strings, filterIDs, level, traits, idxOrCount, chance, path] : a_INIDataVec) {
		try {
			if (auto form = detail::get_form<Form>(a_dataHandler, formOrEditorID, path); form) {
				FormFilters filterForms{};

				bool validEntry = detail::formID_to_form(a_dataHandler, filterIDs.ALL, filterForms.ALL, path, true);
				if (validEntry) {
					validEntry = detail::formID_to_form(a_dataHandler, filterIDs.NOT, filterForms.NOT, path);
				}
				if (validEntry) {
					validEntry = detail::formID_to_form(a_dataHandler, filterIDs.MATCH, filterForms.MATCH, path);
				}

				if (validEntry) {
					forms.emplace_back(index, form, idxOrCount, FilterData(strings, filterForms, level, traits, chance), path);
					index++;
				}
			}
		} catch (const Lookup::UnknownFormIDException& e) {
			buffered_logger::error("\t[{}] [0x{:X}] ({}) FAIL - formID doesn't exist", e.path, e.formID, e.modName.value_or(""));
		} catch (const Lookup::InvalidKeywordException& e) {
			buffered_logger::error("\t[{}] [0x{:X}] ({}) FAIL - keyword does not have a valid editorID", e.path, e.formID, e.modName.value_or(""));
		} catch (const Lookup::KeywordNotFoundException& e) {
			if (e.isDynamic) {
				buffered_logger::critical("\t[{}] {} FAIL - couldn't create keyword", e.path, e.editorID);
			} else {
				buffered_logger::critical("\t[{}] {} FAIL - couldn't get existing keyword", e.path, e.editorID);
			}
		} catch (const Lookup::UnknownEditorIDException& e) {
			buffered_logger::error("\t[{}] ({}) FAIL - editorID doesn't exist", e.path, e.editorID);
		} catch (const Lookup::MalformedEditorIDException& e) {
			buffered_logger::error("\t[{}] FAIL - editorID can't be empty", e.path);
		} catch (const Lookup::InvalidFormTypeException& e) {
			// Whitelisting is disabled, so this should not occur
		} catch (const Lookup::UnknownPluginException& e) {
			// Likewise, we don't expect plugin names in distributable forms.
		}
	}
}

template <class Form>
void Forms::Distributables<Form>::FinishLookupForms()
{
	if (forms.empty()) {
		return;
	}

	// reverse overridable form vectors, winning configs first (Zzzz -> Aaaa)
	// entry order within config is preserved
	// thanks, chatGPT!
	if constexpr (std::is_same_v<RE::BGSOutfit, Form> || std::is_same_v<RE::TESObjectARMO, Form>) {
		std::stable_sort(forms.begin(), forms.end(), [](const auto& a_form1, const auto& a_form2) {
			return a_form1.path > a_form2.path;  // Compare paths in reverse order
		});
	}

	formsWithLevels.reserve(forms.size());

	std::copy_if(forms.begin(), forms.end(),
		std::back_inserter(formsWithLevels),
		[](const auto& formData) { return formData.filters.HasLevelFilters(); });
}
