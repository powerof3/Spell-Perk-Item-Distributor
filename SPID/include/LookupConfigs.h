#pragma once

namespace RECORD
{
	enum TYPE
	{
		/// <summary>
		/// A generic form type that requries type inference.
		/// </summary>
		kForm = 0,

		kSpell,
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

	namespace detail
	{
		inline static constexpr std::array names{
			"Form"sv,
			"Spell"sv,
			"Perk"sv,
			"Item"sv,
			"Shout"sv,
			"LevSpell"sv,
			"Package"sv,
			"Outfit"sv,
			"Keyword"sv,
			"DeathItem"sv,
			"Faction"sv,
			"SleepOutfit"sv,
			"Skin"sv
		};
	}

	inline constexpr std::string_view GetTypeName(const TYPE aType)
	{
		return detail::names.at(aType);
	}

	template <typename T>
	constexpr TYPE GetType(const T& aType)
	{
		using namespace detail;
		return static_cast<TYPE>(std::distance(names.begin(), std::find(names.begin(), names.end(), aType)));
	}
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
		IndexOrCount            idxOrCount{ RandomCount(1, 1) };
		PercentChance           chance{ 100 };
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

	inline Map<RECORD::TYPE, DataVec> configs{};

	/// <summary>
	/// A list of RawExclusiveGroups that will be processed along with configs.
	/// </summary>
	inline ExclusiveGroupsVec exclusiveGroups{};

	std::pair<bool, bool> GetConfigs();
}
