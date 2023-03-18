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

		RawFilterEntries parse_entries(const std::string& filter_str, const std::function<const FilterValue*(const std::string&)> parser)
		{
			RawFilterEntries entries;
			auto             entries_str = distribution::split_entry(filter_str, "+");

			for (auto& entry_str : entries_str) {
				bool isNegated = false;

				if (entry_str.at(0) == '-') {
					entry_str.erase(0, 1);
					isNegated = true;
				}
				if (const auto res = parser(entry_str)) {
					entries.emplace_back(RawFilterValue(res, isNegated));
				}
			}
			return entries;
		}

		void parse_filters(const std::string& expression_str, RawFilters &filters, const std::function<const FilterValue*(const std::string&)> parser)
		{
            const auto filters_str = distribution::split_entry(expression_str, ",");
			for (const auto& filter_str : filters_str) {
				filters.emplace_back(parse_entries(filter_str, parser));
			}
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
				parse_filters(sections[kStrings], data.stringFilters, [](const std::string& entry_str)->StringValue* {
					if (entry_str.at(0) == '*') {
						std::string wildcard = entry_str;
						wildcard.erase(0, 1);
						return new Wildcard(wildcard);
					}
					return new Match(entry_str);
				});
			}

			//FILTER FORMS
			if (kFilterIDs < size) {
				parse_filters(sections[kFilterIDs], data.idFilters, [](const std::string& entry_str) {
						return new FormFilterValue<FormOrEditorID>(distribution::get_record(entry_str));
				});
			}

			//LEVEL
			if (kLevel < size) {
				// Matches all types of level and skill filters
                const std::regex regex(R"(^(?:(w)?(\d+)\((\d+)?(?:\/(\d+)?)?\)|(\d+)?(?:\/(\d+)?)?)$)", std::regex_constants::optimize);
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

				parse_filters(sections[kLevel], data.levelFilters, [&](const std::string& entry_str)-> LevelRange* {
					std::smatch matches;
					if (!entry_str.empty() && std::regex_search(entry_str, matches, regex) && matches.size() >= kTotal) {
						if (matches[kSkill].length() > 0) {  // skills
							if (const auto skill = string::to_num<SkillLevelRange::Skill>(matches[kSkill].str()); skill < SkillLevelRange::Skills::kTotal) {
                                const auto min = matches[kSkillMin].length() > 0 ? string::to_num<SkillLevelRange::Level>(matches[kSkillMin].str()) : LevelRange::MinLevel;
                                const auto max = matches[kSkillMax].length() > 0 ? string::to_num<SkillLevelRange::Level>(matches[kSkillMax].str()) : LevelRange::MaxLevel;
								if (matches[kModifier].length() == 0) {
									return new SkillLevelRange(skill, min, max);
								}
                                if (matches[kModifier].str().at(0) == 'w') {
                                    return new SkillWeightRange(skill, min, max);
                                }
                                buffered_logger::warn("\tUnrecognized skill modifier in filter (\'{}\'): {}", matches[kModifier].str().at(0), entry_str);
                                return nullptr;
                            }
							buffered_logger::warn("\tInvalid skill filter. Skill Type must be a number in range [0; 17]: {}", entry_str);
							return nullptr;
						}
                        // levels
                        const auto min = matches[kLevelMin].length() > 0 ? string::to_num<LevelRange::Level>(matches[kLevelMin].str()) : LevelRange::MinLevel;
                        const auto max = matches[kLevelMax].length() > 0 ? string::to_num<LevelRange::Level>(matches[kLevelMax].str()) : LevelRange::MaxLevel;

                        return new LevelRange(min, max);
                    }
					buffered_logger::warn("\tInvalid level or skill filter: {}", entry_str);
					return nullptr;
				});
			}

			//TRAITS
			if (kTraits < size) {
				parse_filters(sections[kTraits], data.traitFilters, [&](const std::string& entry_str) -> Trait* {
					if (!entry_str.empty()) {
                        switch (const auto trait = tolower(entry_str.at(0))) {
						case 'm':
							return new SexTrait(RE::SEX::kMale);
						case 'f':
							return new SexTrait(RE::SEX::kFemale);
						case 'u':
							return new UniqueTrait();
						case 's':
							return new SummonableTrait();
						case 'c':
							return new ChildTrait();
						default:
							buffered_logger::warn("\tUnrecognized trait filter (\'{}\'): {}", trait, entry_str);
							return nullptr;
						}
					}
					buffered_logger::critical("\tUnexpectedly found an empty trait filter. Please report this issue if you see this log :)");
					return nullptr;
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
					data.chanceFilters.maximum = string::to_num<Chance::chance>(str);
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
