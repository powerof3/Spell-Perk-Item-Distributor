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

		kTotal
	};

	inline constexpr std::array add = { "Spell", "Perk", "Item", "Shout", "LevSpell", "Package", "Outfit", "Keyword", "DeathItem" };
	inline constexpr std::array remove = { "-Spell", "-Perk", "-Item", "-Shout", "-LevSpell", "-Package", "-Outfit", "-Keyword", "-DeathItem" };
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
}
using INI_TYPE = INI::TYPE;

namespace DATA
{
	enum TYPE : std::uint32_t
	{
		kForm = 0,
		kItemCount,
		kStrings = kItemCount,
		kFilterForms,
		kLevel,
		kTraits,
		kChance,
		kNPCCount
	};
}
using DATA_TYPE = DATA::TYPE;


using FormIDPair = std::pair<
	std::optional<RE::FormID>,
	std::optional<std::string>>;
using FormIDPairVec = std::vector<FormIDPair>;
using StringVec = std::vector<std::string>;
using FormVec = std::vector<
	std::variant<RE::TESForm*, const RE::TESFile*>>;

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

using INIData = std::tuple<
	std::variant<FormIDPair, std::string>,
	std::array<StringVec, 4>,
	std::array<FormIDPairVec, 3>,
	std::pair<ActorLevel, SkillLevel>,
	Traits,
	ItemCount,
	Chance>;
using INIDataVec = std::vector<INIData>;

using NPCCount = std::uint32_t;

template <class Form>
using FormCountPair = std::pair<Form*, ItemCount>;
template <class Form>
using FormData = std::tuple<
	FormCountPair<Form>,
	std::array<StringVec, 4>,
	std::array<FormVec, 3>,
	std::pair<ActorLevel, SkillLevel>,
	Traits,
	Chance,
	NPCCount>;
template <class Form>
using FormDataVec = std::vector<FormData<Form>>;
