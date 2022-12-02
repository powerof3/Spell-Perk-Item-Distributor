#pragma once

#include "LookupFilters.h"

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

	template <class ValueType>
	struct RawFilterValue
	{
		ValueType value;
		bool negated;

		RawFilterValue(ValueType val, bool isNegated) :
			value(val), negated(isNegated) {} 
	};

	template<class ValueType>
	struct RawExpression
	{
		std::vector<ValueType> entries;
	};

	template <class T>
	using RawFilterEntries = RawExpression<RawFilterValue<T>>;

	template<class T>
	using RawFilters = RawExpression<RawFilterEntries<T>>;

	struct Data
	{

		FormOrEditorID                  rawForm{};
		RawFilters<Filter::StringValue> stringFilters{};
		RawFilters<FormOrEditorID>      idFilters{};
		RawFilters<Filter::LevelRange>  levelFilters{};
		RawFilters<Filter::Trait>       traitFilters{};
		Filter::Chance                  chanceFilters{ 100 };
		IdxOrCount                      idxOrCount{ 1 };
		std::string                     path{};
	};
	using DataVec = std::vector<Data>;

	inline StringMap<DataVec> configs{};

	std::pair<bool, bool> GetConfigs();
}
