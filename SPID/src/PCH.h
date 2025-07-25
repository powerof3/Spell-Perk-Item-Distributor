#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <ranges>
#include <shared_mutex>

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include <MergeMapperPluginAPI.h>
#include <ankerl/unordered_dense.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <srell.hpp>
#include <xbyak/xbyak.h>

#include <ClibUtil/distribution.hpp>
#include <ClibUtil/editorID.hpp>
#include <ClibUtil/rng.hpp>
#include <ClibUtil/simpleINI.hpp>
#include <ClibUtil/singleton.hpp>
#include <ClibUtil/string.hpp>
#include <ClibUtil/timer.hpp>

#include "LogBuffer.h"

#define DLLEXPORT __declspec(dllexport)

namespace logger = SKSE::log;
namespace buffered_logger = LogBuffer;

using namespace clib_util;

using namespace std::literals;
using namespace string::literals;
using namespace singleton;

// for visting variants
template <class... Ts>
struct overload : Ts...
{
	using Ts::operator()...;
};

template <class K, class D>
using Map = ankerl::unordered_dense::map<K, D>;
template <class K>
using Set = ankerl::unordered_dense::set<K>;

using Lock = RE::BSReadWriteLock;
using ReadLocker = RE::BSReadLockGuard;
using WriteLocker = RE::BSWriteLockGuard;

struct string_hash
{
	using is_transparent = void;  // enable heterogeneous overloads
	using is_avalanching = void;  // mark class as high quality avalanching hash

	[[nodiscard]] std::uint64_t operator()(std::string_view str) const noexcept
	{
		return ankerl::unordered_dense::hash<std::string_view>{}(str);
	}
};

template <class D>
using StringMap = ankerl::unordered_dense::segmented_map<std::string, D, string_hash, std::equal_to<>>;
using StringSet = ankerl::unordered_dense::segmented_set<std::string, string_hash, std::equal_to<>>;

namespace stl
{
	using namespace SKSE::stl;
};

#ifdef SKYRIM_AE
#	define OFFSET(se, ae) ae
#else
#	define OFFSET(se, ae) se
#endif

// These macros are used to provide a standardized way of managing hooks.
// A hook should be contained in a struct,
// which should call Process#HookName#() function of the responsible Manager class where this macro is used.

/// Creates an alias for a hook named `name` without creating corresponding function declaration.
/// given hook will be made friend to the enclosing struct/class.
#define HOOK_HANDLER_ALIAS(name, functionName) \
	friend struct name;

/// Creates a function declaration for a hook named `name` with the given return type and parameters.
/// given hook will be made friend to the enclosing struct/class.
#define HOOK_HANDLER(returnType, name, ...) \
	friend struct name;                     \
	returnType Process##name(__VA_ARGS__, std::function<returnType()> funcCall);

/// Creates a function declaration named `functionName` for a hook named `hookName` with the given return type and parameters.
/// given hook will be made friend to the enclosing struct/class.
#define HOOK_HANDLER_EX(returnType, hookName, functionName, ...) \
	friend struct hookName;                                      \
	returnType Process##functionName(__VA_ARGS__, std::function<returnType()> funcCall);

#include "Cache.h"
#include "Defs.h"
#include "LogHeader.h"
#include "Version.h"
