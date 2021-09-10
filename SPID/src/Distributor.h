#pragma once

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

		inline FormIDPair get_filter_ID(const std::string& a_str)
		{
			if (a_str.find("~"sv) != std::string::npos) {
				auto splitID = string::split(a_str, "~");
				return std::make_pair(
					string::lexical_cast<RE::FormID>(splitID.at(kFormID), true),
					splitID.at(kESP));
			}
			return std::make_pair(
				string::lexical_cast<RE::FormID>(a_str, true),
				std::nullopt);
		}

		inline std::string sanitize(const std::string& a_value)
		{
			static const boost::regex re_spaces(R"((\s+-\s+|\b\s+\b)|\s+)", boost::regex_constants::optimize);
			if (auto newValue = boost::regex_replace(a_value, re_spaces, "$1"); newValue != a_value) {
				string::replace_first_instance(newValue, " - ", "~");
				string::replace_all(newValue, "NOT ", "-");
				return newValue;
			}
			return a_value;
		}
	}

	inline std::pair<INIData, std::optional<std::string>> parse_ini(const std::string& a_value)
	{
		using namespace FILTERS;

		INIData data;
		auto& [formIDPair_ini, strings_ini, filterIDs_ini, level_ini, gender_ini, itemCount_ini, chance_ini] = data;

		auto sanitized_value = detail::sanitize(a_value);
		auto sections = string::split(sanitized_value, "|");

		//[FORMID/ESP] / string
		std::variant<FormIDPair, std::string> item_ID;
		try {
			auto formSection = sections.at(kFormID);
			if (!string::is_only_letter(formSection)) {
				FormIDPair pair;
				pair.second = std::nullopt;

				if (string::is_only_hex(formSection)) {
					// formID
					pair.first = string::lexical_cast<RE::FormID>(formSection, true);
				} else {
					// formID~esp
					auto formIDpair = string::split(formSection, "~");
					pair.first = string::lexical_cast<RE::FormID>(formIDpair.at(kFormID), true);
					if (auto size = formIDpair.size(); size > 1) {
						pair.second = formIDpair.at(kESP);
					}
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

				} else if (auto it = str.find("-"sv); it != std::string::npos) {
					str.erase(it, 1);
					strings_NOT.emplace_back(str);

				} else if (it = str.find("*"sv); it != std::string::npos) {
					str.erase(it, 1);
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
						filterIDs_ALL.push_back(detail::get_filter_ID(IDs_ALL));
					}
				} else if (auto it = IDs.find("-"sv); it != std::string::npos) {
					IDs.erase(it, 1);
					filterIDs_NOT.push_back(detail::get_filter_ID(IDs));

				} else {
					filterIDs_MATCH.push_back(detail::get_filter_ID(IDs));
				}
			}
		} catch (...) {
		}

		//LEVEL
		ActorLevel actorLevelPair = { ACTOR_LEVEL::MAX, ACTOR_LEVEL::MAX };
		SkillLevel skillLevelPair = { SKILL_LEVEL::TYPE_MAX, { SKILL_LEVEL::VALUE_MAX, SKILL_LEVEL::VALUE_MAX } };
		try {
			for (auto split_levels = detail::split_sub_string(sections.at(kLevel)); auto& levels : split_levels) {
				if (levels.find('(') != std::string::npos) {
					//skill(min/max)
					auto sanitizedLevel = string::remove_non_alphanumeric(levels);
					auto skills = string::split(sanitizedLevel, " ");
					//skill min max
					if (!skills.empty()) {
						if (skills.size() > 2) {
							auto type = string::lexical_cast<std::uint32_t>(skills.at(S_LEVEL::kType));
							auto minLevel = string::lexical_cast<std::uint8_t>(skills.at(S_LEVEL::kMin));
							auto maxLevel = string::lexical_cast<std::uint8_t>(skills.at(S_LEVEL::kMax));
							skillLevelPair = { type, { minLevel, maxLevel } };
						} else {
							auto type = string::lexical_cast<std::uint32_t>(skills.at(S_LEVEL::kType));
							auto minLevel = string::lexical_cast<std::uint8_t>(skills.at(S_LEVEL::kMin));
							skillLevelPair = { type, { minLevel, SKILL_LEVEL::VALUE_MAX } };
						}
					}
				} else {
					auto split_level = string::split(levels, "/");
					if (split_level.size() > 1) {
						auto minLevel = string::lexical_cast<std::uint16_t>(split_level.at(A_LEVEL::kMin));
						auto maxLevel = string::lexical_cast<std::uint16_t>(split_level.at(A_LEVEL::kMax));
						actorLevelPair = { minLevel, maxLevel };
					} else {
						auto level = string::lexical_cast<std::uint16_t>(levels);
						actorLevelPair = { level, ACTOR_LEVEL::MAX };
					}
				}
			}
		} catch (...) {
		}
		level_ini = { actorLevelPair, skillLevelPair };

		//GENDER
		gender_ini = RE::SEX::kNone;
		try {
			if (auto genderStr = sections.at(kGender); !genderStr.empty() && genderStr.find("NONE"sv) == std::string::npos) {
				if (genderStr == "M") {
					gender_ini = RE::SEX::kMale;
				} else if (genderStr == "F") {
					gender_ini = RE::SEX::kFemale;
				} else {
					gender_ini = string::lexical_cast<RE::SEX>(sections.at(kGender));
				}
			}
		} catch (...) {
		}

		//ITEMCOUNT
		itemCount_ini = 1;
		try {
			if (const auto itemCountStr = sections.at(kItemCount); !itemCountStr.empty() && itemCountStr.find("NONE"sv) == std::string::npos) {
				itemCount_ini = string::lexical_cast<std::int32_t>(itemCountStr);
			}
		} catch (...) {
		}

		//CHANCE
		chance_ini = 100;
		try {
			if (const auto chanceStr = sections.at(kChance); !chanceStr.empty() && chanceStr.find("NONE"sv) == std::string::npos) {
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
		inline bool formID_to_form(RE::TESDataHandler* a_dataHandler, const FormIDPairVec& a_formIDVec, FormVec& a_formVec)
		{
			if (!a_formIDVec.empty()) {
				constexpr auto lookup_form_type = [](const RE::FormType a_type) {
					switch (a_type) {
					case RE::FormType::Faction:
						return "Faction"sv;
					case RE::FormType::Class:
						return "Class"sv;
					case RE::FormType::CombatStyle:
						return "CombatStyle"sv;
					case RE::FormType::Race:
						return "Race"sv;
					case RE::FormType::Outfit:
						return "Outfit"sv;
					case RE::FormType::NPC:
						return "NPC"sv;
					default:
						return ""sv;
					}
				};

				for (auto& [formID, modName] : a_formIDVec) {
					RE::TESForm* filterForm = nullptr;
					if (modName.has_value()) {
						filterForm = a_dataHandler->LookupForm(formID, modName.value());
					} else {
						filterForm = RE::TESForm::LookupByID(formID);
					}
					if (filterForm) {
						const auto formType = filterForm->GetFormType();
						if (const auto type = lookup_form_type(formType); !type.empty()) {
							a_formVec.push_back(filterForm);
						} else {
							logger::error("				Filter [0x{:X}]) FAIL - invalid formtype ({})", formID, formType);
							return false;
						}
					} else {
						logger::error("				Filter [0x{:X}] FAIL - form doesn't exist", formID);
						return false;
					}
				}
			}
			return true;
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
			Form* form;

			if (std::holds_alternative<FormIDPair>(formIDPair_ini)) {
				auto [formID, modName] = std::get<FormIDPair>(formIDPair_ini);
				if (modName.has_value()) {
					form = a_dataHandler->LookupForm<Form>(formID, modName.value());
					if constexpr (std::is_same_v<Form, RE::TESBoundObject>) {
						if (!form) {
							const auto base = a_dataHandler->LookupForm(formID, modName.value());
							form = base ? static_cast<Form*>(base) : nullptr;
						}
					}
				} else {
					form = RE::TESForm::LookupByID<Form>(formID);
					if constexpr (std::is_same_v<Form, RE::TESBoundObject>) {
						if (!form) {
							const auto base = RE::TESForm::LookupByID(formID);
							form = base ? static_cast<Form*>(base) : nullptr;
						}
					}
				}
				if (!form) {
					logger::error("		{} [0x{:X}] ({}) FAIL - doesn't exist or has invalid form type", a_type, formID, modName.has_value() ? modName.value() : "null esp");
					continue;
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
						if (const auto keyword = *result; keyword && !keyword->IsDynamicForm()) {
							logger::info("		{} [0x{:X}] INFO - using existing keyword", keywordName, keyword->GetFormID());

							form = keyword;
						} else {
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
							continue;
						}
					}
				}
			}

			std::array<FormVec, 3> filterForms;
			bool result = true;
			for (std::uint32_t i = 0; i < 3; i++) {
				if (!detail::formID_to_form(a_dataHandler, filterIDs_ini[i], filterForms[i])) {
					result = false;
					break;
				}
			}
			if (!result) {
				continue;
			}

			FormCountPair<Form> formCountPair = { form, itemCount_ini };
			std::uint32_t npcCount = 0;

			FormData<Form> formData = { formCountPair, strings_ini, filterForms, level_ini, gender_ini, chance_ini, npcCount };
			a_formDataVec.emplace_back(formData);
		}
	}

	bool GetForms();
}

