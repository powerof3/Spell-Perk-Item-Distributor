#include "LookupConfigs.h"
#include "LookupFilters.h"

namespace INI
{
	namespace detail
	{
		using namespace filters;
		using namespace SPID;

		std::string sanitize(const std::string& a_value)
		{
			std::string newValue = a_value;

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

			// TODO: Sanitize () to be [] to support future ability to group filters with ()

			// TODO: Santize "-" filters to properly behave in the new system.
			// "A,B+C,-D,-E" should become "A-D-E,B+C-D-E"

			return newValue;
		}

		AndExpression* parse_entries(const std::string& filter_str, const std::function<Filter*(const std::string&)> parser)
		{
			auto entries = new AndExpression();
			auto entries_str = distribution::split_entry(filter_str, "+");
			// TODO: Allow omitting "+" when there is already a "-" (e.g. instead of "A+-B" allow "A-B")

			for (auto& entry_str : entries_str) {
				bool isNegated = false;

				if (entry_str.at(0) == '-') {
					entry_str.erase(0, 1);
					isNegated = true;
				}
				if (const auto res = parser(entry_str)) {
					if (isNegated) {
						entries->emplace_back(new Negated(res));
					} else {
						entries->emplace_back(res);
					}
				}
			}
			return entries;
		}

		OrExpression* parse_filters(const std::string& expression_str, const std::function<Filter*(const std::string&)> parser)
		{
			const auto expression = new OrExpression();
			const auto filters_str = distribution::split_entry(expression_str, ",");
			for (const auto& filter_str : filters_str) {
				expression->emplace_back(parse_entries(filter_str, parser));
			}

			return expression;
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
				data.stringFilters = parse_filters(sections[kStrings], [](const std::string& entry_str) -> Filter* {
					if (entry_str.at(0) == '*') {
						std::string wildcard = entry_str;
						wildcard.erase(0, 1);
						return new WildcardFilter(wildcard);
					}
					return new MatchFilter(entry_str);
				});
			}

			//FILTER FORMS
			if (kFilterIDs < size) {
				data.idFilters = parse_filters(sections[kFilterIDs], [](const std::string& entry_str) {
					return new UnknownFormIDFilter(distribution::get_record(entry_str));
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

				data.levelFilters = parse_filters(sections[kLevel], [&](const std::string& entry_str) -> Filter* {
					std::smatch matches;
					if (!entry_str.empty() && std::regex_search(entry_str, matches, regex) && matches.size() >= kTotal) {
						if (matches[kSkill].length() > 0) {  // skills
							if (const auto skill = string::to_num<SkillFilter::Skill>(matches[kSkill].str()); skill < SkillFilter::Skills::kTotal) {
								const auto min = matches[kSkillMin].length() > 0 ? string::to_num<Level>(matches[kSkillMin].str()) : LevelFilter::MinLevel;
								const auto max = matches[kSkillMax].length() > 0 ? string::to_num<Level>(matches[kSkillMax].str()) : LevelFilter::MaxLevel;
								if (matches[kModifier].length() == 0) {
									return new SkillFilter(skill, min, max);
								}
								if (matches[kModifier].str().at(0) == 'w') {
									return new SkillWeightFilter(skill, min, max);
								}
								buffered_logger::warn("\tUnrecognized skill modifier in filter (\'{}\'): {}", matches[kModifier].str().at(0), entry_str);
								return nullptr;
							}
							buffered_logger::warn("\tInvalid skill filter. Skill Type must be a number in range [0; 17]: {}", entry_str);
							return nullptr;
						}
						// levels
						const auto min = matches[kLevelMin].length() > 0 ? string::to_num<Level>(matches[kLevelMin].str()) : LevelFilter::MinLevel;
						const auto max = matches[kLevelMax].length() > 0 ? string::to_num<Level>(matches[kLevelMax].str()) : LevelFilter::MaxLevel;

						return new LevelFilter(min, max);
					}
					buffered_logger::warn("\tInvalid level or skill filter: {}", entry_str);
					return nullptr;
				});
			}

			//TRAITS
			if (kTraits < size) {
				data.traitFilters = parse_filters(sections[kTraits], [&](const std::string& entry_str) -> Filter* {
					if (!entry_str.empty()) {
						switch (const auto trait = tolower(entry_str.at(0))) {
						case 'm':
							return new SexFilter(RE::SEX::kMale);
						case 'f':
							return new SexFilter(RE::SEX::kFemale);
						case 'u':
							return new UniqueFilter();
						case 's':
							return new SummonableFilter();
						case 'c':
							return new ChildFilter();
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
					data.chanceFilters.value = string::to_num<chance>(str);
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
