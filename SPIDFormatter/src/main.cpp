#include "main.h"

bool INI::Format(TYPE a_type)
{
	std::vector<std::string> configs;

	for (const auto& entry : std::filesystem::directory_iterator("./")) {
		if (entry.exists() && !entry.path().empty() && entry.path().extension() == ".ini") {
			if (const auto path = entry.path().string(); path.rfind("_DISTR") != std::string::npos) {
				configs.push_back(path);
			}
		}
	}

	if (configs.empty()) {
		std::cout << "\nUnable to find _DISTR files in the current directory. Make sure you're running this from the Skyrim Data folder!\n";
		return false;
	}

	auto size = configs.size();
	a_type == INI::kUpgrade ? std ::cout << "\nUpgrading " << size << " INI files...\n\n" : std ::cout << "\nDowngrading " << size << " INI files...\n\n";

	for (auto& path : configs) {
		detail::replace_first_instance(path, "./", "");
		std::cout << "ini : " << path << "\n";

		CSimpleIniA ini;
		ini.SetUnicode();
		ini.SetMultiKey();

		if (const auto rc = ini.LoadFile(path.c_str()); rc < 0) {
			std::cout << "	Unable to load INI, skipping... \n";
			continue;
		}

		if (auto values = ini.GetSection(""); values) {
			std::multimap<CSimpleIniA::Entry, std::pair<std::string, std::string>, CSimpleIniA::Entry::LoadOrder> oldFormatMap;

			for (auto& [key, entry] : *values) {
				auto sanitized_str =
					a_type == INI::kUpgrade ? detail::upgrade(entry) : detail::downgrade(entry);
				if (sanitized_str != entry) {
					oldFormatMap.emplace(key, std::make_pair(entry, sanitized_str));
				}
			}

			if (!oldFormatMap.empty()) {
				bool doOnce = false;
				for (auto [key, entry] : oldFormatMap) {
					auto& [original, sanitized] = entry;
					if (a_type == INI::kDowngrade) {
						if (const auto count = std::ranges::count_if(sanitized, [](const char c) { return c == '~'; }); count > 0) {
							if (!doOnce) {
								std::cout << "	Incompatible filter(s) detected (ie. 0x123~MyMod.esp). These must be removed manually\n";
								doOnce = true;
							}
							std::cout << "		" << sanitized << "\n ";
						}
					}

					ini.DeleteValue("", key.pItem, original.c_str());
					ini.SetValue("", key.pItem, sanitized.c_str(), key.pComment, false);
				}
				(void)ini.SaveFile(path.c_str());

				std::cout << "	Sanitized " << oldFormatMap.size() << " entries \n";
			}
		}
	}

	return true;
}

int main()
{
	do {
		FreeConsole();
	} while (!AllocConsole());

	FILE* fp;
	freopen_s(&fp, "CONIN$", "r", stdin);
	freopen_s(&fp, "CONOUT$", "w", stdout);

	std::cout.clear();
	std::cin.clear();

	printf("SPID INI Formatter { 1 to upgrade INIs to new 5.0 format , 2 to downgrade INIs }\n\n");

	std::int32_t formatType{ INI::kInvalid };
	do {
		std::cin.clear();
		std::cout << "Enter format type. [1/2] : ";
		std::cin >> formatType;
		std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
	} while (std::cin.fail() || formatType != INI::kUpgrade && formatType != INI::kDowngrade);

	INI::Format(static_cast<INI::TYPE>(formatType));

	std::cin.clear();
	std::cout << "\nPress any key to exit\n";
	_getch();

	if (fp) {
		fclose(fp);
	}
	FreeConsole();

	return 0;
}
