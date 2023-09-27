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
#include <ClibUtil/string.hpp>

#include "LogBuffer.h"

#define DLLEXPORT __declspec(dllexport)

namespace logger = SKSE::log;
namespace buffered_logger = LogBuffer;

namespace string = clib_util::string;
namespace distribution = clib_util::distribution;
namespace hash = clib_util::hash;
namespace edid = clib_util::editorID;

using namespace std::literals;
using namespace string::literals;

using RNG = clib_util::RNG;

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
