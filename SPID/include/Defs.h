#pragma once

using FormIDPair = std::pair<
	std::optional<RE::FormID>,    // formID
	std::optional<std::string>>;  // modName

using FormOrEditorID = std::variant<
	FormIDPair,    // formID~modName
	std::string>;  // editorID

template <class T>
struct Filters
{
	std::vector<T> ALL{};
	std::vector<T> NOT{};
	std::vector<T> MATCH{};
};

using FormIDVec = std::vector<FormOrEditorID>;
using FormFiltersRaw = Filters<FormOrEditorID>;

using StringVec = std::vector<std::string>;
struct StringFilters : Filters<std::string>
{
	StringVec ANY{};
};

using FormOrMod = std::variant<RE::TESForm*,  // form
	const RE::TESFile*>;                 // mod
using FormVec = std::vector<FormOrMod>;
using FormFilters = Filters<FormOrMod>;

using ActorLevel = std::pair<std::uint16_t, std::uint16_t>;  // min/maxLevel
using SkillLevel = std::pair<
	std::uint32_t,                           // skill type
	std::pair<std::uint8_t, std::uint8_t>>;  // skill Level
using LevelFilters = std::pair<ActorLevel, std::vector<SkillLevel>>;

using IdxOrCount = std::int32_t;

struct Traits
{
	std::optional<RE::SEX> sex{};
	std::optional<bool> unique{};
	std::optional<bool> summonable{};
	std::optional<bool> child{};
};

using Chance = float;
using NPCCount = std::uint32_t;

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
	using Values = std::vector<std::pair<std::string, std::string>>;

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
		FormOrEditorID rawForm{};
		StringFilters stringFilters{};
		FormFiltersRaw rawFormFilters{};
		LevelFilters levelFilters{};
		Traits traits{};
		IdxOrCount idxOrCount{ 1 };
		Chance chance{ 100.0f };
		std::string path{};
	};
	using DataVec = std::vector<Data>;
}
using INIDataVec = INI::DataVec;

/// Custom ordering for keywords that ensures that dependent keywords are disitrbuted after the keywords that they depend on.
struct KeywordDependencySorter
{
	static bool sort(RE::BGSKeyword* a, RE::BGSKeyword* b);
};

template <class Form>
struct FormData
{
	Form* form{ nullptr };
	IdxOrCount idxOrCount{ 1 };
	StringFilters stringFilters{};
	FormFilters formFilters{};
	LevelFilters levelFilters{};
	Traits traits{};
	Chance chance{ 100 };
	NPCCount npcCount{ 0 };

	bool operator<(const FormData& a_rhs) const
	{
		if constexpr (std::is_same_v<RE::BGSKeyword, Form>) {
			return KeywordDependencySorter::sort(form, a_rhs.form);
		} else {
			return true;
		}
	}
};

template <class Form>
using FormDataVec = std::vector<FormData<Form>>;
