#include "LookupConfigs.h"

namespace INI
{
	namespace detail
	{
		std::string sanitize(const std::string& a_value)
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

			//convert 00012345 formIDs to 0x12345
			static const srell::regex re_formID(R"(\b00+([0-9a-fA-F]{1,6})\b)", srell::regex_constants::optimize);
			newValue = srell::regex_replace(newValue, re_formID, "0x$1");

			//strip leading zeros
			static const srell::regex re_zeros(R"((0x00+)([0-9a-fA-F]+))", srell::regex_constants::optimize);
			newValue = srell::regex_replace(newValue, re_zeros, "0x$2");

			//NOT to hyphen
			//string::replace_all(newValue, "NOT ", "-");

			return newValue;
		}

		std::optional<Exclusion> parse_exclusion(const std::string& a_key, const std::string& a_value, const std::string& a_path) 
		{
			if (a_key != "ExclusionGroup") {
				return std::nullopt;
			}

			const auto sections = string::split(a_value, "|");
			const auto size = sections.size();

			if (size == 0) {
				logger::warn("IGNORED: ExclusionGroup must have a name: {} = {}"sv, a_key, a_value);
				return std::nullopt;
			}

			if (size == 1) {
				logger::warn("IGNORED: ExclusionGroup must have at least one filter name: {} = {}"sv, a_key, a_value);
				return std::nullopt;
			}

			auto split_IDs = distribution::split_entry(sections[1]);

			if (split_IDs.empty()) {
				logger::warn("ExclusionGroup must have at least one Form Filter : {} = {}"sv, a_key, a_value);
				return std::nullopt;
			}

			Exclusion group{};
			group.name = sections[0];
			group.path = a_path;
				
			for (auto& IDs : split_IDs) {
				if (IDs.at(0) == '-') {
					IDs.erase(0, 1);
					group.rawFormFilters.NOT.push_back(distribution::get_record(IDs));
				} else {
					group.rawFormFilters.MATCH.push_back(distribution::get_record(IDs));
				}
			}

			return group;
		}

