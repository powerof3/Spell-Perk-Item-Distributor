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

namespace Distribution
{
	namespace INI
	{
		struct Data
		{
			RECORD::TYPE   type{ RECORD::TYPE::kForm };
			FormOrEditorID rawForm{};
			StringFilters  stringFilters{};
			RawFormFilters formFilters{};
			LevelFilters   levelFilters{};
			Traits         traits{};
			IndexOrCount   idxOrCount{ RandomCount(1, 1) };
			PercentChance  chance{ 100 };
			std::string    path{};
		};

		using DataVec = std::vector<Data>;

		inline Map<RECORD::TYPE, DataVec> configs{};

		std::pair<bool, bool> GetConfigs();
	}
}

namespace Distribution::INI
{
	namespace Exception
	{
		struct UnsupportedFormTypeException : std::exception
		{
			const std::string key;

			UnsupportedFormTypeException(const std::string& key) :
				std::exception(fmt::format("Unsupported form type {}"sv, key).c_str()),
				key(key)
			{}
		};

		struct InvalidIndexOrCountException : std::exception
		{
			const std::string entry;

			InvalidIndexOrCountException(const std::string& entry) :
				std::exception(fmt::format("Invalid index or count {}"sv, entry).c_str()),
				entry(entry)
			{}
		};

		struct InvalidChanceException : std::exception
		{
			const std::string entry;

			InvalidChanceException(const std::string& entry) :
				std::exception(fmt::format("Invalid chance {}"sv, entry).c_str()),
				entry(entry)
			{}
		};

		struct MissingDistributableFormException : std::exception
		{
			MissingDistributableFormException() :
				std::exception("Missing distributable form")
			{}
		};

		struct MissingComponentParserException : std::exception
		{
			MissingComponentParserException() :
				std::exception("Missing component parser")
			{}
		};
	}

	namespace concepts
	{
		template <typename Data>
		concept typed_data = requires(Data data) {
			{
				data.type
			} -> std::same_as<RECORD::TYPE&>;
			{
				data.type = std::declval<RECORD::TYPE>()
			};
		};

		template <typename Data>
		concept form_data = requires(Data data) {
			{
				data.rawForm
			} -> std::same_as<FormOrEditorID&>;
			{
				data.rawForm = std::declval<FormOrEditorID>()
			};
		};

		template <typename Data>
		concept string_filterable_data = requires(Data data) {
			{
				data.stringFilters
			} -> std::same_as<StringFilters&>;
			{
				data.stringFilters = std::declval<StringFilters>()
			};
		};

		template <typename Data>
		concept form_filterable_data = requires(Data data) {
			{
				data.formFilters
			} -> std::same_as<RawFormFilters&>;
			{
				data.formFilters = std::declval<RawFormFilters>()
			};
		};

		template <typename Data>
		concept level_filterable_data = requires(Data data) {
			{
				data.levelFilters
			} -> std::same_as<LevelFilters&>;
			{
				data.levelFilters = std::declval<LevelFilters>()
			};
		};

		template <typename Data>
		concept trait_filterable_data = requires(Data data) {
			{
				data.traits
			} -> std::same_as<Traits&>;
			{
				data.traits = std::declval<Traits>()
			};
		};

		template <typename Data>
		concept countable_data = requires(Data data) {
			{
				data.idxOrCount
			} -> std::same_as<IndexOrCount&>;
			{
				data.idxOrCount = std::declval<IndexOrCount>()
			};
		};

		template <typename Data>
		concept randomized_data = requires(Data data) {
			{
				data.chance
			} -> std::same_as<PercentChance&>;
			{
				data.chance = std::declval<PercentChance>()
			};
		};
	}

	using namespace concepts;

	enum ComponentParserFlags : std::uint8_t
	{
		kNone = 0,
		kRequired = 1,

		// Modifiers used by Filters that support them

		kAllowCombineModifier = 1 << 2,
		kAllowExclusionModifier = 1 << 3,
		kAllowPartialMatchModifier = 1 << 4,

		kAllowAllModifiers = kAllowCombineModifier | kAllowExclusionModifier | kAllowPartialMatchModifier,

		// Default modifiers suitable for FormFilters
		kAllowFormsModifiers = kAllowCombineModifier | kAllowExclusionModifier
	};

	constexpr ComponentParserFlags operator | (ComponentParserFlags lhs, ComponentParserFlags rhs)
	{
		return ComponentParserFlags(std::uint8_t(lhs) | std::uint8_t(rhs));
	}

