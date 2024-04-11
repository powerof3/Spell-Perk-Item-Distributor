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
			RECORD::TYPE            type{ RECORD::TYPE::kForm };
			FormOrEditorID          rawForm{};
			StringFilters           stringFilters{};
			Filters<FormOrEditorID> rawFormFilters{};
			LevelFilters            levelFilters{};
			Traits                  traits{};
			IndexOrCount            idxOrCount{ RandomCount(1, 1) };
			PercentChance           chance{ 100 };
			std::string             path{};
		};

		using DataVec = std::vector<Data>;

		inline Map<RECORD::TYPE, DataVec> configs{};

		std::pair<bool, bool> GetConfigs();

		namespace Exception
		{
			struct UnsupportedFormTypeException : std::exception
			{
				const std::string key;

				UnsupportedFormTypeException(const std::string& key) :
					key(key)
				{}

				const char* what() const noexcept override
				{
					return fmt::format("Unsupported Form type {}"sv, key).c_str();
				}
			};

			struct InvalidIndexOrCountException : std::exception
			{
				const std::string entry;

				InvalidIndexOrCountException(const std::string& entry) :
					entry(entry)
				{}

				const char* what() const noexcept override
				{
					return fmt::format("Invalid index or count {}"sv, entry).c_str();
				}
			};
		}

		struct DefaultKeyComponentParser
		{
			template <typename Data>
			void operator()(const std::string& key, Data& data) const;
		};

		struct DistributableFormComponentParser
		{
			template <typename Data>
			void operator()(const std::string& key, Data& data) const;
		};

		struct StringFiltersComponentParser
		{
			template <typename Data>
			void operator()(const std::string& entry, Data& data) const;
		};

		struct FormFiltersComponentParser
		{
			template <typename Data>
			void operator()(const std::string& entry, Data& data) const;
		};

		struct LevelFiltersComponentParser
		{
			template <typename Data>
			void operator()(const std::string& entry, Data& data) const;
		};

		struct TraitsFilterComponentParser
		{
			template <typename Data>
			void operator()(const std::string& entry, Data& data) const;
		};

		struct IndexOrCountComponentParser
		{
			template <typename Data>
			void operator()(const std::string& entry, Data& data) const;
		};

		struct ChanceComponentParser
		{
			template <typename Data>
			void operator()(const std::string& str, Data& data) const;
		};
	}
}

namespace Distribution::INI
{
	template <typename Data>
	void DefaultKeyComponentParser::operator()(const std::string& key, Data& data) const
	{
		auto type = RECORD::GetType(key);
		if (type == RECORD::kTotal) {
			throw Exception::UnsupportedFormTypeException(key);
		}

		data.type = type;
	}

	template <typename Data>
	void DistributableFormComponentParser::operator()(const std::string& key, Data& data) const
	{
		data.rawForm = distribution::get_record(key);
	}

	template <typename Data>
	void StringFiltersComponentParser::operator()(const std::string& entry, Data& data) const
	{
		auto split_str = distribution::split_entry(entry);
		for (auto& str : split_str) {
			if (str.contains("+"sv)) {
				auto strings = distribution::split_entry(str, "+");
				data.stringFilters.ALL.insert(data.stringFilters.ALL.end(), strings.begin(), strings.end());

			} else if (str.at(0) == '-') {
				str.erase(0, 1);
				data.stringFilters.NOT.emplace_back(str);

			} else if (str.at(0) == '*') {
				str.erase(0, 1);
				data.stringFilters.ANY.emplace_back(str);

			} else {
				data.stringFilters.MATCH.emplace_back(str);
			}
		}
	}

	template <typename Data>
	void FormFiltersComponentParser::operator()(const std::string& entry, Data& data) const
	{
		auto split_IDs = distribution::split_entry(entry);
		for (auto& IDs : split_IDs) {
			if (IDs.contains("+"sv)) {
				auto splitIDs_ALL = distribution::split_entry(IDs, "+");
				for (auto& IDs_ALL : splitIDs_ALL) {
					data.rawFormFilters.ALL.push_back(distribution::get_record(IDs_ALL));
				}
			} else if (IDs.at(0) == '-') {
				IDs.erase(0, 1);
				data.rawFormFilters.NOT.push_back(distribution::get_record(IDs));

			} else {
				data.rawFormFilters.MATCH.push_back(distribution::get_record(IDs));
			}
		}
	}

	template <typename Data>
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

	template <typename Data>
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

	template <typename Data>
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
			throw Exception::InvalidIndexOrCountException(entry);
		}
	}

	template <typename Data>
	void ChanceComponentParser::operator()(const std::string& str, Data& data) const
	{
		if (distribution::is_valid_entry(str)) {
			data.chance = string::to_num<PercentChance>(str);
		}
	}
}