namespace Distribute
{
	namespace filter
	{
		namespace name
		{
			inline bool contains(const std::string& a_name, const StringVec& a_strings)
			{
				return std::ranges::any_of(a_strings, [&](const auto& str) {
					return string::icontains(str, a_name);
				});
			}

			inline bool matches(const std::string& a_name, const StringVec& a_strings)
			{
				return std::ranges::any_of(a_strings, [&](const auto& str) {
					return string::iequals(str, a_name);
				});
			}
		}

		namespace form
		{
			inline bool get_type(RE::TESNPC& a_actorbase, RE::TESForm* a_type)
			{
				switch (a_type->GetFormType()) {
				case RE::FormType::CombatStyle:
					if (const auto combatStyle = static_cast<RE::TESCombatStyle*>(a_type); a_actorbase.GetCombatStyle() == combatStyle) {
						return true;
					}
					break;
				case RE::FormType::Class:
					if (const auto npcClass = static_cast<RE::TESClass*>(a_type); a_actorbase.npcClass == npcClass) {
						return true;
					}
					break;
				case RE::FormType::Faction:
					if (const auto faction = static_cast<RE::TESFaction*>(a_type); a_actorbase.IsInFaction(faction)) {
						return true;
					}
					break;
				case RE::FormType::Race:
					if (const auto race = static_cast<RE::TESRace*>(a_type); a_actorbase.GetRace() == race) {
						return true;
					}
					break;
				case RE::FormType::Outfit:
					if (const auto outfit = static_cast<RE::BGSOutfit*>(a_type); a_actorbase.defaultOutfit == outfit) {
						return true;
					}
					break;
				case RE::FormType::NPC:
					if (const auto npc = static_cast<RE::TESNPC*>(a_type); &a_actorbase == npc) {
						return true;
					}
					break;
				default:
					break;
				}

				return false;
			}

