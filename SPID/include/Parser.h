#pragma once

namespace detail
{
	/// <summary>
	/// An utility function that will iterate over the list of ComponentParsers and call each one with the corresponding section of the entry.
	/// </summary>
	template <typename Data, typename... ComponentParsers, size_t... Is>
	void parse_each(Data& data, const std::string splited[sizeof...(ComponentParsers)], std::index_sequence<Is...>)
	{
		(ComponentParsers()(splited[Is], data), ...);
	}
}

template <typename ComponentParser, typename Data>
concept component_parser = requires(ComponentParser const, const std::string& section, Data& data) {
	{ ComponentParser()(section, data) } -> std::same_as<void>;
};

template <typename KeyComponentParser, typename Data>
concept key_component_parser = requires(KeyComponentParser const, const std::string& key, Data& data) {
	{ KeyComponentParser()(key, data) } -> std::same_as<bool>;
};

struct NotEnoughComponentsException: std::exception
{
	const size_t componentParsersCount;
	const size_t entrySectionsCount;

	NotEnoughComponentsException(size_t componentParsersCount, size_t entrySectionsCount) :
		componentParsersCount(componentParsersCount),
		entrySectionsCount(entrySectionsCount)
	{}

	const char* what() const noexcept override
	{
		return fmt::format("Too many sections. Expected at most {}, but got {}"sv, componentParsersCount, entrySectionsCount).c_str();
	}
};

/// <summary>
/// A composable Parsing function that accepts a variadic list of functors that define structure of the entry being parsed.
/// 
/// Number of component parsers must be at least the same as the number of sections in the entry.
/// If there are fewer sections than component parsers, the remaining parsers will be called with an empty string.
/// 
/// Parsing may throw and exception if there was not enough ComponentParsers to match all available entries.
/// It will also rethrow any exceptions thrown by the ComponentParsers.
/// </summary>
/// <typeparam name="Data">A type of the data object that will accumulate parsing results.</typeparam>
/// <typeparam name="KeyComponentParser">A special ComponentParser that is used for parsing entry's Key. It can also determine whether or not to proceed with parsing.</typeparam>
/// <typeparam name="...ComponentParsers">A list of ComponentParsers ordered in the same way thay you expect sections to appear in the line.</typeparam>
/// <param name="key">Key associated with the entry.</param>
/// <param name="entry">The entry line as it was read from the file.</param>
/// <returns></returns>
template <typename Data, key_component_parser<Data> KeyComponentParser, component_parser<Data>... ComponentParsers>
Data Parse(const std::string& key, const std::string& entry)
{
	constexpr const size_t numberOfComponents = sizeof...(ComponentParsers);

	const auto rawSections = string::split(entry, "|");
	const size_t numberOfSections = rawSections.size();

	if (numberOfSections > numberOfComponents) {
		throw NotEnoughComponentsException(numberOfComponents, numberOfSections);
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

	if (!KeyComponentParser()(key, data)) {
		return data;
	}

	detail::parse_each<Data, ComponentParsers...>(data, sections, std::index_sequence_for<ComponentParsers...>());

	return data;
};