		std::pair<Data, std::optional<std::string>> parse_ini(const std::string& a_key, const std::string& a_value, const std::string& a_path)
		{
			Data data{};

			auto       sanitized_value = sanitize(a_value);
			const auto sections = string::split(sanitized_value, "|");

			const auto size = sections.size();

			//[FORMID/ESP] / EDITORID
			if (kFormID < size) {
				data.rawForm = distribution::get_record(sections[kFormID]);
			}

			//KEYWORDS
			if (kStrings < size) {
				StringFilters filters;

				auto split_str = distribution::split_entry(sections[kStrings]);
				for (auto& str : split_str) {
					if (str.contains("+"sv)) {
						auto strings = distribution::split_entry(str, "+");
						data.stringFilters.ALL.insert(data.stringFilters.ALL.end(), strings.begin(), strings.end());

					} else if (str.at(0) == '-') {
						str.erase(0, 1);
						data.stringFilters.NOT.emplace_back(str);

					} else if (str.at(0) == '*') {
						str.erase(0, 1);
						data.stringFilters.ANY.emplace_back(str);

					} else {
						data.stringFilters.MATCH.emplace_back(str);
					}
				}
			}

			//FILTER FORMS
			if (kFilterIDs < size) {
				auto split_IDs = distribution::split_entry(sections[kFilterIDs]);
				for (auto& IDs : split_IDs) {
					if (IDs.contains("+"sv)) {
						auto splitIDs_ALL = distribution::split_entry(IDs, "+");
						for (auto& IDs_ALL : splitIDs_ALL) {
							data.rawFormFilters.ALL.push_back(distribution::get_record(IDs_ALL));
						}
					} else if (IDs.at(0) == '-') {
						IDs.erase(0, 1);
						data.rawFormFilters.NOT.push_back(distribution::get_record(IDs));

					} else {
						data.rawFormFilters.MATCH.push_back(distribution::get_record(IDs));
					}
				}
			}

			//LEVEL
			Range<std::uint16_t>    actorLevel;
			std::vector<SkillLevel> skillLevels;
			std::vector<SkillLevel> skillWeights;
			if (kLevel < size) {
				auto split_levels = distribution::split_entry(sections[kLevel]);
				for (auto& levels : split_levels) {
					if (levels.contains('(')) {
						//skill(min/max)
						const auto isWeightFilter = levels.starts_with('w');
						auto       sanitizedLevel = string::remove_non_alphanumeric(levels);
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
										skillWeights.push_back({ type, Range(minLevel, maxLevel) });
									} else {
										skillLevels.push_back({ type, Range(minLevel, maxLevel) });
									}
								} else {
									if (isWeightFilter) {
										// Single value is treated as exact match.
										skillWeights.push_back({ type, Range(minLevel) });
									} else {
										skillLevels.push_back({ type, Range(minLevel) });
									}
								}
							}
						}
					} else {
						if (auto actor_level = string::split(levels, "/"); actor_level.size() > 1) {
							auto minLevel = string::to_num<std::uint16_t>(actor_level[0]);
							auto maxLevel = string::to_num<std::uint16_t>(actor_level[1]);

							actorLevel = Range(minLevel, maxLevel);
						} else {
							auto level = string::to_num<std::uint16_t>(levels);

							actorLevel = Range(level);
						}
					}
				}
			}
			data.levelFilters = { actorLevel, skillLevels, skillWeights };

			//TRAITS
			if (kTraits < size) {
				auto split_traits = distribution::split_entry(sections[kTraits], "/");
				for (auto& trait : split_traits) {
					switch (string::const_hash(trait)) {
					case "M"_h:
					case "-F"_h:
						data.traits.sex = RE::SEX::kMale;
						break;
					case "F"_h:
					case "-M"_h:
						data.traits.sex = RE::SEX::kFemale;
						break;
					case "U"_h:
						data.traits.unique = true;
						break;
					case "-U"_h:
						data.traits.unique = false;
						break;
					case "S"_h:
						data.traits.summonable = true;
						break;
					case "-S"_h:
						data.traits.summonable = false;
						break;
					case "C"_h:
						data.traits.child = true;
						break;
					case "-C"_h:
						data.traits.child = false;
						break;
					case "L"_h:
						data.traits.leveled = true;
						break;
					case "-L"_h:
						data.traits.leveled = false;
						break;
					case "T"_h:
						data.traits.teammate = true;
						break;
					case "-T"_h:
						data.traits.teammate = false;
						break;
					default:
						break;
					}
				}
			}

			//ITEMCOUNT/INDEX
			if (a_key == "Package") {  // reuse item count for package stack index
				data.idxOrCount = 0;
			}
			if (kIdxOrCount < size) {
				if (const auto& str = sections[kIdxOrCount]; distribution::is_valid_entry(str)) {
					data.idxOrCount = string::to_num<std::int32_t>(str);
				}
			}

			//CHANCE
			if (kChance < size) {
				if (const auto& str = sections[kChance]; distribution::is_valid_entry(str)) {
					data.chance = string::to_num<Chance>(str);
				}
			}

			data.path = a_path;

			if (sanitized_value != a_value) {
				return { data, sanitized_value };
			}
			return { data, std::nullopt };
		}
	}

	std::pair<bool, bool> GetConfigs()
	{
		logger::info("{:*^50}", "INI");

		std::vector<std::string> files = distribution::get_configs(R"(Data\)", "_DISTR"sv);

		if (files.empty()) {
			logger::warn("No .ini files with _DISTR suffix were found within the Data folder, aborting...");
			return { false, false };
		}

		logger::info("{} matching inis found", files.size());

		bool shouldLogErrors{ false };

		for (const auto& path : files) {
			logger::info("\tINI : {}", path);

			CSimpleIniA ini;
			ini.SetUnicode();
			ini.SetMultiKey();

			if (const auto rc = ini.LoadFile(path.c_str()); rc < 0) {
				logger::error("\t\tcouldn't read INI");
				continue;
			}

			if (auto values = ini.GetSection(""); values && !values->empty()) {
				std::multimap<CSimpleIniA::Entry, std::pair<std::string, std::string>, CSimpleIniA::Entry::LoadOrder> oldFormatMap;

				auto truncatedPath = path.substr(5);  //strip "Data\\"

				for (auto& [key, entry] : *values) {
					try {						
						if (const auto exclusionOpt = detail::parse_exclusion(key.pItem, entry, truncatedPath); exclusionOpt) {
							const auto& exclusion = *exclusionOpt;
							exclusions.emplace_back(exclusion);
							continue;
						}

						auto [data, sanitized_str] = detail::parse_ini(key.pItem, entry, truncatedPath);
						configs[key.pItem].emplace_back(data);

						if (sanitized_str) {
							oldFormatMap.emplace(key, std::make_pair(entry, *sanitized_str));
						}
					} catch (...) {
						logger::warn("\t\tFailed to parse entry [{} = {}]", key.pItem, entry);
						shouldLogErrors = true;
					}
				}

				if (!oldFormatMap.empty()) {
					logger::info("\t\tsanitizing {} entries", oldFormatMap.size());

					for (auto& [key, entry] : oldFormatMap) {
						auto& [original, sanitized] = entry;
						ini.DeleteValue("", key.pItem, original.c_str());
						ini.SetValue("", key.pItem, sanitized.c_str(), key.pComment, false);
					}

					(void)ini.SaveFile(path.c_str());
				}
			}
		}

		return { true, shouldLogErrors };
	}
}
