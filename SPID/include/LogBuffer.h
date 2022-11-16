#pragma once

#define DECLARE_BUFFERED_LOG(a_func)			\
												\
		template <class... Args>				\
		void a_func(							\
			RE::FormID formId,					\
			fmt::format_string<Args...> a_fmt,	\
			Args&&... a_args);					\
												\
		template <class... Args>				\
		void a_func(							\
			std::string editorId,				\
			fmt::format_string<Args...> a_fmt,	\
			Args&&... a_args);
																			


/// LogBuffer proxies typical logging calls extended with related FormID or EditorID to differentiate
namespace LogBuffer
{		
	DECLARE_BUFFERED_LOG(trace);
	DECLARE_BUFFERED_LOG(debug);
	DECLARE_BUFFERED_LOG(info);
	DECLARE_BUFFERED_LOG(warn);
	DECLARE_BUFFERED_LOG(error);
	DECLARE_BUFFERED_LOG(critical);
	
	void clear();
};

