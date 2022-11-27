#pragma once

namespace INI
{
	inline std::unordered_map<std::string_view, INIDataVec> configs;

	namespace detail
	{
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

	inline std::tuple<INIData, std::optional<std::string>> parse_ini(const std::string& a_key, const std::string& a_value, const std::string& a_path)
	{
		INIData data;
		auto& [recordID, strings_ini, filterIDs_ini, level_ini, traits_ini, idxOrCount_ini, chance_ini, path] = data;
		path = a_path;

		auto sanitized_value = detail::sanitize(a_value);
		const auto sections = string::split(sanitized_value, "|");
		const auto size = sections.size();

		//[FORMID/ESP] / EDITORID
		if (kFormID < size) {
			recordID = distribution::get_record(sections[kFormID]);
		}

		//KEYWORDS
		if (kStrings < size) {
			auto& [strings_ALL, strings_NOT, strings_MATCH, strings_ANY] = strings_ini;

			auto split_str = distribution::split_entry(sections[kStrings]);
			for (auto& str : split_str) {
				if (str.contains("+"sv)) {
					auto strings = distribution::split_entry(str, "+");
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

			auto split_IDs = distribution::split_entry(sections[kFilterIDs]);
			for (auto& IDs : split_IDs) {
				if (IDs.contains("+"sv)) {
					auto splitIDs_ALL = distribution::split_entry(IDs, "+");
					for (auto& IDs_ALL : splitIDs_ALL) {
						filterIDs_ALL.push_back(distribution::get_record(IDs_ALL));
					}
				} else if (IDs.at(0) == '-') {
					IDs.erase(0, 1);
					filterIDs_NOT.push_back(distribution::get_record(IDs));

				} else {
					filterIDs_MATCH.push_back(distribution::get_record(IDs));
				}
			}
		}

		//LEVEL
		ActorLevel actorLevelPair = { UINT16_MAX, UINT16_MAX };
		std::vector<SkillLevel> skillLevelPairs;
		std::vector<SkillLevel> skillWeightPairs;
		if (kLevel < size) {
			auto split_levels = distribution::split_entry(sections[kLevel]);
			for (auto& levels : split_levels) {
				if (levels.contains('(')) {
					//skill(min/max)
					const auto isWeightFilter = levels.starts_with('w');
					auto sanitizedLevel = string::remove_non_alphanumeric(levels);
					if (isWeightFilter) {
						sanitizedLevel.erase(0, 1);
					}
					//skill min max
					if (auto skills = string::split(sanitizedLevel, " "); !skills.empty()) {
						if (auto type = string::to_num<std::uint32_t>(skills[0]); type < 18) {
							auto minLevel = string::to_num<std::uint8_t>(skills[1]);
							if (skills.size() > 2) {
								auto maxLevel = string::to_num<std::uint8_t>(skills[2]);
								if (isWeightFilter) {
									skillWeightPairs.push_back({ type, { minLevel, maxLevel } });
								} else {
									skillLevelPairs.push_back({ type, { minLevel, maxLevel } });
								}
							} else {
								if (isWeightFilter) {
									// Single value is treated as exact match.
									skillWeightPairs.push_back({ type, { minLevel, minLevel } });
								} else {
									skillLevelPairs.push_back({ type, { minLevel, UINT8_MAX } });
								}
							}
						}
					}
				} else {
					auto split_level = string::split(levels, "/");
					if (split_level.size() > 1) {
						auto minLevel = string::to_num<std::uint16_t>(split_level[0]);
						auto maxLevel = string::to_num<std::uint16_t>(split_level[1]);

						actorLevelPair = { minLevel, maxLevel };
					} else {
						auto level = string::to_num<std::uint16_t>(levels);

						actorLevelPair = { level, UINT16_MAX };
					}
				}
			}
		}
		level_ini = { actorLevelPair, skillLevelPairs, skillWeightPairs };

		//TRAITS
		if (kTraits < size) {
			auto& [sex, unique, summonable, child] = traits_ini;

			auto split_traits = distribution::split_entry(sections[kTraits], "/");
			for (auto& trait : split_traits) {
				if (trait == "M" || trait == "-F") {
					sex = RE::SEX::kMale;
				} else if (trait == "F" || trait == "-M") {
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
			if (distribution::is_valid_entry(str)) {
				idxOrCount_ini = string::to_num<std::int32_t>(str);
			}
		}

		//CHANCE
		chance_ini = 100;
		if (kChance < size) {
			const auto& str = sections[kChance];
			if (distribution::is_valid_entry(str)) {
				chance_ini = string::to_num<float>(str);
			}
		}

		if (sanitized_value != a_value) {
			return std::make_tuple(data, sanitized_value);
		}
		return std::make_tuple(data, std::nullopt);
	}

	std::pair<bool, bool> Read();
}
