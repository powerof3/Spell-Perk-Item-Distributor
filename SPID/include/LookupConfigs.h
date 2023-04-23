#pragma once
#include "Configs.h"
#include "ConfigsCoding.h"
#include "FixedDataINIConfigDecoder.h"
#include "FreeformDataINIConfigDecoder.h"
#include "SectionedDataINIConfigDecoder.h"

namespace RECORD
{
	using TYPE = Configs::Type;

	inline constexpr std::array add{ "Spell"sv, "Perk"sv, "Item"sv, "Shout"sv, "LevSpell"sv, "Package"sv, "Outfit"sv, "Keyword"sv, "DeathItem"sv, "Faction"sv, "SleepOutfit"sv, "Skin"sv };
	inline constexpr std::array remove{ "-Spell"sv, "-Perk"sv, "-Item"sv, "-Shout"sv, "-LevSpell"sv, "-Package"sv, "-Outfit"sv, "-Keyword"sv, "-DeathItem"sv, "-Faction"sv, "-SleepOutfit"sv, "-Skin"sv };
}

namespace Configs
{
	inline std::vector<Config> configs{};

	std::pair<bool, bool> LoadConfigs();
}

// Factories
namespace Configs
{
	namespace Factory
	{
		inline std::unique_ptr<DataConfigDecoder> makeDecoder(const Config& config)
		{
			switch (config.format) {
			case Format::kFreeformINI:
				return std::make_unique<FreeformDataINIConfigDecoder>();
			case Format::kFixedINI:
				return std::make_unique<FixedDataINIConfigDecoder>();
			case Format::kSectionINI:
				return std::make_unique<SectionedDataINIConfigDecoder>();
			}
			return std::unique_ptr<DataINIConfigDecoder>{};
		}
	}

}
