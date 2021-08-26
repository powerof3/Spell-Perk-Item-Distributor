#pragma once

using FormIDPair = std::pair<RE::FormID, std::string>;
using FormIDVec = std::vector<RE::FormID>;
using StringVec = std::vector<std::string>;
using FormVec = std::vector<RE::TESForm*>;

using ActorLevel = std::pair<std::uint16_t, std::uint16_t>;
using SkillLevel = std::pair<std::uint32_t, std::pair<std::uint8_t, std::uint8_t>>;
using ItemCount = std::int32_t;
using Chance = float;
using NPCCount = std::uint32_t;

using INIData = std::tuple<
	std::variant<FormIDPair, std::string>,
	std::array<StringVec, 4>,
	std::array<FormIDVec, 3>,
	std::pair<ActorLevel, SkillLevel>,
	RE::SEX,
	ItemCount,
	Chance>;
using INIDataVec = std::vector<INIData>;

template <class Form>
using FormCountPair = std::pair<Form*, ItemCount>;
template <class Form>
using FormData = std::tuple<
	FormCountPair<Form>,
	std::array<StringVec, 4>,
	std::array<FormVec, 3>,
	std::pair<ActorLevel, SkillLevel>,
	RE::SEX,
	Chance,
	NPCCount>;
template <class Form>
using FormDataVec = std::vector<FormData<Form>>;

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
		kGender,
		kItemCount,
		kChance
	};
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
		kGender,
		kChance,
		kNPCCount
	};
}
using DATA_TYPE = DATA::TYPE;

namespace ACTOR_LEVEL
{
	enum TYPE : std::uint32_t
	{
		kMin = 0,
		kMax
	};

	inline constexpr auto MAX = std::numeric_limits<std::uint16_t>::max();
}
using A_LEVEL = ACTOR_LEVEL::TYPE;

namespace SKILL_LEVEL
{
	enum TYPE : std::uint32_t
	{
		kType = 0,
		kMin,
		kMax
	};

	inline constexpr auto TYPE_MAX = std::numeric_limits<std::uint32_t>::max();
	inline constexpr auto VALUE_MAX = std::numeric_limits<std::uint8_t>::max();
}
using S_LEVEL = SKILL_LEVEL::TYPE;
