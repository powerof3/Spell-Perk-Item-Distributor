#pragma once

#include "Cache.h"
#include "Defs.h"

namespace INI
{
	namespace detail
	{
		inline std::vector<std::string> split_sub_string(const std::string& a_str, const std::string& a_delimiter = ",")
		{
			if (!a_str.empty() && a_str.find("NONE"sv) == std::string::npos) {
				return string::split(a_str, a_delimiter);
			}
			return std::vector<std::string>();
		}

		inline bool is_mod_name(const std::string& a_str)
		{
			return a_str.rfind(".esp") != std::string::npos || a_str.rfind(".esl") != std::string::npos || a_str.rfind(".esm ") != std::string::npos;
		}

		inline FormIDPair get_formID(const std::string& a_str)
		{
			if (a_str.find("~"sv) != std::string::npos) {
				auto splitID = string::split(a_str, "~");
				return std::make_pair(
					string::lexical_cast<RE::FormID>(splitID.at(kFormID), true),
					splitID.at(kESP));
			} else if (is_mod_name(a_str) || !string::is_only_hex(a_str)) {
				return std::make_pair(
					std::nullopt,
					a_str);
			} else {
				return std::make_pair(
					string::lexical_cast<RE::FormID>(a_str, true),
					std::nullopt);
			}
		}

		inline std::string sanitize(const std::string& a_value)
		{
			auto newValue = a_value;

			//formID hypen
			if (newValue.find('~') == std::string::npos) {
				string::replace_first_instance(newValue, " - ", "~");
			}

			//strip spaces between " | "
			static const boost::regex re_bar(R"(\s*\|\s*)", boost::regex_constants::optimize);
			newValue = regex_replace(newValue, re_bar, "|");

			//strip spaces between " , "
			static const boost::regex re_comma(R"(\s*,\s*)", boost::regex_constants::optimize);
			newValue = regex_replace(newValue, re_comma, ",");

			//strip leading zeros
			static const boost::regex re_zeros(R"((0x00+)([0-9a-fA-F]+))", boost::regex_constants::optimize);
			newValue = regex_replace(newValue, re_zeros, "0x$2");

			//NOT to hyphen
			string::replace_all(newValue, "NOT ", "-");

			return newValue;
		}
	}

