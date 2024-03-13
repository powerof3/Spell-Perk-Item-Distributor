#pragma once

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

template <typename T>
struct Range
{
	Range() = default;

	explicit Range(T a_min) :
		min(a_min)
	{}
	Range(T a_min, T a_max) :
		min(a_min),
		max(a_max)
	{}

	[[nodiscard]] bool IsValid() const
	{
		return min > std::numeric_limits<T>::min();  // min must always be valid, max is optional
	}
	[[nodiscard]] bool IsInRange(T value) const
	{
		return value >= min && value <= max;
	}

	[[nodiscard]] bool IsExact() const 
	{
		return min == max;
	}

	[[nodiscard]] T GetRandom() const
	{
		return IsExact() ? min : RNG().generate<T>(min, max);
	}

	// members
	T min{ std::numeric_limits<T>::min() };
	T max{ std::numeric_limits<T>::max() };
};

// skill type, skill Level
struct SkillLevel
{
	std::uint32_t       type;
	Range<std::uint8_t> range;
};

struct LevelFilters
{
	Range<std::uint16_t>    actorLevel;
	std::vector<SkillLevel> skillLevels;   // skill levels
	std::vector<SkillLevel> skillWeights;  // skill weights (from Class)
};

struct Traits
{
	std::optional<RE::SEX> sex{};
	std::optional<bool>    unique{};
	std::optional<bool>    summonable{};
	std::optional<bool>    child{};
	std::optional<bool>    leveled{};
	std::optional<bool>    teammate{};
};

using Index = std::int32_t;
using Count = std::int32_t;
using RandomCount = Range<Count>;
using IndexOrCount = std::variant<Index, RandomCount>;
using Chance = std::uint32_t;

/// A standardized way of converting any object to string.
///
///	<p>
///	Overload `operator<<` to provide custom formatting for your value.
///	Alternatively, specialize this method and provide your own implementation.
///	</p>
template <typename Value>
std::string describe(Value value)
{
	std::ostringstream os;
	os << value;
	return os.str();
}

inline std::ostream& operator<<(std::ostream& os, RE::TESFile* file)
{
	os << file->fileName;
	return os;
}

inline std::ostream& operator<<(std::ostream& os, RE::TESForm* form)
{
	if (const auto& edid = editorID::get_editorID(form); !edid.empty()) {
		os << edid << " ";
	}
	os << "["
	   << std::to_string(form->GetFormType())
	   << ":"
	   << std::setfill('0')
	   << std::setw(sizeof(RE::FormID) * 2)
	   << std::uppercase
	   << std::hex
	   << form->GetFormID()
	   << "]";

	return os;
}