	/// <summary>
	/// Simply parses type of the record from the key.
	/// Always returns true and doesn't prevent further parsing.
	/// </summary>
	struct DefaultKeyComponentParser
	{
		template <typed_data Data>
		bool operator()(const std::string& key, Data& data) const;
	};

	struct DistributableFormComponentParser
	{
		template <form_data Data>
		void operator()(const std::string& entry, Data& data) const;
	};

	template <ComponentParserFlags flags = kAllowAllModifiers>
	struct StringFiltersComponentParser
	{
		template <string_filterable_data Data>
		void operator()(const std::string& entry, Data& data) const;
	};

	template <ComponentParserFlags flags = kAllowFormsModifiers>
	struct FormFiltersComponentParser
	{
		template <form_filterable_data Data>
		void operator()(const std::string& entry, Data& data) const;
	};

	struct LevelFiltersComponentParser
	{
		template <level_filterable_data Data>
		void operator()(const std::string& entry, Data& data) const;
	};

	struct TraitsFilterComponentParser
	{
		template <trait_filterable_data Data>
		void operator()(const std::string& entry, Data& data) const;
	};

	struct IndexOrCountComponentParser
	{
		template <countable_data Data>
		void operator()(const std::string& entry, Data& data) const;
	};

	struct ChanceComponentParser
	{
		template <randomized_data Data>
		void operator()(const std::string& entry, Data& data) const;
	};
}

namespace Distribution::INI
{
	using namespace Exception;

	template <typed_data Data>
	bool DefaultKeyComponentParser::operator()(const std::string& key, Data& data) const
	{
		auto type = RECORD::GetType(key);
		if (type == RECORD::kTotal) {
			throw UnsupportedFormTypeException(key);
		}

		data.type = type;
		return true;
	}

	template <form_data Data>
	void DistributableFormComponentParser::operator()(const std::string& entry, Data& data) const
	{
		if (entry.empty()) {
			throw MissingDistributableFormException();
		}

		data.rawForm = distribution::get_record(entry);
	}

	template <ComponentParserFlags flags>
	template <string_filterable_data Data>
	void StringFiltersComponentParser<flags>::operator()(const std::string& entry, Data& data) const
	{
		auto split_str = distribution::split_entry(entry);
		for (auto& str : split_str) {
			if constexpr (flags & kAllowCombineModifier) {
				if (str.contains("+"sv)) {
					auto strings = distribution::split_entry(str, "+");
					data.stringFilters.ALL.insert(data.stringFilters.ALL.end(), strings.begin(), strings.end());
					break;
				}
			}
			if constexpr (flags & kAllowExclusionModifier) {
				if (str.at(0) == '-') {
					str.erase(0, 1);
					data.stringFilters.NOT.emplace_back(str);
					break;
				}
			}
			if constexpr (flags & kAllowPartialMatchModifier) {
				if (str.at(0) == '*') {
					str.erase(0, 1);
					data.stringFilters.ANY.emplace_back(str);
					break;
				}
			}

			data.stringFilters.MATCH.emplace_back(str);
		}
	}

	template <ComponentParserFlags flags>
	template <form_filterable_data Data>
	void FormFiltersComponentParser<flags>::operator()(const std::string& entry, Data& data) const
	{
		auto split_IDs = distribution::split_entry(entry);

		if (split_IDs.empty()) {
			if constexpr (flags & ComponentParserFlags::kRequired) {
				throw MissingComponentParserException();
			}
			return;
		}

		for (auto& IDs : split_IDs) {
			if constexpr (flags & kAllowCombineModifier) {
				if (IDs.contains("+"sv)) {
					auto splitIDs_ALL = distribution::split_entry(IDs, "+");
					for (auto& IDs_ALL : splitIDs_ALL) {
						data.formFilters.ALL.push_back(distribution::get_record(IDs_ALL));
					}
					break;
				}
			}

			if constexpr (flags & kAllowExclusionModifier) {
				if (IDs.at(0) == '-') {
					IDs.erase(0, 1);
					data.formFilters.NOT.push_back(distribution::get_record(IDs));
					break;
				}
			}

			data.formFilters.MATCH.push_back(distribution::get_record(IDs));
		}
	}

