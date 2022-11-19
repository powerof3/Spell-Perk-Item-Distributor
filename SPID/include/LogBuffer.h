#pragma once

#define MAKE_BUFFERED_LOG(a_func, a_type)																		\
																												\
	template <class... Args>																					\
	void a_func(																								\
		RE::FormID formId,																						\
		fmt::format_string<Args...> a_fmt,																		\
		Args&&... a_args)																						\
	{																											\
		if (buffer.insert(Entry(a_type, formId, fmt::format(a_fmt, std::forward<Args>(a_args)...))).second)		\
		{																										\
			logger::a_func(a_fmt, std::forward<Args>(a_args)...);												\
		}																										\
	}																											\
																												\
	template <class... Args>																					\
	void a_func(																								\
		std::string editorId,																					\
		fmt::format_string<Args...> a_fmt,																		\
		Args&&... a_args)																						\
	{																											\
		if (buffer.insert(Entry(a_type, editorId, fmt::format(a_fmt, std::forward<Args>(a_args)...))).second)	\
		{																										\
			logger::a_func(a_fmt, std::forward<Args>(a_args)...);												\
		}																										\
	}                                                                                               



/// LogBuffer proxies typical logging calls extended with related FormID or EditorID to differentiate
namespace LogBuffer
{
	enum Type : std::int8_t
	{
		kTrace = 0,
		kDebug,
		kInfo,
		kWarn,
		kError,
		kCritical
	};

	/// An object representing a single log entry, associated with a Form described with either `formId` or `editorId`.
	struct Entry
	{
		Type type{ kInfo };
		RE::FormID formId{ 0 };
		std::string editorId{ "" };
		std::string message;

		constexpr Entry(Type t, RE::FormID fid, std::string message) :
			type(t), formId(fid), message(message) {}

		constexpr Entry(Type t, std::string eid, std::string message) :
			type(t), editorId(eid), message(message) {}

		bool operator==(const LogBuffer::Entry& other) const
		{
			return (formId == other.formId && editorId == other.editorId && message == other.message);
		}
	};

	inline std::unordered_set<Entry> buffer{};

	inline void clear()
	{
		buffer.clear();
	};
};

/// Main log proxy functions.
/// Each log function checks whether given message was already logged for specified Form and skips the log.
namespace LogBuffer
{
	MAKE_BUFFERED_LOG(trace, kTrace);
	MAKE_BUFFERED_LOG(debug, kDebug);
	MAKE_BUFFERED_LOG(info, kInfo);
	MAKE_BUFFERED_LOG(warn, kWarn);
	MAKE_BUFFERED_LOG(error, kError);
	MAKE_BUFFERED_LOG(critical, kCritical);
};

/// Add hashing for custom log entries.
namespace std
{
	template <>
	struct hash<LogBuffer::Entry>
	{
		std::size_t operator()(const LogBuffer::Entry& entry) const
		{
			using std::hash;
			using std::size_t;
			using std::string;

			std::size_t result = 0;

			if (entry.formId > 0) {
				result = hash<RE::FormID>()(entry.formId);
			} else if (!entry.editorId.empty()) {
				result = hash<std::string>()(entry.editorId);
			}
			// we disregard message in a hash, message will be compared when checked for equality.

			// Make room for unique type of the entry.
			return (result << 3) + entry.type;
		}
	};
}
