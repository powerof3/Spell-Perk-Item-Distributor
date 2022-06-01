#pragma once

namespace INI
{
	inline std::map<std::string, INIDataMap> configs;

    namespace detail
	{
		inline std::vector<std::string> split_sub_string(const std::string& a_str, const std::string& a_delimiter = ",")
		{
			if (!a_str.empty() && a_str.find("NONE"sv) == std::string::npos) {
				return string::split(a_str, a_delimiter);
			}
			return {};
		}

		inline FormIDPair get_formID(const std::string& a_str)
		{
			if (a_str.find("~"sv) != std::string::npos) {
				auto splitID = string::split(a_str, "~");
				return std::make_pair(
					string::lexical_cast<RE::FormID>(splitID.at(kFormID), true),
					splitID.at(kESP));
			}
            if (is_mod_name(a_str) || !string::is_only_hex(a_str)) {
                return std::make_pair(
                    std::nullopt,
                    a_str);
            }
            return std::make_pair(
                string::lexical_cast<RE::FormID>(a_str, true),
                std::nullopt);
        }

		inline std::string sanitize(const std::string& a_value)
		{
			auto newValue = a_value;

			//formID hypen
			if (newValue.find('~') == std::string::npos) {
				string::replace_first_instance(newValue, " - ", "~");
			}

			//strip spaces between " | "
			static const srell::regex re_bar(R"(\s*\|\s*)", srell::regex_constants::optimize);
			newValue = srell::regex_replace(newValue, re_bar, "|");

			//strip spaces between " , "
			static const srell::regex re_comma(R"(\s*,\s*)", srell::regex_constants::optimize);
			newValue = srell::regex_replace(newValue, re_comma, ",");

			//strip leading zeros
			static const srell::regex re_zeros(R"((0x00+)([0-9a-fA-F]+))", srell::regex_constants::optimize);
			newValue = srell::regex_replace(newValue, re_zeros, "0x$2");

			//NOT to hyphen
			string::replace_all(newValue, "NOT ", "-");

			return newValue;
		}
	}

	inline std::tuple<FormOrEditorID, INIData, std::optional<std::string>> parse_ini(const std::string& a_value)
	{
		FormOrEditorID recordID;

        INIData data;
		auto& [strings_ini, filterIDs_ini, level_ini, traits_ini, itemCount_ini, chance_ini] = data;

		auto sanitized_value = detail::sanitize(a_value);
		const auto sections = string::split(sanitized_value, "|");

		//[FORMID/ESP] / EDITORID
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

				recordID.emplace<FormIDPair>(pair);
			} else {
				recordID.emplace<std::string>(formSection);
			}
		} catch (...) {
			FormIDPair pair = { 0, std::nullopt };
			recordID.emplace<FormIDPair>(pair);
		}

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
			return std::make_tuple(recordID, data, sanitized_value);
		}
		return std::make_tuple(recordID, data, std::nullopt);
	}

	bool Read();
}
