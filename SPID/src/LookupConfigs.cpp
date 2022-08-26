#include "LookupConfigs.h"

bool INI::Read()
{
	logger::info("{:*^30}", "INI");

	std::vector<std::string> files;

	auto constexpr folder = R"(Data\)";
	for (const auto& entry : std::filesystem::directory_iterator(folder)) {
		if (entry.exists() && !entry.path().empty() && entry.path().extension() == ".ini"sv) {
			if (const auto path = entry.path().string(); path.rfind("_DISTR") != std::string::npos) {
				files.push_back(path);
			}
		}
	}

	if (files.empty()) {
		logger::warn("	No .ini files with _DISTR suffix were found within the Data folder, aborting...");
		return false;
	}

	logger::info("	{} matching inis found", files.size());

	//initialize map
	for (size_t i = 0; i < RECORD::kTotal; i++) {
		configs[RECORD::add[i]] = INIDataMap{};
	}

	for (auto& path : files) {
		logger::info("	INI : {}", path);

		CSimpleIniA ini;
		ini.SetUnicode();
		ini.SetMultiKey();

		if (const auto rc = ini.LoadFile(path.c_str()); rc < 0) {
			logger::error("	couldn't read INI");
			continue;
		}

		if (auto values = ini.GetSection(""); values) {
			std::multimap<CSimpleIniA::Entry, std::pair<std::string, std::string>, CSimpleIniA::Entry::LoadOrder> oldFormatMap;

			for (auto& [key, entry] : *values) {
				auto [recordID, data, sanitized_str] = parse_ini(key.pItem, entry);

				configs[key.pItem][recordID].emplace_back(data);

				if (sanitized_str) {
					oldFormatMap.emplace(key, std::make_pair(entry, *sanitized_str));
				}
			}

			if (!oldFormatMap.empty()) {
				logger::info("		sanitizing {} entries", oldFormatMap.size());

				for (auto& [key, entry] : oldFormatMap) {
					auto& [original, sanitized] = entry;
					ini.DeleteValue("", key.pItem, original.c_str());
					ini.SetValue("", key.pItem, sanitized.c_str(), key.pComment, false);
				}

				(void)ini.SaveFile(path.c_str());
			}
		}
	}

	return true;
}
