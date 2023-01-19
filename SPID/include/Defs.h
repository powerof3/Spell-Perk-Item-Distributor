#pragma once

template <class K, class D>
using Map = ankerl::unordered_dense::map<K, D>;
template <class K>
using Set = ankerl::unordered_dense::set<K>;

struct string_hash
{
	using is_transparent = void;  // enable heterogeneous overloads
	using is_avalanching = void;  // mark class as high quality avalanching hash

	[[nodiscard]] std::uint64_t operator()(std::string_view str) const noexcept
	{
		return ankerl::unordered_dense::hash<std::string_view>{}(str);
	}
};

template <class D>
using StringMap = ankerl::unordered_dense::map<std::string, D, string_hash, std::equal_to<>>;

// Record = FormOrEditorID|StringFilters|RawFormFilters|LevelFilters|Traits|IdxOrCount|Chance

using FormModPair = std::pair<
	std::optional<RE::FormID>,    // formID
	std::optional<std::string>>;  // modName

using FormOrEditorID = std::variant<
	FormModPair,   // formID~modName
	std::string>;  // editorID

template <class T>
struct Filters
{
	std::vector<T> ALL{};
	std::vector<T> NOT{};
	std::vector<T> MATCH{};
};

using StringVec = std::vector<std::string>;
struct StringFilters : Filters<std::string>
{
	StringVec ANY{};
};

using RawFormVec = std::vector<FormOrEditorID>;
using RawFormFilters = Filters<FormOrEditorID>;

using FormOrMod = std::variant<RE::TESForm*,  // form
	const RE::TESFile*>;                      // mod
using FormVec = std::vector<FormOrMod>;
using FormFilters = Filters<FormOrMod>;

using ActorLevel = std::pair<std::uint16_t, std::uint16_t>;  // min/maxLevel
using SkillLevel = std::pair<
	std::uint32_t,                           // skill type
	std::pair<std::uint8_t, std::uint8_t>>;  // skill Level
using LevelFilters = std::tuple<ActorLevel,
	std::vector<SkillLevel>,   // skill levels
	std::vector<SkillLevel>>;  // skill weights (from Class)

struct Traits
{
	std::optional<RE::SEX> sex{};
	std::optional<bool> unique{};
	std::optional<bool> summonable{};
	std::optional<bool> child{};
};

using IdxOrCount = std::int32_t;
using Chance = std::uint32_t;
