#pragma once
#include "Bitmasks.h"
#include "LookupFilters.h"

namespace Configs
{
	enum Type: std::uint8_t
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

		kTotal,
		kInvalid = kTotal
	};

	constexpr Type operator++(Type& type) {
		return type = static_cast<Type>(static_cast<std::uint8_t>(type) + 1);
	}

	/// An ordered array of strings representing all supported distributable types.
	/// This array must be ordered accordingly to Type enum, so that rawTypes[Type::kXXX] will always yield correct mapping.
	inline constexpr std::array rawTypes{ "Spell"sv, "Perk"sv, "Item"sv, "Shout"sv, "LevSpell"sv, "Package"sv, "Outfit"sv, "Keyword"sv, "DeathItem"sv, "Faction"sv, "SleepOutfit"sv, "Skin"sv };

	static constexpr std::string_view toRawType(const Type& t) { return rawTypes[t]; }

	static constexpr Type fromRawType(const std::string_view& raw) {
		for (Type t = kSpell; t < kTotal; ++t) {
			if (rawTypes[t] == raw) {
				return t;
			}
		}
		return kInvalid;
	}
}

namespace Configs
{
	enum class Sections : std::uint8_t
	{
		kNone = 0,

		kSPID = 0b1,
		kGeneral = 0b10,
		kPlayer = 0b100,
		kNPC = 0b1000,

		kAll = kSPID | kGeneral | kPlayer | kNPC
	};

	enum class Format : std::uint8_t
	{
		///	Loosely structured config similarly to <b>kFixedINI</b>, but allows more freedom with filters.
		///	There are no more restrictions as to where certain types of filters should be placed,
		///	and number of sections is unlimited. Each section is treated as AND separator which is equivalent to `+`.
		///
		///	<p>Example:</p>
		/// <p>
		///	Record = IdxOrCount FormOrEditorID|[AnyFilterTypes]
		///	</p>
		///
		/// <p><b>This is the default format introduced with the SPID 7.</b></p>
		kFreeformINI,

		/// Structured config with a pre-determined positions for each type of filters.
		///	Each filter type has a fixed place in the entry and empty sections must be present (optionally marked with NONE keyword).
		/// This format will be automatically converted to <b>kFreeformINI</b>, unless <b>Options::kNoConversion</b> is specified.
		///
		///	<p>Example:</p>
		/// <p>
		///	Record = FormOrEditorID|StringFilters|RawFormFilters|LevelFilters|Traits|IdxOrCount|Chance
		///	</p>
		///
		/// <p><b>This is the legacy format that was the only option prior to SPID 7.</b></p>
		kFixedINI,

		/// Structured config that provides better verbosity.
		///	Entries are defined by separate INI sections and each previously unnamed section of the entry appears as a named key-value pair.
		/// Empty filters might be omitted completely.
		///
		/// Multiple entries for the same Record must contain <b>UniqueSuffix</b> in section's name, to distinguish different sections.
		///
		///	`|` symbol is still allowed in each filter and is treated as AND separator which is equivalent to `+`.
		///
		///	<p>Example:</p>
		///
		///	<p>[Record|FormOrEditorID|UniqueSuffix]</p>
		///	<p>StringsFilters = ...|...|...</p>
		///	<p>FormsFilters = ...|...|...</p>
		///	<p>LevelsFilters = ...|...|...</p>
		///	<p>Traits = ...|...|...</p>
		///	<p>IdxOrCount = 1</p>
		///	<p>Chance = 100</p>
		///
		/// <p><b>This is an optional format introduced with SPID 7.</b></p>
		kSectionINI
	};

	// TODO: Consider using more detailed bitmask to indicate specific features that are introduce by the Schema
	// For example, kFixedINI will have schema = 3, as it supports specific treatment of Negated filters and writes traits in a slash-separated format.
	// However, kFreeformINI (and kSectionINI) will have schema = 0, because they don't treat negated filters unique nor they support separating traits with slashes.
	// Required upgrades will be determined using this schema flags.
	enum class Schema : std::uint32_t
	{
		kNone = 0,

