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

using Lock = std::shared_mutex;
using ReadLocker = std::shared_lock<Lock>;
using WriteLocker = std::unique_lock<Lock>;

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

	template <class T>
	void write_thunk_call(std::uintptr_t a_src)
	{
		auto& trampoline = SKSE::GetTrampoline();
		SKSE::AllocTrampoline(14);

		T::func = trampoline.write_call<5>(a_src, T::thunk);
	}

	template <class F, class T>
	void write_vfunc()
	{
		REL::Relocation<std::uintptr_t> vtbl{ F::VTABLE[T::index] };
		T::func = vtbl.write_vfunc(T::size, T::thunk);
	}
}

#ifdef SKYRIM_AE
#	define OFFSET(se, ae) ae
#else
#	define OFFSET(se, ae) se
#endif

#include "Cache.h"
#include "Defs.h"
#include "Version.h"
