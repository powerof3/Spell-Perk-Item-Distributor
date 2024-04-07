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
			const Path        path;

			UnknownPluginException(const std::string& modName, const Path& path) :
				modName(modName),
				path(path)
			{}
		};

		struct UnknownFormIDException : std::exception
		{
			const RE::FormID                 formID;
			const std::optional<std::string> modName;
			const Path                       path;

			UnknownFormIDException(RE::FormID formID, const Path& path, std::optional<std::string> modName = std::nullopt) :
				formID(formID),
				path(path),
				modName(modName)
			{}
		};

		/// <summary>
		/// An exception thrown when actual form's type does not match the form type excplicilty defined in the config.
		/// E.g. Spell = 0x12345, but the 0x12345 form is actually a Perk.
		/// </summary>
		struct MismatchingFormTypeException : std::exception
		{
			const RE::FormType   expectedFormType;
			const RE::FormType   actualFormType;
			const FormOrEditorID formOrEditorID;
			const Path           path;

			MismatchingFormTypeException(RE::FormType expectedFormType, RE::FormType actualFormType, const FormOrEditorID& formOrEditorID, const Path& path) :
				expectedFormType(expectedFormType),
				actualFormType(actualFormType),
				formOrEditorID(formOrEditorID),
				path(path)
			{}
		};

		struct InvalidKeywordException : std::exception
		{
			const RE::FormID                 formID;
			const std::optional<std::string> modName;
			const Path                       path;

			InvalidKeywordException(RE::FormID formID, const Path& path, std::optional<std::string> modName = std::nullopt) :
				formID(formID),
				modName(modName),
				path(path)
			{}
		};

		struct KeywordNotFoundException : std::exception
		{
			const std::string editorID;
			const bool        isDynamic;
			const Path        path;

			KeywordNotFoundException(const std::string& editorID, bool isDynamic, const Path& path) :
				editorID(editorID),
				isDynamic(isDynamic),
				path(path)
			{}
		};

		struct UnknownEditorIDException : std::exception
		{
			const std::string editorID;
			const Path        path;

			UnknownEditorIDException(const std::string& editorID, const Path& path) :
				editorID(editorID),
				path(path)
			{}
		};

		/// <summary>
		/// An exception thrown when actual form's type is not in the whitelist.
		/// </summary>
		struct InvalidFormTypeException : std::exception
		{
			const RE::FormType   formType;
			const FormOrEditorID formOrEditorID;
			const Path           path;

			InvalidFormTypeException(RE::FormType formType, const FormOrEditorID& formOrEditorID, const Path& path) :
				formType(formType),
				formOrEditorID(formOrEditorID),
				path(path)
			{}
		};

		struct MalformedEditorIDException : std::exception
		{
			const Path path;

			MalformedEditorIDException(const Path& path) :
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
		std::variant<Form*, const RE::TESFile*> get_form_or_mod(RE::TESDataHandler* const dataHandler, const FormOrEditorID& formOrEditorID, const Path& path, bool whitelistedOnly = false)
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
								   RE::TESForm* anyForm;
								   if (modName) {
									   anyForm = dataHandler->LookupForm(*formID, *modName);
								   } else {
									   anyForm = RE::TESForm::LookupByID(*formID);
								   }

								   if (!anyForm) {
									   throw UnknownFormIDException(*formID, path, modName);
								   }

								   form = as_form(anyForm);
								   if (!form) {
									   throw MismatchingFormTypeException(Form::FORMTYPE, anyForm->GetFormType(), FormModPair{ *formID, modName }, path);
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
							   if constexpr (std::is_same_v<Form, RE::BGSKeyword>) {
								   form = find_or_create_keyword(editorID);
							   } else {
								   if (const auto anyForm = RE::TESForm::LookupByEditorID(editorID); anyForm) {
									   form = as_form(anyForm);
									   if (!form) {
										   throw MismatchingFormTypeException(anyForm->GetFormType(), Form::FORMTYPE, editorID, path);
									   }
								   } else {
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
				if (FormType::GetWhitelisted(formType)) {
					return form;
				} else {
					throw InvalidFormTypeException(formType, formOrEditorID, path);
				}
			}

			return form;
		}

		inline const RE::TESFile* get_file(RE::TESDataHandler* const dataHandler, const FormOrEditorID& formOrEditorID, const Path& path)
		{
			auto formOrMod = get_form_or_mod(dataHandler, formOrEditorID, path);

			if (std::holds_alternative<const RE::TESFile*>(formOrMod)) {
				return std::get<const RE::TESFile*>(formOrMod);
			}

			return nullptr;
		}

		template <class Form = RE::TESForm>
		Form* get_form(RE::TESDataHandler* const dataHandler, const FormOrEditorID& formOrEditorID, const Path& path, bool whitelistedOnly = false)
		{
			auto formOrMod = get_form_or_mod<Form>(dataHandler, formOrEditorID, path, whitelistedOnly);

			if (std::holds_alternative<Form*>(formOrMod)) {
				return std::get<Form*>(formOrMod);
			}

			return nullptr;
		}

		inline bool formID_to_form(RE::TESDataHandler* const a_dataHandler, RawFormVec& a_rawFormVec, FormVec& a_formVec, const Path& a_path, bool a_all = false, bool whitelistedOnly = true)
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
				} catch (const Lookup::MismatchingFormTypeException& e) {
					std::visit(overload{
								   [&](const FormModPair& formMod) {
									   auto& [formID, modName] = formMod;
									   buffered_logger::error("\t\t[{}] Filter[0x{:X}] ({}) FAIL - mismatching form type (expected: {}, actual: {})", e.path, *formID, modName.value_or(""), e.expectedFormType, e.actualFormType);
								   },
								   [&](std::string editorID) {
									   buffered_logger::error("\t\t[{}] Filter ({}) FAIL - mismatching form type (expected: {}, actual: {})", e.path, editorID, e.expectedFormType, e.actualFormType);
								   } },
						e.formOrEditorID);
				} catch (const InvalidFormTypeException& e) {
					std::visit(overload{
								   [&](const FormModPair& formMod) {
									   auto& [formID, modName] = formMod;
									   buffered_logger::error("\t\t[{}] Filter [0x{:X}] ({}) SKIP - invalid formtype ({})", e.path, *formID, modName.value_or(""), e.formType);
								   },
								   [&](std::string editorID) {
									   buffered_logger::error("\t\t[{}] Filter ({}) SKIP - invalid formtype ({})", e.path, editorID, e.formType);
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

		Path          path{};
		std::uint32_t npcCount{ 0 };

		bool operator==(const Data& a_rhs) const;
	};

	template <class Form>
	using DataVec = std::vector<Data<Form>>;

	using DistributedForm = std::pair<RE::TESForm*, const Path>;
	using DistributedForms = std::set<DistributedForm>;

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

		template <typename Form>
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

		std::size_t GetLookupCount() const;

		RECORD::TYPE GetType() const;

		DataVec<Form>& GetForms(bool a_onlyLevelEntries);
		DataVec<Form>& GetForms();

		void LookupForms(RE::TESDataHandler*, std::string_view a_type, INI::DataVec&);
		void EmplaceForm(bool isValid, Form*, const IndexOrCount&, const FilterData&, const Path&);

		// Init formsWithLevels and formsNoLevels
		void FinishLookupForms();

	private:
		RECORD::TYPE  type;
		DataVec<Form> forms{};
		DataVec<Form> formsWithLevels{};

		/// Total number of entries that were matched to this Distributable, including invalid.
		/// This counter is used for logging purposes.
		std::size_t lookupCount{ 0 };

		void LookupForm(RE::TESDataHandler*, INI::Data&);
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
		a_func(factions, std::forward<Args>(args)...);
		a_func(perks, std::forward<Args>(args)...);
		a_func(spells, std::forward<Args>(args)...);
		a_func(levSpells, std::forward<Args>(args)...);
		a_func(shouts, std::forward<Args>(args)...);
		a_func(items, std::forward<Args>(args)...);
		a_func(deathItems, std::forward<Args>(args)...);
		a_func(outfits, std::forward<Args>(args)...);
		a_func(sleepOutfits, std::forward<Args>(args)...);
		a_func(packages, std::forward<Args>(args)...);
		a_func(skins, std::forward<Args>(args)...);
	}

	/// <summary>
	/// Performs lookup for a single entry.
	/// It's up to the callee to add the form to the appropriate distributable container.
	/// </summary>
	/// <typeparam name="Form">Type of the form to lookup. This type can be defaulted to accept any TESForm.</typeparam>
	/// <param name="dataHandler"></param>
	/// <param name="rawForm">A raw form entry that needs to be looked up.</param>
	/// <param name="callback">A callback to be called with validated data after successful lookup.</param>
	template <class Form = RE::TESForm*>
	void LookupGenericForm(RE::TESDataHandler* const dataHandler, INI::Data& rawForm, std::function<void(bool isValid, Form*, const IndexOrCount&, const FilterData&, const Path& path)> callback);
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
std::size_t Forms::Distributables<Form>::GetLookupCount() const
{
	return lookupCount;
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
void Forms::Distributables<Form>::LookupForm(RE::TESDataHandler* dataHandler, INI::Data& rawForm)
{
	Forms::LookupGenericForm<Form>(dataHandler, rawForm, [&](bool isValid, Form* form, const auto& idxOrCount, const auto& filters, const auto& path) {
		EmplaceForm(isValid, form, idxOrCount, filters, path);
	});
}

template <class Form>
void Forms::Distributables<Form>::LookupForms(RE::TESDataHandler* dataHandler, std::string_view a_type, INI::DataVec& a_INIDataVec)
{
	if (a_INIDataVec.empty()) {
		return;
	}

	logger::info("{}", a_type);

	forms.reserve(a_INIDataVec.size());

	for (auto& rawForm : a_INIDataVec) {
		LookupForm(dataHandler, rawForm);
	}
}

template <class Form>
void Forms::Distributables<Form>::EmplaceForm(bool isValid, Form* form, const IndexOrCount& idxOrCount, const FilterData& filters, const Path& path)
{
	if (isValid) {
		forms.emplace_back(forms.size(), form, idxOrCount, filters, path);
	}
	lookupCount++;
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

template <class Form>
void Forms::LookupGenericForm(RE::TESDataHandler* const dataHandler, INI::Data& rawForm, std::function<void(bool isValid, Form*, const IndexOrCount&, const FilterData&, const Path& path)> callback)
{
	auto& [formOrEditorID, strings, filterIDs, level, traits, idxOrCount, chance, path] = rawForm;

	try {
		if (auto form = detail::get_form<Form>(dataHandler, formOrEditorID, path); form) {
			FormFilters filterForms{};

			bool validEntry = detail::formID_to_form(dataHandler, filterIDs.ALL, filterForms.ALL, path, true);
			if (validEntry) {
				validEntry = detail::formID_to_form(dataHandler, filterIDs.NOT, filterForms.NOT, path);
			}
			if (validEntry) {
				validEntry = detail::formID_to_form(dataHandler, filterIDs.MATCH, filterForms.MATCH, path);
			}

			FilterData filters{ strings, filterForms, level, traits, chance };
			callback(validEntry, form, idxOrCount, filters, path);
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
	} catch (const Lookup::MismatchingFormTypeException& e) {
		std::visit(overload{
					   [&](const FormModPair& formMod) {
						   auto& [formID, modName] = formMod;
						   buffered_logger::error("\t\t[{}] [0x{:X}] ({}) FAIL - mismatching form type (expected: {}, actual: {})", e.path, *formID, modName.value_or(""), e.expectedFormType, e.actualFormType);
					   },
					   [&](std::string editorID) {
						   buffered_logger::error("\t\t[{}] ({}) FAIL - mismatching form type (expected: {}, actual: {})", e.path, editorID, e.expectedFormType, e.actualFormType);
					   } },
			e.formOrEditorID);
	} catch (const Lookup::InvalidFormTypeException& e) {
		// Whitelisting is disabled, so this should not occur
	} catch (const Lookup::UnknownPluginException& e) {
		// Likewise, we don't expect plugin names in distributable forms.
	}
}

inline std::ostream& operator<<(std::ostream& os, Forms::DistributedForm form)
{
	os << form.first;

	if (!form.second.empty()) {
		os << " @" << form.second;
	}

	return os;
}
