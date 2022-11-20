#pragma once

#define MAKE_BUFFERED_LOG(a_func)																		\
																												\
	template <class... Args>																					\
	void a_func(																								\
		fmt::format_string<Args...> a_fmt,																		\
		Args&&... a_args,																						\
		std::source_location a_loc = std::source_location::current())											\
	{																											\
		if (buffer.insert(Entry(a_loc, fmt::format(a_fmt, std::forward<Args>(a_args)...))).second)							\
		{																										\
			logger::a_func(a_fmt, std::forward<Args>(a_args)..., a_loc);												\
		}																										\
	}																											\
																												                                                                           

/// LogBuffer proxies typical logging calls extended with related FormID or EditorID to differentiate
namespace LogBuffer
{
	struct Entry
	{
		std::source_location loc;
		std::string message;

		bool operator==(const LogBuffer::Entry& other) const
		{
			return (strcmp(loc.file_name(), other.loc.file_name()) == 0) && loc.line() == other.loc.line() && message == other.message;
		}
	};
};

/// Add hashing for custom log entries.
namespace std
{
	template <>
	struct hash<LogBuffer::Entry>
	{
		std::size_t operator()(const LogBuffer::Entry& entry) const
		{
			return hash<std::string>()(entry.message);
		}
	};
}

/// Main log proxy functions.
/// Each log function checks whether given message was already logged for specified Form and skips the log.
namespace LogBuffer
{

	inline std::unordered_set<Entry> buffer{};

	inline void clear()
	{
		buffer.clear();
	};

	template <class... Args>
	void a_func(
		fmt::format_string<Args...> a_fmt,
		Args&&... a_args,
		std::source_location a_loc = std::source_location::current())
	{
		if (buffer.insert(Entry(a_loc, fmt::format(a_fmt, std::forward<Args>(a_args)...))).second) {
			logger::info(a_fmt, std::forward<Args>(a_args)..., a_loc);
		}
	}	

	MAKE_BUFFERED_LOG(trace);
	MAKE_BUFFERED_LOG(debug);
	MAKE_BUFFERED_LOG(info);
	MAKE_BUFFERED_LOG(warn);
	MAKE_BUFFERED_LOG(error);
	MAKE_BUFFERED_LOG(critical);
};
