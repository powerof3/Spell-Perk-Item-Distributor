#pragma once

namespace RECORD
{
	enum TYPE
	{
		kSpell = 0,
		kPerk,
		kItem,
		kShout,
		kLevSpell,
		kPackage,
		kOutfit,
		kKeyword,
		kDeathItem,
		kFaction,
		kSleepOutfit,
		kSkin,

		kTotal
	};

	inline constexpr std::array add{ "Spell"sv, "Perk"sv, "Item"sv, "Shout"sv, "LevSpell"sv, "Package"sv, "Outfit"sv, "Keyword"sv, "DeathItem"sv, "Faction"sv, "SleepOutfit"sv, "Skin"sv };
	inline constexpr std::array remove{ "-Spell"sv, "-Perk"sv, "-Item"sv, "-Shout"sv, "-LevSpell"sv, "-Package"sv, "-Outfit"sv, "-Keyword"sv, "-DeathItem"sv, "-Faction"sv, "-SleepOutfit"sv, "-Skin"sv };
}

namespace INI
{
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

	struct Data
	{
		FormOrEditorID          rawForm{};
		StringFilters           stringFilters{};
		Filters<FormOrEditorID> rawFormFilters{};
		LevelFilters            levelFilters{};
		Traits                  traits{};
		IdxOrCount              idxOrCount{ 1 };
		Chance                  chance{ 100 };
		std::string             path{};
	};

	struct RawExclusiveGroup
	{
		std::string name{};

		/// Raw filters in RawExclusiveGroup only use NOT and MATCH, there is no meaning for ALL, so it's ignored.
		Filters<FormOrEditorID> rawFormFilters{};
		std::string             path{};
	};

	using DataVec = std::vector<Data>;
	using ExclusiveGroupsVec = std::vector<RawExclusiveGroup>;

	inline StringMap<DataVec> configs{};

	/// <summary>
	/// A list of RawExclusiveGroups that will be processed along with configs.
	/// </summary>
	inline ExclusiveGroupsVec exclusiveGroups{};

	std::pair<bool, bool> GetConfigs();
}