		/// Schema flag that indicates whether current config file still uses exclusive negation filters.
		///	<p>For example: "A,B" would work as "A OR B", but "A,-B" would work as "A AND NOT B" instead of expected "A OR NOT B".</p>
		///
		///	<p><b>This is a legacy behavior that will be converted when read by newer SPID</b></p>
		kExclusiveNegation = 0b1,

		/// Schema flag that indicates whether current config file uses a special format of Trait filters that use `/` as a separator.
		///	<p>For example: "F/U/-S" is the same as "F,U,-S" which is interpreted as "F OR U OR NOT S" (though kExclusiveNegation must be handled) </p>
		///
		///	<p><b>This is a legacy behavior that will be converted when read by newer SPID</b></p>
		kSlashSeparatedTraits = 0b10,

		/// Schema flag that indicates whether current config file uses Format::kFreeformINI.
		///
		///	Note: Legacy configs are assumed to not use that and will be converted.
		kFreeformFilters = 0b100,

		kAll = kExclusiveNegation | kSlashSeparatedTraits | kFreeformFilters
	};

	enum class Options : std::uint32_t
	{
		kNone = 0,

		/// Specifies that config file that uses legacy format <b>Format::kFixedINI</b>
		/// should not be converted into the latest default format <b>Format::kFreeformINI</b>.
		kSkipSchemaUpgrade = 0b1,

		/// Specifies that records from that config file should be traced in logs.
		kTrace = 0b10,

		kAll = kSkipSchemaUpgrade | kTrace
	};
}

// Enabling bitmask behavior for Config's enums
template <>
struct enable_bitmask_operators<Configs::Schema>
{
	static constexpr bool enable = true;
};

template <>
struct enable_bitmask_operators<Configs::Options>
{
	static constexpr bool enable = true;
};

template <>
struct enable_bitmask_operators<Configs::Sections>
{
	static constexpr bool enable = true;
};


// Main data
namespace Configs
{
	/// Schema that will be used when encountering a configuration file without explicit schema.
	/// Such files are assumed to be legacy configuration files created before SPID 7.
	inline static constexpr Schema defaultSchema = Schema::kExclusiveNegation | Schema::kSlashSeparatedTraits;

	/// Schema that is being used by latest version of SPID.
	///	When configuration file's format evolving you should add schema flags that represent new features of the format
	///	and combine them here into latestSchema.
	///	This schema will be used to determine what
	inline static constexpr Schema latestSchema = Schema::kFreeformFilters;

    struct Data
	{
		using TypeHint = Type;

		IdxOrCount     idxOrCount{ 1 };
		FormOrEditorID rawForm{};
		TypeHint       typeHint{};

		filters::NPCExpression* filters{ nullptr };
	};

	struct Config
	{
		/// Schema version represents a set of rules by which given config is parsed.
		///	This version is used to provide backward compatibility support for future versions and will be specified in a special section.
		///	Configuration files that do not have that section or do not specify schema will be considered SPID 6, as such <b>default schema is 6</b>.
		///	When performing conversion SPID will automatically set current schema.
		///
		/// <p>
		///	These might include treating certain symbols in a specific way or requiring specific syntax.
		///	</p>
		Schema schema{ defaultSchema };

		/// Format of the configuration file that was loaded.
		///	This format is automatically detected when reading the file.
		///	Some of the formats might be converted into each other.
		Format format{ Format::kFreeformINI };

		/// A set of options that can customize how the configuration file will be treated.
		Options options{ Options::kNone };

		Sections sections{};

		/// File path of the configuration file.
		std::filesystem::path path{};

		/// Name of the configuration file.
		std::string name{};

		std::vector<Data> data{};

		std::vector<Data> operator[](Type type) const
		{
			std::vector<Data> subset;
			std::ranges::copy_if(data, std::back_inserter(subset),[type](const Data& data) {
					return data.typeHint == type;
			});
			return subset;
		}
	};
}
