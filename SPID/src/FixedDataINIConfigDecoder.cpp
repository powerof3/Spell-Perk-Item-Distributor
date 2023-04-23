#include "FixedDataINIConfigDecoder.h"

namespace Configs
{
	namespace detail
	{
		using namespace filters;

		enum TYPE : std::uint32_t
		{
			kFormIDPair = 0,
			kFormID = kFormIDPair,
			kStrings,
			kESP = kStrings,
			kFilterIDs,
			kLevel,
			kTraits,
			kIdxOrCount,
			kChance
		};

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

			//convert 00012345 formIDs to 0x12345
			static const srell::regex re_formID(R"(\b00+([0-9a-fA-F]{1,6})\b)", srell::regex_constants::optimize);
			newValue = srell::regex_replace(newValue, re_formID, "0x$1");

			//strip leading zeros
			static const srell::regex re_zeros(R"((0x00+)([0-9a-fA-F]+))", srell::regex_constants::optimize);
			newValue = srell::regex_replace(newValue, re_zeros, "0x$2");

			//NOT to hyphen
			string::replace_all(newValue, "NOT ", "-");

			// These TODOs are no longer needed here, as all of these will be taken care of by higher level expression analyzer

			// TODO: Sanitize level filters with a single number so that they will be replaced with an one-sided range: 5(20) => 5(20/)

			// TODO: Sanitize () to be [] to support future ability to group filters with ()

			// TODO: Santize "-" filters to properly behave in the new system.
			// "A,B+C,-D,-E" should become "A-D-E,B+C-D-E"

			return newValue;
		}

		NPCAndExpression* parse_entries(const std::string& filter_str, const std::function<NPCFilter*(const std::string&)> parser)
		{
			const auto entries = new NPCAndExpression();
			auto       entries_str = distribution::split_entry(filter_str, "+");
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

		NPCOrExpression* parse_filters(const std::string& expression_str, const std::function<NPCFilter*(const std::string&)> parser)
		{
			const auto expression = new NPCOrExpression();
			const auto filters_str = distribution::split_entry(expression_str, ",");
			for (const auto& filter_str : filters_str) {
				expression->emplace_back(parse_entries(filter_str, parser));
			}

			return expression;
		}

		void parse_ini(const Data::TypeHint& type, const std::string& a_value, Config& config)
		{
			const auto sanitized_value = sanitize(a_value);
			const auto sections = string::split(sanitized_value, "|");

			const auto size = sections.size();

			FormOrEditorID rawForm;
			IdxOrCount     idxOrCount = type == Type::kPackage ? 0 : 1;  // reuse item count for package stack index
			auto           filters = new NPCAndExpression();

			//[FORMID/ESP] / EDITORID
			if (kFormID < size) {
				rawForm = distribution::get_record(sections[kFormID]);
			}

			//KEYWORDS
			if (kStrings < size) {
				const auto stringFilters = parse_filters(sections[kStrings], [](const std::string& entry_str) -> NPCFilter* {
					if (entry_str.at(0) == '*') {
						std::string wildcard = entry_str;
						wildcard.erase(0, 1);
						return new WildcardFilter(wildcard);
					}
					return new MatchFilter(entry_str);
				});

				filters->emplace_back(stringFilters);
			}

			//FILTER FORMS
			if (kFilterIDs < size) {
				const auto idFilters = parse_filters(sections[kFilterIDs], [](const std::string& entry_str) {
					return new UnknownFormIDFilter(distribution::get_record(entry_str));
				});

				filters->emplace_back(idFilters);
			}

			//LEVEL
			if (kLevel < size) {
				// Matches all types of level and skill filters
				const std::regex regex(R"(^(?:(w)?(\d+)\((\d+)?(\/(\d+)?)?\)|(\d+)?(\/(\d+)?)?)$)", std::regex_constants::optimize);
				// Indices of matched groups
				enum
				{
					kMatch = 0,
					kModifier,
					kSkill,
					kSkillMin,
					kSkillRange,  // marker for one-sided range
					kSkillMax,
					kLevelMin,
					kLevelRange,  // marker for one-sided range
					kLevelMax,

					kTotal

				};

				const auto levelFilters = parse_filters(sections[kLevel], [&](const std::string& entry_str) -> NPCFilter* {
					std::smatch matches;
					if (!entry_str.empty() && std::regex_search(entry_str, matches, regex) && matches.size() >= kTotal) {
						if (matches[kSkill].length() > 0) {  // skills
							if (const auto skill = string::to_num<SkillFilter::Skill>(matches[kSkill].str()); skill < SkillFilter::Skills::kTotal) {
								const auto min = matches[kSkillMin].length() > 0 ? string::to_num<Level>(matches[kSkillMin].str()) : LevelFilter::MinLevel;
								const auto max = matches[kSkillMax].length() > 0 ? string::to_num<Level>(matches[kSkillMax].str()) : matches[kSkillRange].length() > 0 ? LevelFilter::MaxLevel :
                                                                                                                                                                         min;
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
						const auto max = matches[kLevelMax].length() > 0 ? string::to_num<Level>(matches[kLevelMax].str()) : matches[kLevelRange].length() > 0 ? LevelFilter::MaxLevel :
                                                                                                                                                                 min;

						return new LevelFilter(min, max);
					}
					buffered_logger::warn("\tInvalid level or skill filter: {}", entry_str);
					return nullptr;
				});

				filters->emplace_back(levelFilters);
			}

			//TRAITS
			if (kTraits < size) {
				const auto traitFilters = parse_filters(sections[kTraits], [&](const std::string& entry_str) -> NPCFilter* {
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
				filters->emplace_back(traitFilters);
			}

			//ITEMCOUNT/INDEX
			if (kIdxOrCount < size) {
				if (const auto& str = sections[kIdxOrCount]; distribution::is_valid_entry(str)) {
					idxOrCount = string::to_num<std::int32_t>(str);
				}
			}

			//CHANCE
			if (kChance < size) {
				if (const auto& str = sections[kChance]; distribution::is_valid_entry(str)) {
					const auto chanceFilters = new ChanceFilter(string::to_num<chance>(str));
					filters->emplace_back(chanceFilters);
				}
			}

			// If there were no filters we gonna reset them.
			if (filters->isEmpty()) {
				delete filters;
				filters = nullptr;
			}

			config.data.emplace_back(idxOrCount, rawForm, type, filters);
		}
	}

	void FixedDataINIConfigDecoder::decode(Config& config)
	{
		ini.SetUnicode();
		ini.SetMultiKey();

		if (!load(config.path)) {
			throw InvalidINIException();
		}

		if (auto values = ini.GetSection(""); values && !values->empty()) {
			for (auto& [key, entry] : *values) {
				if (const auto type = fromRawType(key.pItem); type != kInvalid) {
					detail::parse_ini(type, entry, config);
				}
			}
		}
	}
}
