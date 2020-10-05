#pragma once

#include "RE/Skyrim.h"
#include "REL/Relocation.h"
#include "SKSE/SKSE.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <SimpleIni.h>

namespace logger = SKSE::log;
#define DLLEXPORT __declspec(dllexport)


namespace OLD_DATA
{
	enum TYPE : std::uint32_t
	{
		kFormID = 0,
		kESP,
		kKeywords,
		kFactions,
		kItemCount
	};
}
using OLD_TYPE = OLD_DATA::TYPE;


namespace INI_DATA
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
using INI_TYPE = INI_DATA::TYPE;


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


using FormIDPair = std::pair<RE::FormID, std::string>;
using StringVec = std::vector<std::string>;

using FormIDVec = std::vector<RE::FormID>;
using FormVec = std::vector<RE::TESForm*>;

using ActorLevel = std::pair<std::uint16_t, std::uint16_t>;
using SkillLevel = std::pair<std::uint32_t, std::pair<std::uint8_t, std::uint8_t>>;
using ItemCount = std::int32_t;
using Chance = std::uint32_t;
using NPCCount = std::uint32_t;


using INIData = std::tuple<FormIDPair, StringVec, FormIDVec, std::pair<ActorLevel, SkillLevel>, RE::SEX, ItemCount, Chance>;
using INIDataVec = std::vector<INIData>;

static INIDataVec spellsINI;
static INIDataVec perksINI;
static INIDataVec itemsINI;
static INIDataVec shoutsINI;
static INIDataVec levSpellsINI;
static INIDataVec packagesINI;

template <class Form>
using FormCountPair = std::pair<Form*, ItemCount>;
template <class Form>
using FormData = std::tuple<FormCountPair<Form>, StringVec, FormVec, std::pair<ActorLevel, SkillLevel>, RE::SEX, Chance, NPCCount>;
template <class Form>
using FormDataVec = std::vector<FormData<Form>>;

static FormDataVec<RE::SpellItem> spells;
static FormDataVec<RE::BGSPerk> perks;
static FormDataVec<RE::TESBoundObject> items;
static FormDataVec<RE::TESLevSpell> levSpells;
static FormDataVec<RE::TESShout> shouts;
static FormDataVec<RE::TESPackage> packages;