			inline bool matches(RE::TESNPC& a_actorbase, const FormVec& a_forms)
			{
				return std::ranges::any_of(a_forms, [&a_actorbase](const auto& a_form) {
					return a_form && get_type(a_actorbase, a_form);
				});
			}

			inline bool matches_ALL(RE::TESNPC& a_actorbase, const FormVec& a_forms)
			{
				return std::ranges::all_of(a_forms, [&a_actorbase](const auto& a_form) {
					return a_form && get_type(a_actorbase, a_form);
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

			inline bool matches(RE::TESNPC& a_actorbase, const StringVec& a_strings)
			{
				return std::ranges::any_of(a_strings, [&a_actorbase](const auto& str) {
					return a_actorbase.HasKeyword(str);
				});
			}

			inline bool matches_ALL(RE::TESNPC& a_actorbase, const StringVec& a_strings)
			{
				return std::ranges::all_of(a_strings, [&a_actorbase](const auto& str) {
					return a_actorbase.HasKeyword(str);
				});
			}
		}

		template <class Form>
		bool strings(RE::TESNPC& a_actorbase, const FormData<Form>& a_formData)
		{
			auto& [strings_ALL, strings_NOT, strings_MATCH, strings_ANY] = std::get<DATA_TYPE::kStrings>(a_formData);

			if (strings_ALL.empty() && strings_NOT.empty() && strings_MATCH.empty() && strings_ANY.empty()) {
				return true;
			}

			if (!strings_ALL.empty() && !keyword::matches_ALL(a_actorbase, strings_ALL)) {
				return false;
			}

			const std::string name = a_actorbase.GetName();
			if (!strings_NOT.empty()) {
				bool result = false;
				if (!name.empty() && name::matches(name, strings_NOT)) {
					result = true;
				}
				if (!result && keyword::matches(a_actorbase, strings_NOT)) {
					result = true;
				}
				if (result) {
					return false;
				}
			}
			if (!strings_MATCH.empty()) {
				bool result = false;
				if (!name.empty() && name::matches(name, strings_MATCH)) {
					result = true;
				}
				if (!result && keyword::matches(a_actorbase, strings_MATCH)) {
					result = true;
				}
				if (!result) {
					return false;
				}
			}
			if (!strings_ANY.empty()) {
				bool result = false;
				if (!name.empty() && name::contains(name, strings_ANY)) {
					result = true;
				}
				if (!result && keyword::contains(a_actorbase, strings_ANY)) {
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

			if (filterForms_ALL.empty() && filterForms_NOT.empty() && filterForms_MATCH.empty()) {
				return true;
			}

			if (!filterForms_ALL.empty() && !form::matches_ALL(a_actorbase, filterForms_ALL)) {
				return false;
			}

			if (!filterForms_NOT.empty() && form::matches(a_actorbase, filterForms_NOT)) {
				return false;
			}

			if (!filterForms_MATCH.empty() && !form::matches(a_actorbase, filterForms_MATCH)) {
				return false;
			}

			return true;
		}

		template <class Form>
		bool secondary(RE::TESNPC& a_actorbase, const FormData<Form>& a_formData)
		{
			using namespace FILTERS;

			auto const levelPair = std::get<DATA_TYPE::kLevel>(a_formData);
			auto const& [actorLevelPair, skillLevelPair] = levelPair;

			auto const gender = std::get<DATA_TYPE::kGender>(a_formData);
			auto const chance = std::get<DATA_TYPE::kChance>(a_formData);

			if (gender != RE::SEX::kNone && a_actorbase.GetSex() != gender) {
				return false;
			}

			if (!a_actorbase.HasPCLevelMult()) {
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

			if (!numeric::essentially_equal(chance, 100.0)) {
				if (auto rng = RNG::GetSingleton()->Generate<float>(0.0, 100.0); rng > chance) {
					return false;
				}
			}

			return true;
		}
	}

	template <class Form>
	void for_each_form(RE::TESNPC& a_actorbase, FormDataVec<Form>& a_formDataVec, std::function<bool(const FormCountPair<Form>&)> a_fn)
	{
		for (auto& formData : a_formDataVec) {
			if (!filter::strings(a_actorbase, formData) || !filter::forms(a_actorbase, formData) || !filter::secondary(a_actorbase, formData)) {
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
						name = form->formEditorID;
					} else {
						name = form->GetName();
					}
					logger::info("		{} [0x{:X}] added to {}/{} NPCs", name, form->GetFormID(), npcCount, a_totalNPCCount);
				}
			}
		}
	}

	void ApplyToNPCs();

	namespace Leveled
	{
		void Hook();
	}

	namespace Hook
	{
		void Install();
	}

	namespace DeathItem
	{
		struct detail  //AddObjectToContainer doesn't work with leveled items :s
		{
			static void add_item(RE::Actor* a_actor, RE::TESBoundObject* a_item, std::uint32_t a_itemCount, bool a_silent, std::uint32_t a_stackID, RE::BSScript::Internal::VirtualMachine* a_vm)
			{
				using func_t = decltype(&detail::add_item);
				REL::Relocation<func_t> func{ REL::ID(55945) };
				return func(a_actor, a_item, a_itemCount, a_silent, a_stackID, a_vm);
			}
		};

		class DeathManager : public RE::BSTEventSink<RE::TESDeathEvent>
		{
		public:
			static DeathManager* GetSingleton()
			{
				static DeathManager singleton;
				return &singleton;
			}

			static void Register();

		protected:
			using EventResult = RE::BSEventNotifyControl;

			EventResult ProcessEvent(const RE::TESDeathEvent* a_event, RE::BSTEventSource<RE::TESDeathEvent>*) override;

		private:
			DeathManager() = default;
			DeathManager(const DeathManager&) = delete;
			DeathManager(DeathManager&&) = delete;

			~DeathManager() = default;

			DeathManager& operator=(const DeathManager&) = delete;
			DeathManager& operator=(DeathManager&&) = delete;
		};
	}
}
