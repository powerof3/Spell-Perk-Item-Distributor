#include "LookupConfigs.h"
#include "LookupFilters.h"

namespace INI
{
	namespace detail
	{
		using namespace Filter;

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

			//strip leading zeros
			static const srell::regex re_zeros(R"((0x00+)([0-9a-fA-F]+))", srell::regex_constants::optimize);
			newValue = srell::regex_replace(newValue, re_zeros, "0x$2");

			//NOT to hyphen
			string::replace_all(newValue, "NOT ", "-");

			return newValue;
		}

		template <class T>
		Expression parse_entries(const std::string& filter_str, std::function<std::optional<T>(std::string&)> parser)
		{
			AndExpression entries;
			auto          entries_str = distribution::split_entry(filter_str, "+");

			for (auto& entry_str : entries_str) {
				bool isNegated = false;

				if (entry_str.at(0) == '-') {
					entry_str.erase(0, 1);
					isNegated = true;
				}

				if (auto val = parser(entry_str)) {
					entries.entries.push_back(isNegated ? NegatedFilter(val) : FilterEntry(val));
				} else {
					buffered_logger::warn("\tUnrecognized filter: {}", entry_str);
				}
			}
			return entries;
		}

		template <class T>
		OrExpression parse_filters(const std::string& expression_str, std::function<std::optional<T>(std::string&)> parser)
		{
			OrExpression filters;
			auto         filters_str = distribution::split_entry(expression_str);
			for (auto& filter_str : filters_str) {
				filters.filters.push_back(parse_entries(filter_str, parser));
			}
			return filters;
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
				data.stringFilters = parse_filters<StringValue>(sections[kStrings], [](std::string& entry_str) -> StringValue {
					if (entry_str.at(0) == '*') {
						entry_str.erase(0, 1);
						return Wildcard{ entry_str };
					} else {
						return Match{ entry_str };
					}
				});
			}

			//FILTER FORMS
			if (kFilterIDs < size) {
				data.idFilters = parse_filters<FormOrEditorID>(sections[kFilterIDs], [](std::string& entry_str) {
					return distribution::get_record(entry_str);
				});
			}

			//LEVEL
			if (kLevel < size) {
				// Matches all types of level and skill filters
				std::regex regex("^(?:(w)?(\\d+)\\((\\d+)?(?:\\/(\\d+)?)?\\)|(\\d+)?(?:\\/(\\d+)?)?)$", std::regex_constants::optimize);
				// Indices of matched groups
				enum
				{
					kMatch = 0,
					kModifier,
					kSkill,
					kSkillMin,
					kSkillMax,
					kLevelMin,
					kLevelMax,

					kTotal

				};
				data.levelFilters = parse_filters<LevelRange>(sections[kLevel], [&](std::string& entry_str) -> std::optional<LevelRange> {
					std::smatch matches;
					if (!entry_str.empty() && std::regex_search(entry_str, matches, regex) && matches.size() >= kTotal) {
						if (matches[kSkill].length() > 0) {  // skills
							if (auto skill = string::to_num<SkillLevelRange::Skill>(matches[kSkill].str()); skill < SkillLevelRange::Skills::kTotal) {
								auto min = matches[kSkillMin].length() > 0 ? string::to_num<SkillLevelRange::Level>(matches[kSkillMin].str()) : LevelRange::MinLevel;
								auto max = matches[kSkillMax].length() > 0 ? string::to_num<SkillLevelRange::Level>(matches[kSkillMax].str()) : LevelRange::MaxLevel;
								if (matches[kModifier] == 'w') {
									return SkillWeightRange(skill, min, max);
								} else if (matches[kModifier].length() == 0) {
									return SkillLevelRange(skill, min, max);
								} else {
									return std::nullopt;
								}
							}
							return std::nullopt;
						} else {  // levels
							auto min = matches[kLevelMin].length() > 0 ? string::to_num<LevelRange::Level>(matches[kLevelMin].str()) : LevelRange::MinLevel;
							auto max = matches[kLevelMax].length() > 0 ? string::to_num<LevelRange::Level>(matches[kLevelMax].str()) : LevelRange::MaxLevel;

							return LevelRange(min, max);
						}
					}
				});
			}

			//TRAITS
			if (kTraits < size) {
				data.traitFilters = parse_filters<Trait>(sections[kTraits], [&](std::string& entry_str) -> std::optional<Trait> {
					if (!entry_str.empty()) {
						switch (tolower(entry_str.at(0))) {
						case 'm':
							return SexTrait(RE::SEX::kMale);
						case 'f':
							return SexTrait(RE::SEX::kFemale);
						case 'u':
							return UniqueTrait();
						case 's':
							return SummonableTrait();
						case 'c':
							return ChildTrait();
						default:
							return std::nullopt;
						}
					}
				});
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
					auto chance = string::to_num<Chance::chance>(str);
					data.chanceFilters = FilterEntry<Chance>(FilterEntry<Chance>(Chance(chance)));
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
			logger::warn("	No .ini files with _DISTR suffix were found within the Data folder, aborting...");
			return { false, false };
		}

		logger::info("\t{} matching inis found", files.size());

		//initialize map
		for (size_t i = 0; i < RECORD::kTotal; i++) {
			configs[RECORD::add[i]] = DataVec{};
		}

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
				auto                                                                                                  truncatedPath = path.substr(5);  //strip "Data\\"
				for (auto& [key, entry] : *values) {
					try {
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
