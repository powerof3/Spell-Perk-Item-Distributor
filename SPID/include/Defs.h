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

		kTotal
	};

	inline constexpr std::array add { "Spell", "Perk", "Item", "Shout", "LevSpell", "Package", "Outfit", "Keyword", "DeathItem", "Faction" };
	inline constexpr std::array remove { "-Spell", "-Perk", "-Item", "-Shout", "-LevSpell", "-Package", "-Outfit", "-Keyword", "-DeathItem", "-Faction" };
}

namespace TRAITS
{
	enum : std::uint32_t
	{
		kSex,
		kUnique,
		kSummonable
	};
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
		kItemCount,
		kChance
	};

	using Values = std::vector<std::pair<std::string, std::string>>;

	inline bool is_mod_name(const std::string& a_str)
	{
		return a_str.rfind(".esp") != std::string::npos || a_str.rfind(".esl") != std::string::npos || a_str.rfind(".esm") != std::string::npos;
	}
}

namespace DATA
{
	enum TYPE : std::uint32_t
	{
		kStrings = 0,
		kFilterForms,
		kLevel,
		kTraits,
		kChance,
		kItemCount
	};
}

using FormIDPair = std::pair<
	std::optional<RE::FormID>,
	std::optional<std::string>>;
using FormIDPairVec = std::vector<FormIDPair>;
using StringVec = std::vector<std::string>;
using FormVec = std::vector<
	std::variant<RE::TESForm*, const RE::TESFile*>>;
using FormOrEditorID = std::variant<FormIDPair, std::string>;

using ActorLevel = std::pair<std::uint16_t, std::uint16_t>;
using SkillLevel = std::pair<
	std::uint32_t,
	std::pair<std::uint8_t, std::uint8_t>>;
using ItemCount = std::int32_t;
using Traits = std::tuple<
    std::optional<RE::SEX>,
	std::optional<bool>,
	std::optional<bool>>;
using Chance = float;
using NPCCount = std::uint32_t;

using INIData = std::tuple<
	std::array<StringVec, 4>,
	std::array<FormIDPairVec, 3>,
	std::pair<ActorLevel, std::vector<SkillLevel>>,
	Traits,
	ItemCount,
	Chance>;
using INIDataMap = std::map<FormOrEditorID, std::vector<INIData>>;

using FormData = std::tuple<
	std::array<StringVec, 4>,
	std::array<FormVec, 3>,
	std::pair<ActorLevel, std::vector<SkillLevel>>,
	Traits,
	Chance,
	ItemCount>;
template <class T>
using FormCount = std::pair<T*, ItemCount>;
template <class T>
using FormDataMap = std::map<T*, std::pair<NPCCount, std::vector<FormData>>>;
