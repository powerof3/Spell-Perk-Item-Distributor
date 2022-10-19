#pragma once

namespace INI
{
	inline std::unordered_map<std::string_view, INIDataMap> configs;

	namespace detail
	{
		inline bool is_valid_entry(const std::string& a_str)
		{
			return !a_str.empty() && !a_str.contains("NONE"sv);
		}

		inline std::vector<std::string> split_sub_string(const std::string& a_str, const std::string& a_delimiter = ",")
		{
			if (is_valid_entry(a_str)) {
				return string::split(a_str, a_delimiter);
			}
			return {};
		}

		inline FormIDPair get_formID(const std::string& a_str)
		{
			if (a_str.contains("~"sv)) {
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
			if (!newValue.contains('~')) {
				string::replace_first_instance(newValue, " - ", "~");
			}

#ifdef SKYRIMVR
			// swap dawnguard and dragonborn forms
			// we do this during sanitize instead of in get_formID to squelch log errors
			// VR apparently does not load masters in order so the lookup fails
			static const srell::regex re_dawnguard(R"((0x0*2)([0-9a-f]{6}))", srell::regex_constants::optimize | srell::regex::icase);
			newValue = regex_replace(newValue, re_dawnguard, "0x$2~Dawnguard.esm");

			static const srell::regex re_dragonborn(R"((0x0*4)([0-9a-f]{6}))", srell::regex_constants::optimize | srell::regex::icase);
			newValue = regex_replace(newValue, re_dragonborn, "0x$2~Dragonborn.esm");
#endif

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

	inline std::tuple<FormOrEditorID, INIData, std::optional<std::string>> parse_ini(const std::string& a_key, const std::string& a_value, const std::string& a_path)
	{
		FormOrEditorID recordID;

		INIData data;
		auto& [strings_ini, filterIDs_ini, level_ini, traits_ini, idxOrCount_ini, chance_ini, path] = data;
		path = a_path;

		auto sanitized_value = detail::sanitize(a_value);
		const auto sections = string::split(sanitized_value, "|");
		const auto size = sections.size();

		//[FORMID/ESP] / EDITORID
		if (kFormID < size) {
			auto& formSection = sections[kFormID];
			if (formSection.contains('~') || string::is_only_hex(formSection)) {
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
		} else {
			FormIDPair pair = { 0, std::nullopt };
			recordID.emplace<FormIDPair>(pair);
		}

		//KEYWORDS
		if (kStrings < size) {
			auto& [strings_ALL, strings_NOT, strings_MATCH, strings_ANY] = strings_ini;

			auto split_str = detail::split_sub_string(sections[kStrings]);
			for (auto& str : split_str) {
				if (str.contains("+"sv)) {
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
		}

		//FILTER FORMS
		if (kFilterIDs < size) {
			auto& [filterIDs_ALL, filterIDs_NOT, filterIDs_MATCH] = filterIDs_ini;

			auto split_IDs = detail::split_sub_string(sections[kFilterIDs]);
			for (auto& IDs : split_IDs) {
				if (IDs.contains("+"sv)) {
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
		}

		//LEVEL
		ActorLevel actorLevelPair = { UINT16_MAX, UINT16_MAX };
		std::vector<SkillLevel> skillLevelPairs;
		if (kLevel < size) {
			auto split_levels = detail::split_sub_string(sections[kLevel]);
			for (auto& levels : split_levels) {
				if (levels.contains('(')) {
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
		}
		level_ini = { actorLevelPair, skillLevelPairs };

		//TRAITS
		if (kTraits < size) {
			auto& [sex, unique, summonable, child] = traits_ini;

			auto split_traits = detail::split_sub_string(sections[kTraits], "/");
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
				} else if (trait == "C") {
					child = true;
				} else if (trait == "-C") {
					child = false;
				}
			}
		}

		//ITEMCOUNT/INDEX
		idxOrCount_ini = a_key == "Package" ? 0 : 1;  //reuse item count for package stack index
		if (kIdxOrCount < size) {
			const auto& str = sections[kIdxOrCount];
			if (detail::is_valid_entry(str)) {
				idxOrCount_ini = string::lexical_cast<std::int32_t>(str);
			}
		}

		//CHANCE
		chance_ini = 100;
		if (kChance < size) {
			const auto& str = sections[kChance];
			if (detail::is_valid_entry(str)) {
				chance_ini = string::lexical_cast<float>(str);
			}
		}

		if (sanitized_value != a_value) {
			return std::make_tuple(recordID, data, sanitized_value);
		}
		return std::make_tuple(recordID, data, std::nullopt);
	}

	bool Read();
}
