#include "LookupConfigs.h"

std::pair<bool, bool> INI::Read()
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
		configs[RECORD::add[i]] = INIDataVec{};
	}

	bool shouldLogErrors{ false };

	for (auto& path : files) {
		logger::info("\tINI : {}", path);

		CSimpleIniA ini;
		ini.SetUnicode();
		ini.SetMultiKey();

		if (const auto rc = ini.LoadFile(path.c_str()); rc < 0) {
			logger::error("\t\tcouldn't read INI");
			continue;
		}

		string::replace_first_instance(path, "Data\\", "");

		if (auto values = ini.GetSection(""); values && !values->empty()) {
			std::multimap<CSimpleIniA::Entry, std::pair<std::string, std::string>, CSimpleIniA::Entry::LoadOrder> oldFormatMap;
			for (auto& [key, entry] : *values) {
				try {
					auto [data, sanitized_str] = parse_ini(key.pItem, entry, path);
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
