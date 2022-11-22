#pragma once

#define MAKE_BUFFERED_LOG(a_func, a_type)                                                             \
                                                                                                      \
	template <class... Args>                                                                          \
	struct [[maybe_unused]] a_func                                                                    \
	{                                                                                                 \
		a_func() = delete;                                                                            \
                                                                                                      \
		explicit a_func(                                                                              \
			fmt::format_string<Args...> a_fmt,                                                        \
			Args&&... a_args,                                                                         \
			std::source_location a_loc = std::source_location::current())                             \
		{                                                                                             \
			if (buffer.insert({ a_loc, fmt::format(a_fmt, std::forward<Args>(a_args)...) }).second) { \
				spdlog::log(                                                                          \
					spdlog::source_loc{                                                               \
						a_loc.file_name(),                                                            \
						static_cast<int>(a_loc.line()),                                               \
						a_loc.function_name() },                                                      \
					spdlog::level::a_type,                                                            \
					a_fmt,                                                                            \
					std::forward<Args>(a_args)...);                                                   \
			}                                                                                         \
		}                                                                                             \
	};                                                                                                \
	template <class... Args>                                                                          \
	a_func(fmt::format_string<Args...>, Args&&...) -> a_func<Args...>;

namespace LogBuffer
{
	struct Entry
	{
		std::source_location loc;
		std::string message;

		bool operator==(const Entry& other) const
		{
			return strcmp(loc.file_name(), other.loc.file_name()) == 0 && loc.line() == other.loc.line() && message == other.message;
		}
	};
}

/// Add hashing for custom log entries.
template <>
struct std::hash<LogBuffer::Entry>
{
	std::size_t operator()(const LogBuffer::Entry& entry) const noexcept
	{
		return hash<std::string>()(entry.message);
	}
};

/// LogBuffer proxies typical logging calls and buffers received entries to avoid duplication.
///
/// Main log proxy functions.
/// Each log function checks whether given message was already logged and skips the log.
namespace LogBuffer
{
	inline robin_hood::unordered_flat_set<Entry> buffer{};

	/// Clears already buffered messages to allow them to be logged once again.
	inline void clear()
	{
		buffer.clear();
	}

	MAKE_BUFFERED_LOG(trace, trace);
	MAKE_BUFFERED_LOG(debug, debug);
	MAKE_BUFFERED_LOG(info, info);
	MAKE_BUFFERED_LOG(warn, warn);
	MAKE_BUFFERED_LOG(error, err);
	MAKE_BUFFERED_LOG(critical, critical);
}

#undef MAKE_BUFFERED_LOG