	template <level_filterable_data Data>
	void LevelFiltersComponentParser::operator()(const std::string& entry, Data& data) const
	{
		Range<std::uint16_t>    actorLevel;
		std::vector<SkillLevel> skillLevels;
		std::vector<SkillLevel> skillWeights;
		auto                    split_levels = distribution::split_entry(entry, ",");
		for (auto& levels : split_levels) {
			if (levels.contains('(')) {
				//skill(min/max)
				const auto isWeightFilter = levels.starts_with('w');
				auto       sanitizedLevel = string::remove_non_alphanumeric(levels);
				if (isWeightFilter) {
					sanitizedLevel.erase(0, 1);
				}
				//skill min max
				if (auto skills = string::split(sanitizedLevel, " "); !skills.empty()) {
					if (auto type = string::to_num<std::uint32_t>(skills[0]); type < 18) {
						auto minLevel = string::to_num<std::uint8_t>(skills[1]);
						if (skills.size() > 2) {
							auto maxLevel = string::to_num<std::uint8_t>(skills[2]);
							if (isWeightFilter) {
								skillWeights.push_back({ type, Range(minLevel, maxLevel) });
							} else {
								skillLevels.push_back({ type, Range(minLevel, maxLevel) });
							}
						} else {
							if (isWeightFilter) {
								// Single value is treated as exact match.
								skillWeights.push_back({ type, Range(minLevel) });
							} else {
								skillLevels.push_back({ type, Range(minLevel) });
							}
						}
					}
				}
			} else {
				if (auto actor_level = string::split(levels, "/"); actor_level.size() > 1) {
					auto minLevel = string::to_num<std::uint16_t>(actor_level[0]);
					auto maxLevel = string::to_num<std::uint16_t>(actor_level[1]);

					actorLevel = Range(minLevel, maxLevel);
				} else {
					auto level = string::to_num<std::uint16_t>(levels);

					actorLevel = Range(level);
				}
			}
		}
		data.levelFilters = { actorLevel, skillLevels, skillWeights };
	}

	template <trait_filterable_data Data>
	void TraitsFilterComponentParser::operator()(const std::string& entry, Data& data) const
	{
		auto split_traits = distribution::split_entry(entry, "/");
		for (auto& trait : split_traits) {
			switch (string::const_hash(trait)) {
			case "M"_h:
			case "-F"_h:
				data.traits.sex = RE::SEX::kMale;
				break;
			case "F"_h:
			case "-M"_h:
				data.traits.sex = RE::SEX::kFemale;
				break;
			case "U"_h:
				data.traits.unique = true;
				break;
			case "-U"_h:
				data.traits.unique = false;
				break;
			case "S"_h:
				data.traits.summonable = true;
				break;
			case "-S"_h:
				data.traits.summonable = false;
				break;
			case "C"_h:
				data.traits.child = true;
				break;
			case "-C"_h:
				data.traits.child = false;
				break;
			case "L"_h:
				data.traits.leveled = true;
				break;
			case "-L"_h:
				data.traits.leveled = false;
				break;
			case "T"_h:
				data.traits.teammate = true;
				break;
			case "-T"_h:
				data.traits.teammate = false;
				break;
			default:
				break;
			}
		}
	}

	template <countable_data Data>
	void IndexOrCountComponentParser::operator()(const std::string& entry, Data& data) const
	{
		auto typeHint = data.type;

		if (typeHint == RECORD::kPackage) {  // reuse item count for package stack index
			data.idxOrCount = 0;
		}

		if (!distribution::is_valid_entry(entry)) {
			return;
		}
		try {
			if (typeHint == RECORD::kPackage) {  // If it's a package, then we only expect a single number.
				data.idxOrCount = string::to_num<Index>(entry);
			} else {
				if (auto countPair = string::split(entry, "-"); countPair.size() > 1) {
					auto minCount = string::to_num<Count>(countPair[0]);
					auto maxCount = string::to_num<Count>(countPair[1]);

					data.idxOrCount = RandomCount(minCount, maxCount);
				} else {
					auto count = string::to_num<Count>(entry);

					data.idxOrCount = RandomCount(count, count);  // create the exact match range.
				}
			}
		} catch (const std::exception& e) {
			throw InvalidIndexOrCountException(entry);
		}
	}

	template <randomized_data Data>
	void ChanceComponentParser::operator()(const std::string& entry, Data& data) const
	{
		if (distribution::is_valid_entry(entry)) {
			try {
				data.chance = string::to_num<PercentChance>(entry);
			} catch (const std::exception&) {
				throw InvalidChanceException(entry);
			}
		}
	}
}
