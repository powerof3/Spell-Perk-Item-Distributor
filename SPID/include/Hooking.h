#pragma once

#include "SKSE/SKSE.h"

#define ByteAt(addr) *reinterpret_cast<std::uint8_t*>(addr)

/// Declraing a pre_hook function allows Hook to receive a call before the main hook will be installed.
template <typename Hook>
concept pre_hook = requires {
	{ Hook::pre_hook() };
};

/// Declraing a post_hook function allows Hook to receive a call immediately after the main hook will be installed.
template <typename Hook>
concept post_hook = requires {
	{ Hook::post_hook() };
};

/// Fundamental concept for a hook.
/// A hook must have a static thunk function that will be written to a trampoline.
template <typename Hook>
concept hook = requires {
	{ Hook::thunk };
};

/// Optionally Hook can define a static member named func that will contain the original function to chain the call.
/// static inline REL::Relocation<decltype(thunk)> func;
template <typename Hook>
concept chain_hook = requires {
	{ Hook::func };
};

/// Basic Hook that writes a call (write_call<5>) instruction to a thunk.
/// This also supports writing to lea instructions, which store function addresses.
template <typename Hook>
concept call_hook = hook<Hook> && requires {
	{ Hook::relocation } -> std::convertible_to<REL::ID>;
	{ Hook::offset } -> std::convertible_to<std::size_t>;
};

/// A type that has a vtable to hook into.
/// vtable_hook can only be used with Targets that have a vtable.
template <typename Target>
concept has_vtable = requires {
	{ Target::VTABLE };
};

/// Defines required fields for a valid vtable hook.
/// Note that providing a custom vtable index is optional, if ommited `0`th table will be used by default.
template <typename Hook>
concept vtable_hook = hook<Hook> && requires {
	{ Hook::index } -> std::convertible_to<std::size_t>;
	requires(has_vtable<typename Hook::Target>);
};

/// Allows to provide a custom vtable index for a vtable hook.
/// Note that providing a custom vtable index is optional, if ommited `0`th table will be used by default.
template <typename Hook>
concept custom_vtable_index = requires {
	{ Hook::vtable } -> std::convertible_to<std::size_t>;
};

// Optional properties of a hook.
namespace details
{
	template <typename Hook>
	constexpr std::size_t get_vtable()
	{
		if constexpr (custom_vtable_index<Hook>) {
			return Hook::vtable;  // Use the vtable if it exists
		} else {
			return 0;  // Default to 0 if vtable doesn't exist
		}
	}

	template <typename Hook>
	constexpr void set_func(std::uintptr_t func)
	{
		if constexpr (chain_hook<Hook>) {
			Hook::func = func;
		}
	}
}

namespace stl
{
	using namespace SKSE::stl;

	template <hook Hook>
	void write_thunk_call(std::uintptr_t a_src)
	{
		auto& trampoline = SKSE::GetTrampoline();
		SKSE::AllocTrampoline(14);

		details::set_func<Hook>(trampoline.write_call<5>(a_src, Hook::thunk));
	}

	template <has_vtable F, typename Hook>
	void write_vfunc()
	{
		REL::Relocation<std::uintptr_t> vtbl{ F::VTABLE[details::get_vtable<Hook>()] };
		details::set_func<Hook>(vtbl.write_vfunc(Hook::index, Hook::thunk));
	}

	template <vtable_hook Hook>
	void write_vfunc()
	{
		write_vfunc<typename Hook::Target, Hook>();
	}

	template <call_hook Hook>
	void write_thunk()
	{
		const REL::Relocation<std::uintptr_t> rel{ Hook::relocation, Hook::offset };
		std::uintptr_t sourceAddress = rel.address();

		auto byteAddress = sourceAddress;
		auto opcode = ByteAt(byteAddress);

		if (opcode == 0xE8) {  // CALL instruction
			write_thunk_call<Hook>(sourceAddress);
		} else {
			auto leaSize = 7;
			constexpr std::uint8_t rexw = 0x48;
			if ((opcode & rexw) != rexw) { // REX.W Must be present for a valid 64-bit address replacement.
				stl::report_and_fail("Invalid hook location, lea instruction must use 64-bit register (first byte should be between 0x48 and 04F)"sv);
			}
			opcode = ByteAt(++byteAddress);

			if (opcode == 0x8D) {  // LEA instruction
				auto op1 = ByteAt(++byteAddress);  // Get first operand byte.
				auto opAddress = byteAddress;
				// Store original displacement
				std::int32_t disp = 0;
				for (std::uint8_t i = 0; i < 4; ++i) {
					disp |= ByteAt(++byteAddress) << (i * 8);
				}

				assert(disp != 0);
				// write CALL on top of LEA
				// This will fill new displacement
				// 8D MM XX XX XX XX -> 8D E8 YY YY YY YY (where MM is the operand #1, XX is the old func, and YY is the new func)
				write_thunk_call<Hook>(opAddress);  

				// Restore operand byte
				// Since we overwrote first operand of lea we need to write it back
				// 8D E8 YY YY YY YY -> 8D MM YY YY YY YY
				REL::safe_write(opAddress, op1);       

				// Find original function and store it in the hook's func.
				details::set_func<Hook>(sourceAddress + leaSize + disp);
			} else {
				stl::report_and_fail("Invalid hook location, write_thunk can only be used for call or lea instructions"sv);
			}
		}
	}

	/// Installs given hook
	template <hook Hook>
	void install_hook()
	{
		using ThunkType = decltype(Hook::thunk);
		if constexpr (chain_hook<Hook>) {
			using FuncType = decltype(Hook::func);
			static_assert(std::is_same_v<REL::Relocation<ThunkType>, FuncType>, "Mismatching type of thunk and func. 'Use static inline REL::Relocation<decltype(thunk)> func;' to always match the type.");
		}

		if constexpr (pre_hook<Hook>) {
			Hook::pre_hook();
		}

		if constexpr (call_hook<Hook>) {
			stl::write_thunk<Hook>();
		} else if constexpr (vtable_hook<Hook>) {
			stl::write_vfunc<Hook>();
		} else {
			static_assert(false, "Unsupported hook type. Hook must target either call, lea or vtable");
		}

		if constexpr (post_hook<Hook>) {
			Hook::post_hook();
		}
	}
}

#undef ByteAt
