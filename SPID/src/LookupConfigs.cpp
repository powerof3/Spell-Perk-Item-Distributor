#include "LookupConfigs.h"
#include "OptionsINIConfigDecoder.h"

namespace Configs
{
	// to be moved to Clib util
	inline std::vector<std::filesystem::path> get_configs(const std::filesystem::path& a_folder,const std::string_view& a_suffix, const std::string_view& a_extension = ".ini")
	{
		std::vector<std::filesystem::path> configs{};
		const auto                         iterator = std::filesystem::directory_iterator(a_folder);
		for (const auto& entry : iterator) {
			if (entry.exists()) {
				if (const auto& path = entry.path(); !path.empty() && path.extension() == a_extension && entry.path().string().rfind(a_suffix) != std::string::npos) {
					configs.push_back(path);
				}
			}
		}

		std::ranges::sort(configs);

		return configs;
	}

	std::pair<bool, bool> loadINIs()
	{
		const auto files = get_configs("Data", "_DISTR"sv, ".ini"sv);

		if (files.empty()) {
			return { false, false };
		}
	    logger::info("{} matching config files found", files.size());
		bool shouldLogErrors{ false };

		for (const auto& file : files) {
			const auto name = file.filename().string();
			logger::info("\t\t Loading {}", name);
			try {
				OptionsINIConfigDecoder optionsDecoder;
				auto                    config = optionsDecoder.decode(file);
				FixedDataINIConfigDecoder decoder;
				decoder.decode(config);
				configs.push_back(config);
			} catch (...) {
				logger::warn("\t\tFailed to decode config {}", name);
				shouldLogErrors = true;
			}
		}

		return { true, shouldLogErrors };
	}

	std::pair<bool, bool> LoadConfigs()
	{
		logger::info("{:*^50}", "CONFIG");
		auto result = loadINIs();
		if (!result.first) {
			logger::warn("No SPID files with _DISTR suffix were found within the Data folder, aborting...");
			return { false, false };
		}
		return result;
	}
}
