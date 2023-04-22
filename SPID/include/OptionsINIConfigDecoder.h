#pragma once
#include "Configs.h"
#include "ConfigsCoding.h"

namespace Configs
{
	// Should try to find [SPID] section and read Config from there.
	struct OptionsINIConfigDecoder : INIConfigDecoder<Config>
	{
		Config decode(const std::filesystem::path& path)
		{
			ini.SetAllowKeyOnly();
			load(path);
			Config config;

			CSimpleIniA::TNamesDepend allSections;
			ini.GetAllSections(allSections);

			const bool hasSPIDSection = ini.SectionExists(name(Sections::kSPID).c_str());
			const bool hasGeneralSection = ini.SectionExists(name(Sections::kGeneral).c_str());
			const bool hasPlayerSection = ini.SectionExists(name(Sections::kPlayer).c_str());
			const bool hasNPCSection = ini.SectionExists(name(Sections::kNPC).c_str());

			const uint8_t specialSectionsCount = hasSPIDSection + hasGeneralSection + hasPlayerSection + hasNPCSection;

			const auto configSection = name(hasSPIDSection ? Sections::kSPID : Sections::kGeneral);

			config.schema = create<Schema>(ini.GetLongValue(configSection.c_str(), "schema", rawValue(defaultSchema)));
			config.path = path;
			config.name = path.filename().string();

			// If there are more sections than predefined sections, we must be in a Format::kSectionINI config.
			if (allSections.size() > specialSectionsCount) {
				config.format = Format::kSectionINI;
			} else if (has(config.schema, Schema::kFreeformFilters)) {
				config.format = Format::kFreeformINI;
			} else {  // Otherwise we must be opening legacy config.
				config.format = Format::kFixedINI;
			}

			// Set options
			if (hasOption(configSection, "skipConversion")) {
				enable(config.options, Options::kSkipSchemaUpgrade);
			}
			if (hasOption(configSection, "trace")) {
				enable(config.options, Options::kTrace);
			}

			// Set Sections
			if (hasSPIDSection) {
				enable(config.sections, Sections::kSPID);
			}
			if (hasGeneralSection) {
				enable(config.sections, Sections::kGeneral);
			}
			if (hasPlayerSection) {
				enable(config.sections, Sections::kPlayer);
			}
			if (hasNPCSection) {
				enable(config.sections, Sections::kNPC);
			}

			return config;
		}

	private:
		static constexpr std::string name(const Sections& section)
		{
			using Section = Configs::Sections;
			switch (section) {
			case Section::kGeneral:
				return "";
			case Section::kSPID:
				return "SPID";
			case Section::kPlayer:
				return "Player";
			case Section::kNPC:
				return "NPC";
			default:
				break;
			}
			return "";
		}

		[[nodiscard]] bool hasOption(const std::string& section, const std::string& name) const
		{
			return ini.GetBoolValue(section.c_str(), name.c_str(), ini.KeyExists(section.c_str(), name.c_str()));
		}
	};
}
