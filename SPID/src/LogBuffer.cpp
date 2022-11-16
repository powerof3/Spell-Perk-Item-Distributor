#include "LogBuffer.h"

#define MAKE_BUFFERED_LOG(a_func, a_type)								\
																		\
		template <class... Args>										\
		void a_func(													\
			RE::FormID formId,											\
			fmt::format_string<Args...> a_fmt,							\
			Args&&... a_args)											\
		{																\
			if (buffer.insert(Entry(a_type, formId, a_fmt)).second) {	\
				logger::a_func(a_fmt, std::forward<Args>(a_args)...);	\
			}															\
		}																\
																		\
		template <class... Args>										\
		void a_func(													\
			std::string editorId,										\
			fmt::format_string<Args...> a_fmt,							\
			Args&&... a_args)											\
		{																\
			if (buffer.insert(Entry(a_type, editorId, a_fmt)).second) {	\
				logger::a_func(a_fmt, std::forward<Args>(a_args)...);	\
			}															\
		}                                                                                               

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

	struct AnyEntry
	{
		Type type{ kInfo };
		RE::FormID formId{ 0 };
		std::string editorId{ "" };
	};

	template <class... Args>
	struct Entry : AnyEntry
	{
		fmt::format_string<Args...> message;

		Entry(Type t, RE::FormID fid, fmt::format_string<Args...> message) :
			type(t), formId(fid), message(message) {}

		Entry(Type t, std::string eid, fmt::format_string<Args...> message) :
			type(t), editorId(eid), message(message) {}

		bool operator==(const LogBuffer::Entry<Args...>& other) const
		{
			return (formId == other.formId && editorId == other.editorId && message == other.message);
		}
	};

	std::set<AnyEntry> buffer{};

	MAKE_BUFFERED_LOG(trace, kTrace);
	MAKE_BUFFERED_LOG(debug, kDebug);
	MAKE_BUFFERED_LOG(info, kInfo);
	MAKE_BUFFERED_LOG(warn, kWarn);
	MAKE_BUFFERED_LOG(error, kError);
	MAKE_BUFFERED_LOG(critical, kCritical);

	// an example of the function
	template <class... Args>
	void a_func(
		RE::FormID fid,
		fmt::format_string<Args...> a_fmt,
		Args&&... a_args)
	{
		if (buffer.insert(Entry(kInfo, fid, a_fmt))) {
			logger::info(a_fmt, std::forward<Args>(a_args)...);
		}
	};

	void clear()
	{
		buffer.clear();
	};
};

namespace std
{
	template <>
	struct hash<LogBuffer::AnyEntry>
	{
		std::size_t operator()(const LogBuffer::AnyEntry& entry) const
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