	inline std::pair<INIData, std::optional<std::string>> parse_ini(const std::string& a_value)
	{
		INIData data;
		auto& [formIDPair_ini, strings_ini, filterIDs_ini, level_ini, traits_ini, itemCount_ini, chance_ini] = data;

		auto sanitized_value = detail::sanitize(a_value);

		const auto sections = string::split(sanitized_value, "|");
		const auto size = sections.size();

		//[FORMID/ESP] / string
		std::variant<FormIDPair, std::string> item_ID;
		try {
			auto& formSection = sections.at(kFormID);
			if (formSection.find('~') != std::string::npos || string::is_only_hex(formSection)) {
				FormIDPair pair;
				pair.second = std::nullopt;

				if (string::is_only_hex(formSection)) {
					// formID
					pair.first = string::lexical_cast<RE::FormID>(formSection, true);
				} else {
					// formID~esp
					pair = detail::get_formID(formSection);
				}

				item_ID.emplace<FormIDPair>(pair);
			} else {
				item_ID.emplace<std::string>(formSection);
			}
		} catch (...) {
			FormIDPair pair = { 0, std::nullopt };
			item_ID.emplace<FormIDPair>(pair);
		}
		formIDPair_ini = item_ID;

		//KEYWORDS
		try {
			auto& [strings_ALL, strings_NOT, strings_MATCH, strings_ANY] = strings_ini;

			auto split_str = detail::split_sub_string(sections.at(kStrings));
			for (auto& str : split_str) {
				if (str.find("+"sv) != std::string::npos) {
					auto strings = detail::split_sub_string(str, "+");
					strings_ALL.insert(strings_ALL.end(), strings.begin(), strings.end());

				} else if (str.at(0) == '-') {
					str.erase(0, 1);
					strings_NOT.emplace_back(str);

				} else if (str.at(0) == '*') {
					str.erase(0, 1);
					strings_ANY.emplace_back(str);

				} else {
					strings_MATCH.emplace_back(str);
				}
			}
		} catch (...) {
		}

		//FILTER FORMS
		try {
			auto& [filterIDs_ALL, filterIDs_NOT, filterIDs_MATCH] = filterIDs_ini;

			auto split_IDs = detail::split_sub_string(sections.at(kFilterIDs));
			for (auto& IDs : split_IDs) {
				if (IDs.find("+"sv) != std::string::npos) {
					auto splitIDs_ALL = detail::split_sub_string(IDs, "+");
					for (auto& IDs_ALL : splitIDs_ALL) {
						filterIDs_ALL.push_back(detail::get_formID(IDs_ALL));
					}
				} else if (IDs.at(0) == '-') {
					IDs.erase(0, 1);
					filterIDs_NOT.push_back(detail::get_formID(IDs));

				} else {
					filterIDs_MATCH.push_back(detail::get_formID(IDs));
				}
			}
		} catch (...) {
			logger::info("caught filter");
		}

		//LEVEL
		ActorLevel actorLevelPair = { UINT16_MAX, UINT16_MAX };
		std::vector<SkillLevel> skillLevelPairs;
		try {
			auto split_levels = detail::split_sub_string(sections.at(kLevel));
			for (auto& levels : split_levels) {
				if (levels.find('(') != std::string::npos) {
					//skill(min/max)
					auto sanitizedLevel = string::remove_non_alphanumeric(levels);
					auto skills = string::split(sanitizedLevel, " ");
					//skill min max
					if (!skills.empty()) {
						if (skills.size() > 2) {
							auto type = string::lexical_cast<std::uint32_t>(skills.at(0));
							auto minLevel = string::lexical_cast<std::uint8_t>(skills.at(1));
							auto maxLevel = string::lexical_cast<std::uint8_t>(skills.at(2));

							skillLevelPairs.push_back({ type, { minLevel, maxLevel } });
						} else {
							auto type = string::lexical_cast<std::uint32_t>(skills.at(0));
							auto minLevel = string::lexical_cast<std::uint8_t>(skills.at(1));

							skillLevelPairs.push_back({ type, { minLevel, UINT8_MAX } });
						}
					}
				} else {
					auto split_level = string::split(levels, "/");
					if (split_level.size() > 1) {
						auto minLevel = string::lexical_cast<std::uint16_t>(split_level.at(0));
						auto maxLevel = string::lexical_cast<std::uint16_t>(split_level.at(1));

						actorLevelPair = { minLevel, maxLevel };
					} else {
						auto level = string::lexical_cast<std::uint16_t>(levels);

						actorLevelPair = { level, UINT16_MAX };
					}
				}
			}
		} catch (...) {
		}
		level_ini = { actorLevelPair, skillLevelPairs };

		//TRAITS
		try {
			auto& [sex, unique, summonable] = traits_ini;

			auto split_traits = detail::split_sub_string(sections.at(kTraits), "/");
			for (auto& trait : split_traits) {
				if (trait == "M") {
					sex = RE::SEX::kMale;
				} else if (trait == "F") {
					sex = RE::SEX::kFemale;
				} else if (trait == "U") {
					unique = true;
				} else if (trait == "-U") {
					unique = false;
				} else if (trait == "S") {
					summonable = true;
				} else if (trait == "-S") {
					summonable = false;
				}
			}
		} catch (...) {
		}

		//ITEMCOUNT
		itemCount_ini = 1;
		try {
			const auto& itemCountStr = sections.at(kItemCount);
			if (!itemCountStr.empty() && itemCountStr.find("NONE"sv) == std::string::npos) {
				itemCount_ini = string::lexical_cast<std::int32_t>(itemCountStr);
			}
		} catch (...) {
		}

		//CHANCE
		chance_ini = 100;
		try {
			const auto& chanceStr = sections.at(kChance);
			if (!chanceStr.empty() && chanceStr.find("NONE"sv) == std::string::npos) {
				chance_ini = string::lexical_cast<float>(chanceStr);
			}
		} catch (...) {
		}

		if (sanitized_value != a_value) {
			return std::make_pair(data, sanitized_value);
		}
		return std::make_pair(data, std::nullopt);
	}

