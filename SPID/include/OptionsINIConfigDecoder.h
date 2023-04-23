#pragma once
#include "Configs.h"
#include "ConfigsCoding.h"

namespace Configs
{
	// Should try to find [SPID] section and read Config from there.
	struct OptionsINIConfigDecoder : INIConfigDecoder<Config>
	{
		Config decode(const std::filesystem::path& path);

    private:
		static constexpr std::string name(const Sections& section);

        [[nodiscard]] bool hasOption(const std::string& section, const std::string& name) const;
    };
}
