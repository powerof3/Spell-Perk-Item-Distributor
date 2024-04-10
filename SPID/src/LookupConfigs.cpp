#include "LookupConfigs.h"
#include "DeathDistribution.h"
#include "ExclusiveGroups.h"
#include "LinkedDistribution.h"
#include "Parser.h"

namespace Configs
{
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

			std::pair<Data, std::optional<std::string>> parse_ini(const std::string& key, const std::string& value, const Path& path)
			{
				auto sanitized_value = sanitize(value);

				auto data = Parse<Data,
					DefaultKeyComponentParser,
					DistributableFormComponentParser,
					StringFiltersComponentParser,
					FormFiltersComponentParser,
					LevelFiltersComponentParser,
					TraitsFilterComponentParser,
					IndexOrCountComponentParser,
					ChanceComponentParser>(key, sanitized_value);

				data.path = path;

				if (sanitized_value != value) {
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
							if (ExclusiveGroups::INI::TryParse(key.pItem, entry, truncatedPath)) {
								continue;
							}

							if (LinkedDistribution::INI::TryParse(key.pItem, entry, truncatedPath)) {
								continue;
							}

							if (DeathDistribution::INI::TryParse(key.pItem, entry, truncatedPath)) {
								continue;
							}

							auto type = RECORD::GetType(key.pItem);
							if (type == RECORD::kTotal) {
								logger::warn("\t\tUnsupported Form type ({}): {} = {}"sv, key.pItem, key.pItem, entry);
								continue;
							}
							auto [data, sanitized_str] = detail::parse_ini(key.pItem, entry, truncatedPath);

							configs[type].emplace_back(data);

							if (sanitized_str) {
								oldFormatMap.emplace(key, std::make_pair(entry, *sanitized_str));
							}
						} catch (...) {
							logger::warn("\t\tFailed to parse entry [{} = {}]"sv, key.pItem, entry);
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
}
