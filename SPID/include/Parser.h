#pragma once

namespace detail
{
	template <typename Data, typename... ComponentParsers, size_t... Is>
	void parse_each(Data& data, const std::string splited[sizeof...(ComponentParsers)], std::index_sequence<Is...>)
	{
		(ComponentParsers()(splited[Is], data), ...);
	}
}

template <typename Data, typename KeyComponentParser, typename... ComponentParsers>
Data Parse(const std::string& key, const std::string& entry)
{
	constexpr const size_t numberOfComponents = sizeof...(ComponentParsers);

	const auto rawSections = string::split(entry, "|");
	const auto numberOfSections = rawSections.size();

	if (numberOfSections > numberOfComponents) {
		throw std::runtime_error("Not enough components");
	}

	std::string sections[numberOfComponents];
	for (size_t i = 0; i < numberOfSections; i++) {
		sections[i] = rawSections[i];
	}

	// Fill in default empty trailing sections.
	for (size_t i = numberOfSections; i < numberOfComponents; i++) {
		sections[i] = "";
	}

	Data data{};

	KeyComponentParser kcp;
	kcp(key, data);

	detail::parse_each<Data, ComponentParsers...>(data, sections, std::index_sequence_for<ComponentParsers...>());

	return data;
};

struct DefaultKeyComponentParser
{
	template <typename Data>
	void operator()(const std::string& key, Data& data) const
	{
		// TODO: Parse type.
	}
};


struct DistributableFormComponentParser
{
	template <typename Data>
	void operator()(const std::string& key, Data& data) const
	{
		data.rawForm = distribution::get_record(key);
	}
};

struct StringFiltersComponentParser
{
	template <typename Data>
	void operator()(const std::string& str, Data& data) const
	{
		auto split_str = distribution::split_entry(str);
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
};

struct FormFiltersComponentParser
{
	template <typename Data>
	void operator()(const std::string& str, Data& data) const
	{
		auto split_IDs = distribution::split_entry(str);
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
};

struct LevelFiltersComponentParser
{
	template <typename Data>
	void operator()(const std::string& str, Data& data) const
	{
		Range<std::uint16_t>    actorLevel;
		std::vector<SkillLevel> skillLevels;
		std::vector<SkillLevel> skillWeights;
		auto                    split_levels = distribution::split_entry(str, ",");
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
};


struct TraitsFilterComponentParser
{
	template <typename Data>
	void operator()(const std::string& str, Data& data) const
	{
		auto split_traits = distribution::split_entry(str, "/");
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
};

struct IndexOrCountComponentParser
{
	template <typename Data>
	void operator()(const std::string& str, Data& data) const
	{
		auto typeHint = RECORD::kPackage;

		// TODO: Get type hint from data.
		if (typeHint == RECORD::kPackage) {  // reuse item count for package stack index
			data.idxOrCount = 0;
		}

		if (!distribution::is_valid_entry(str)) {
			return;
		}

		if (typeHint == RECORD::kPackage) {  // If it's a package, then we only expect a single number.
			data.idxOrCount = string::to_num<Index>(str);
		} else {
			if (auto countPair = string::split(str, "-"); countPair.size() > 1) {
				auto minCount = string::to_num<Count>(countPair[0]);
				auto maxCount = string::to_num<Count>(countPair[1]);

				data.idxOrCount = RandomCount(minCount, maxCount);
			} else {
				auto count = string::to_num<Count>(str);

				data.idxOrCount = RandomCount(count, count);  // create the exact match range.
			}
		}
	}
};

struct ChanceComponentParser
{
	template <typename Data>
	void operator()(const std::string& str, Data& data) const
	{
		if (distribution::is_valid_entry(str)) {
			data.chance = string::to_num<PercentChance>(str);
		}
	}
};