	bool Read();
}

namespace Lookup
{
	namespace detail
	{
		inline void formID_to_form(RE::TESDataHandler* a_dataHandler, const FormIDPairVec& a_formIDVec, FormVec& a_formVec)
		{
			if (!a_formIDVec.empty()) {
				for (auto& [formID, modName] : a_formIDVec) {
					if (modName && !formID) {
						if (INI::detail::is_mod_name(*modName)) {
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
			}
		}
	}

	template <class Form>
	void get_forms(RE::TESDataHandler* a_dataHandler, const std::string& a_type, const INIDataVec& a_INIDataVec, FormDataVec<Form>& a_formDataVec)
	{
		if (a_INIDataVec.empty()) {
			return;
		}

		logger::info("	Starting {} lookup", a_type);

		for (auto& [formIDPair_ini, strings_ini, filterIDs_ini, level_ini, gender_ini, itemCount_ini, chance_ini] : a_INIDataVec) {
			Form* form = nullptr;

			if (std::holds_alternative<FormIDPair>(formIDPair_ini)) {
				if (auto [formID, modName] = std::get<FormIDPair>(formIDPair_ini); formID) {
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
				if (!std::holds_alternative<std::string>(formIDPair_ini)) {
					continue;
				}

				if (auto keywordName = std::get<std::string>(formIDPair_ini); !keywordName.empty()) {
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
			} else if (std::holds_alternative<std::string>(formIDPair_ini)) {
				if (auto editorID = std::get<std::string>(formIDPair_ini); !editorID.empty()) {
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

			std::array<FormVec, 3> filterForms;
			for (std::uint32_t i = 0; i < 3; i++) {
				detail::formID_to_form(a_dataHandler, filterIDs_ini[i], filterForms[i]);
			}

			FormCountPair<Form> formCountPair = { form, itemCount_ini };
			std::uint32_t npcCount = 0;

			FormData<Form> formData = { formCountPair, strings_ini, filterForms, level_ini, gender_ini, chance_ini, npcCount };
			a_formDataVec.emplace_back(formData);
		}
	}

	bool GetForms();
}

namespace Filter
{
	namespace detail
	{
		namespace name
		{
			inline bool contains(const std::string& a_name, const StringVec& a_strings)
			{
				return std::ranges::any_of(a_strings, [&](const auto& str) {
					return string::icontains(a_name, str);
				});
			}

			inline bool matches(const std::string& a_name, const StringVec& a_strings)
			{
				return std::ranges::any_of(a_strings, [&](const auto& str) {
					return string::iequals(a_name, str);
				});
			}
		}

		namespace form
		{
			inline bool get_type(RE::TESNPC& a_actorbase, RE::TESForm* a_filter)
			{
				switch (a_filter->GetFormType()) {
				case RE::FormType::CombatStyle:
					return a_actorbase.GetCombatStyle() == a_filter;
				case RE::FormType::Class:
					return a_actorbase.npcClass == a_filter;
				case RE::FormType::Faction:
					{
						const auto faction = static_cast<RE::TESFaction*>(a_filter);
						return faction && a_actorbase.IsInFaction(faction);
					}
				case RE::FormType::Race:
					return a_actorbase.GetRace() == a_filter;
				case RE::FormType::Outfit:
					return a_actorbase.defaultOutfit == a_filter;
				case RE::FormType::NPC:
					return &a_actorbase == a_filter;
				case RE::FormType::VoiceType:
					return a_actorbase.voiceType == a_filter;
				default:
					return false;
				}
			}

			inline bool matches(RE::TESNPC& a_actorbase, const FormVec& a_forms)
			{
				return std::ranges::any_of(a_forms, [&a_actorbase](const auto& a_formFile) {
					if (std::holds_alternative<RE::TESForm*>(a_formFile)) {
						auto form = std::get<RE::TESForm*>(a_formFile);
						return form && get_type(a_actorbase, form);
					} else if (std::holds_alternative<const RE::TESFile*>(a_formFile)) {
						auto file = std::get<const RE::TESFile*>(a_formFile);
						return file && file->IsFormInMod(a_actorbase.GetFormID());
					}
					return false;
				});
			}

			inline bool matches_ALL(RE::TESNPC& a_actorbase, const FormVec& a_forms)
			{
				return std::ranges::all_of(a_forms, [&a_actorbase](const auto& a_formFile) {
					if (std::holds_alternative<RE::TESForm*>(a_formFile)) {
						auto form = std::get<RE::TESForm*>(a_formFile);
						return form && get_type(a_actorbase, form);
					} else if (std::holds_alternative<const RE::TESFile*>(a_formFile)) {
						auto file = std::get<const RE::TESFile*>(a_formFile);
						return file && file->IsFormInMod(a_actorbase.GetFormID());
					}
					return false;
				});
			}
		}

		namespace keyword
		{
			inline bool contains(RE::TESNPC& a_actorbase, const StringVec& a_strings)
			{
				return std::ranges::any_of(a_strings, [&a_actorbase](const auto& str) {
					return a_actorbase.ContainsKeyword(str);
				});
			}

			inline bool matches(RE::TESNPC& a_actorbase, const StringVec& a_strings, bool a_matchesAll = false)
			{
				if (a_matchesAll) {
					return std::ranges::all_of(a_strings, [&a_actorbase](const auto& str) {
						return a_actorbase.HasKeyword(str);
					});
				}
				return std::ranges::any_of(a_strings, [&a_actorbase](const auto& str) {
					return a_actorbase.HasKeyword(str);
				});
			}
		}
	}

	template <class Form>
	bool strings(RE::TESNPC& a_actorbase, const FormData<Form>& a_formData)
	{
		auto& [strings_ALL, strings_NOT, strings_MATCH, strings_ANY] = std::get<DATA_TYPE::kStrings>(a_formData);

		if (!strings_ALL.empty() && !detail::keyword::matches(a_actorbase, strings_ALL, true)) {
			return false;
		}

		const std::string name = a_actorbase.GetName();
		const std::string editorID = Cache::EditorID::GetSingleton()->GetEditorID(a_actorbase.GetFormID());

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

	template <class Form>
	bool forms(RE::TESNPC& a_actorbase, const FormData<Form>& a_formData)
	{
		auto& [filterForms_ALL, filterForms_NOT, filterForms_MATCH] = std::get<DATA_TYPE::kFilterForms>(a_formData);

		if (!filterForms_ALL.empty() && !detail::form::matches_ALL(a_actorbase, filterForms_ALL)) {
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

	template <class Form>
	bool secondary(RE::TESNPC& a_actorbase, const FormData<Form>& a_formData)
	{
		const auto& [sex, isUnique, isSummonable] = std::get<DATA_TYPE::kTraits>(a_formData);
		if (sex && a_actorbase.GetSex() != *sex) {
			return false;
		}
		if (isUnique && a_actorbase.IsUnique() != *isUnique) {
			return false;
		}
		if (isSummonable && a_actorbase.IsSummonable() != *isSummonable) {
			return false;
		}

		const auto& [actorLevelPair, skillLevelPairs] = std::get<DATA_TYPE::kLevel>(a_formData);

		if (!a_actorbase.HasPCLevelMult()) {
			auto [actorMin, actorMax] = actorLevelPair;
			auto actorLevel = a_actorbase.actorData.level;

			if (actorMin < UINT16_MAX && actorMax < UINT16_MAX) {
				if (actorLevel < actorMin || actorLevel > actorMax) {
					return false;
				}
			} else if (actorMin < UINT16_MAX && actorLevel < actorMin) {
				return false;
			} else if (actorMax < UINT16_MAX && actorLevel > actorMax) {
				return false;
			}
		}

		for (auto& [skillType, skill] : skillLevelPairs) {
			auto [skillMin, skillMax] = skill;

			if (skillType >= 0 && skillType < 18) {
				auto const skillLevel = a_actorbase.playerSkills.values[skillType];

				if (skillMin < UINT8_MAX && skillMax < UINT8_MAX) {
					if (skillLevel < skillMin || skillLevel > skillMax) {
						return false;
					}
				} else if (skillMin < UINT8_MAX && skillLevel < skillMin) {
					return false;
				} else if (skillMax < UINT8_MAX && skillLevel > skillMax) {
					return false;
				}
			}
		}

		const auto chance = std::get<DATA_TYPE::kChance>(a_formData);
		if (!numeric::essentially_equal(chance, 100.0)) {
			if (auto rng = RNG::GetSingleton()->Generate<float>(0.0, 100.0); rng > chance) {
				return false;
			}
		}

		return true;
	}
}

namespace Distribute
{
	class DeathItemManager : public RE::BSTEventSink<RE::TESDeathEvent>
	{
	public:
		static DeathItemManager* GetSingleton()
		{
			static DeathItemManager singleton;
			return &singleton;
		}

		static void Register();

	protected:
		struct detail  //AddObjectToContainer doesn't work with leveled items :s
		{
			static void add_item(RE::Actor* a_actor, RE::TESBoundObject* a_item, std::uint32_t a_itemCount, bool a_silent, std::uint32_t a_stackID, RE::BSScript::Internal::VirtualMachine* a_vm)
			{
				using func_t = decltype(&detail::add_item);
				REL::Relocation<func_t> func{ REL::ID(55945) };
				return func(a_actor, a_item, a_itemCount, a_silent, a_stackID, a_vm);
			}
		};

		using EventResult = RE::BSEventNotifyControl;

		EventResult ProcessEvent(const RE::TESDeathEvent* a_event, RE::BSTEventSource<RE::TESDeathEvent>*) override;

	private:
		DeathItemManager() = default;
		DeathItemManager(const DeathItemManager&) = delete;
		DeathItemManager(DeathItemManager&&) = delete;

		~DeathItemManager() = default;

		DeathItemManager& operator=(const DeathItemManager&) = delete;
		DeathItemManager& operator=(DeathItemManager&&) = delete;
	};

	namespace LeveledActor
	{
		void Install();
	}

	template <class Form>
	void for_each_form(RE::TESNPC& a_actorbase, FormDataVec<Form>& a_formDataVec, std::function<bool(const FormCountPair<Form>&)> a_fn)
	{
		for (auto& formData : a_formDataVec) {
			if (!Filter::strings(a_actorbase, formData) || !Filter::forms(a_actorbase, formData) || !Filter::secondary(a_actorbase, formData)) {
				continue;
			}
			if (auto const formCountPair = std::get<DATA_TYPE::kForm>(formData); formCountPair.first != nullptr && a_fn(formCountPair)) {
				++std::get<DATA_TYPE::kNPCCount>(formData);
			}
		}
	}

	template <class Form>
	void list_npc_count(const std::string& a_type, FormDataVec<Form>& a_formDataVec, const size_t a_totalNPCCount)
	{
		if (!a_formDataVec.empty()) {
			logger::info("	{}", a_type);

			for (auto& formData : a_formDataVec) {
				auto [form, itemCount] = std::get<DATA_TYPE::kForm>(formData);
				auto npcCount = std::get<DATA_TYPE::kNPCCount>(formData);

				if (form) {
					std::string name;
					if constexpr (std::is_same_v<Form, RE::BGSKeyword>) {
						name = form->GetFormEditorID();
					} else {
						name = Cache::EditorID::GetSingleton()->GetEditorID(form->GetFormID());
					}

					logger::info("		{} [0x{:X}] added to {}/{} NPCs", name, form->GetFormID(), npcCount, a_totalNPCCount);
				}
			}
		}
	}

	void ApplyToNPCs();
}
