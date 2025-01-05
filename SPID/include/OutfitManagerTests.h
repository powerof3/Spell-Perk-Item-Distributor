#pragma once
#include "OutfitManager.h"
#include "Testing.h"

#define SETUP(A, W, isWd, isWf, G, L)                                                  \
	auto           manager = Outfits::Manager();                                       \
	RE::Actor*     actor = TestsHelper::Get##A##();                                    \
	RE::BGSOutfit* original = actor->GetActorBase()->defaultOutfit;                    \
	RE::BGSOutfit* worn = TestsHelper::Get##W##Outfit();                               \
	RE::BGSOutfit* wears = worn;                                                       \
	RE::BGSOutfit* gets = TestsHelper::Get##G##Outfit();                               \
	RE::BGSOutfit* links = TestsHelper::Get##L##Outfit();                              \
	auto&          wornReplacements = TestsHelper::GetWornReplacements(manager);       \
	auto&          pendingReplacements = TestsHelper::GetPendingReplacements(manager); \
	if (worn) {                                                                        \
		TestsHelper::ApplyOutfit(actor, worn);                                         \
		wornReplacements[actor->GetFormID()] = { original, worn, isWd, isWf, false }; \
	}
// Terms from the Table https://docs.google.com/spreadsheets/d/1JEhAql7hUURYC63k_fScZ9u8OWni6gN9jHzytNQt-m8/edit?usp=sharing
#define Death true
#define Regular false
#define Final true
#define NotFinal false

#define CHECK(replacement, d, f)                                                                                          \
	ASSERT(replacement, "Expected to have a replacement");                                                                \
	ASSERT(replacement->isDeathOutfit == d, fmt::format("Expected replacement to be {} outfit", d ? "Death" : "Regular")) \
	ASSERT(replacement->isFinalOutfit == f, fmt::format("Expected replacement to be {} outfit", f ? "Final" : "NOT Final"))

#define EXPECT_ORIGINAL(outfit)                                                                                           \
	{                                                                                                                     \
		auto it = wornReplacements.find(actor->formID);                                                                   \
		ASSERT(it == wornReplacements.end(), "Expected to have no Worn Replacement for actor");                           \
		auto actual = actor->GetActorBase()->defaultOutfit;                                                               \
		ASSERT(actual == outfit, fmt::format("Expected actor to wear original outfit {}, but got {}", *outfit, *actual)); \
		PASS;                                                                                                             \
	}
#define EXPECT_WORN(replacement, outfit)                                                                                                   \
	{                                                                                                                                      \
		ASSERT(replacement, "Expected to have a Worn Replacement for actor");                                                              \
		auto distributed = replacement->distributed;                                                                                       \
		auto actual = actor->GetActorBase()->defaultOutfit;                                                                                \
		ASSERT(distributed == outfit, fmt::format("Expected Worn Replacement to have distributed {}, but got {}", *outfit, *distributed)); \
		ASSERT(actual == outfit, fmt::format("Expected actor to wear worn outfit {}, but got {}", *outfit, *actual));                      \
		PASS;                                                                                                                              \
	}

#define EXPECT_WORN_FIND(outfit)                                                                                                           \
	{                                                                                                                                      \
		auto it = wornReplacements.find(actor->formID);                                                                                    \
		ASSERT(it != wornReplacements.end(), "Expected to have a Worn Replacement for actor");                                             \
		auto distributed = it->second.distributed;                                                                                         \
		auto actual = actor->GetActorBase()->defaultOutfit;                                                                                \
		ASSERT(distributed == outfit, fmt::format("Expected Worn Replacement to have distributed {}, but got {}", *outfit, *distributed)); \
		ASSERT(actual == outfit, fmt::format("Expected actor to wear worn outfit {}, but got {}", *outfit, *actual));                      \
		PASS;                                                                                                                              \
	}

namespace Outfits
{
	using namespace Testing;

	struct TestsHelper
	{
		// Exposes private members through firend TestsHelper;
		static auto& GetWornReplacements(Manager& manager) { return manager.wornReplacements; }

		static auto& GetPendingReplacements(Manager& manager) { return manager.pendingReplacements; }

		static auto* ResolveAndApplyWornOutfit(Manager& manager, RE::Actor* actor, bool isDying)
		{
			auto* replacement = manager.ResolveWornOutfit(actor, isDying);
			if (replacement) {
				ApplyOutfit(actor, replacement->distributed);
			}
			return replacement;
		}

		static void ApplyOutfit(RE::Actor* actor, RE::BGSOutfit* outfit)
		{
			Manager::GetSingleton()->ApplyOutfit(actor, outfit);
		}

		static RE::BGSOutfit* GetElvenOutfit() { return GetForm<RE::BGSOutfit>(0x57A22); }

		static RE::BGSOutfit* GetGuardOutfit() { return GetForm<RE::BGSOutfit>(0xFF282); }

		static RE::BGSOutfit* GetHunterOutfit() { return GetForm<RE::BGSOutfit>(0x106988); }

		static RE::BGSOutfit* GetFarmOutfit() { return GetForm<RE::BGSOutfit>(0x1DC10); }

		static RE::BGSOutfit* GetNoneOutfit() { return nullptr; }

		static RE::Actor* GetAlive() { return RE::PlayerCharacter::GetSingleton(); }

		static RE::Actor* GetDead() { return GetForm<RE::Actor>(0x4526E); }
	};

	namespace Testing
	{
		namespace
		{
			constexpr static const char* moduleName = "OutfitManager.RegularDistribution.Alive";

			// A bit useless, but for the sake of completeness :)
			TEST(B13NoWearsNoGetsNoLinksIgnore) {
				SETUP(Alive, None, Regular, NotFinal, None, None);
				EXPECT(pendingReplacements.find(actor->formID) == pendingReplacements.end(), "Expected Ignore action");
			}

			TEST(B14NoWearsNoGetsLinksOverwrite) {
				SETUP(Alive, None, Regular, NotFinal, None, Elven);
				manager.SetDefaultOutfit(NPCData(actor), links, NotFinal);
				auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
				CHECK(replacement, Regular, NotFinal);
				EXPECT_WORN(replacement, links);
			}

			TEST(B15NoWearsGetsNoLinksUpdate)
			{
				SETUP(Alive, None, Regular, NotFinal, Elven, None);
				manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
				auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
				CHECK(replacement, Regular, NotFinal);
				EXPECT_WORN(replacement, gets);
			}

			TEST(B16NoWearsFinalGetsNoLinksUpdate)
			{
				SETUP(Alive, None, Regular, NotFinal, Elven, None);
				manager.SetDefaultOutfit(NPCData(actor), gets, Final);
				auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
				CHECK(replacement, Regular, Final);
				EXPECT_WORN(replacement, gets);
			}

			TEST(B17NoWearsGetsLinksOverwrite)
			{
				SETUP(Alive, None, Regular, NotFinal, Elven, Hunter);
				manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
				manager.SetDefaultOutfit(NPCData(actor), links, NotFinal);
				auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
				CHECK(replacement, Regular, NotFinal);
				EXPECT_WORN(replacement, links);
			}

			TEST(B18NoWearsFinalGetsLinksUpdate)
			{
				SETUP(Alive, None, Regular, NotFinal, Elven, Hunter);
				manager.SetDefaultOutfit(NPCData(actor), gets, Final);
				manager.SetDefaultOutfit(NPCData(actor), links, NotFinal);
				auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
				CHECK(replacement, Regular, Final);
				EXPECT_WORN(replacement, gets);
			}

			TEST(B19WearsNoGetsNoLinksRevert)
			{
				SETUP(Alive, Guard, Regular, NotFinal, None, None);
				manager.SetDefaultOutfit(NPCData(actor), nullptr, NotFinal);
				auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
				ASSERT(!replacement, "Expected no outfit replacement to occur");
				EXPECT_ORIGINAL(original);
			}

			TEST(B20FinalWearsNoGetsNoLinksForward)
			{
				SETUP(Alive, Guard, Regular, Final, None, None);
				manager.SetDefaultOutfit(NPCData(actor), nullptr, NotFinal);
				auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
				CHECK(replacement, Regular, Final);
				EXPECT_WORN(replacement, wears);
			}

			TEST(B21WearsNoGetsLinksOverwrite)
			{
				SETUP(Alive, Guard, Regular, NotFinal, None, Hunter);
				manager.SetDefaultOutfit(NPCData(actor), links, NotFinal);
				auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
				CHECK(replacement, Regular, NotFinal);
				EXPECT_WORN(replacement, links);
			}

			TEST(B22FinalWearsNoGetsLinksForward)
			{
				SETUP(Alive, Guard, Regular, Final, None, Hunter);
				manager.SetDefaultOutfit(NPCData(actor), links, NotFinal);
				auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
				auto  it = wornReplacements.find(actor->formID);
				ASSERT(it != wornReplacements.end(), "Expected worn outfit still be in the map");
				ASSERT(!replacement, "Expected no outfit replacement to occur");
				CHECK((&it->second), Regular, Final);
				EXPECT_WORN((&it->second), wears);
			}

			TEST(B23WearsGetsNoLinksUpdate)
			{
				SETUP(Alive, Guard, Regular, NotFinal, Elven, None);
				manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
				auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
				CHECK(replacement, Regular, NotFinal);
				EXPECT_WORN(replacement, gets);
			}

			TEST(B24WearsFinalGetsNoLinksUpdate)
			{
				SETUP(Alive, Guard, Regular, NotFinal, Elven, None);
				manager.SetDefaultOutfit(NPCData(actor), gets, Final);
				auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
				CHECK(replacement, Regular, Final);
				EXPECT_WORN(replacement, gets);
			}

			TEST(B25FinalWearsGetsNoLinksForward)
			{
				SETUP(Alive, Guard, Regular, Final, Elven, None);
				manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
				auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
				auto  it = wornReplacements.find(actor->formID);
				ASSERT(it != wornReplacements.end(), "Expected worn outfit still be in the map");
				ASSERT(!replacement, "Expected no outfit replacement to occur");
				CHECK((&it->second), Regular, Final);
				EXPECT_WORN((&it->second), wears);
			}

			TEST(B26FinalWearsFinalGetsNoLinksForward)
			{
				SETUP(Alive, Guard, Regular, Final, Elven, None);
				manager.SetDefaultOutfit(NPCData(actor), gets, Final);
				auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
				auto  it = wornReplacements.find(actor->formID);
				ASSERT(it != wornReplacements.end(), "Expected worn outfit still be in the map");
				ASSERT(!replacement, "Expected no outfit replacement to occur");
				CHECK((&it->second), Regular, Final);
				EXPECT_WORN((&it->second), wears);
			}

			TEST(B27WearsGetsLinksOverwrite)
			{
				SETUP(Alive, Guard, Regular, NotFinal, Elven, Hunter);
				manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
				manager.SetDefaultOutfit(NPCData(actor), links, NotFinal);
				auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
				CHECK(replacement, Regular, NotFinal);
				EXPECT_WORN(replacement, links);
			}

			TEST(B28WearsFinalGetsLinksUpdate)
			{
				SETUP(Alive, Guard, Regular, NotFinal, Elven, Hunter);
				manager.SetDefaultOutfit(NPCData(actor), gets, Final);
				manager.SetDefaultOutfit(NPCData(actor), links, NotFinal);
				auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
				CHECK(replacement, Regular, Final);
				EXPECT_WORN(replacement, gets);
			}

			TEST(B29FinalWearsGetsLinksForward)
			{
				SETUP(Alive, Guard, Regular, Final, Elven, Hunter);
				manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
				manager.SetDefaultOutfit(NPCData(actor), links, NotFinal);
				auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
				auto  it = wornReplacements.find(actor->formID);
				ASSERT(it != wornReplacements.end(), "Expected worn outfit still be in the map");
				ASSERT(!replacement, "Expected no outfit replacement to occur");
				CHECK((&it->second), Regular, Final);
				EXPECT_WORN((&it->second), wears);
			}

			TEST(B30FinalWearsFinalGetsLinksForward)
			{
				SETUP(Alive, Guard, Regular, Final, Elven, Hunter);
				manager.SetDefaultOutfit(NPCData(actor), gets, Final);
				manager.SetDefaultOutfit(NPCData(actor), links, NotFinal);
				auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
				auto  it = wornReplacements.find(actor->formID);
				ASSERT(it != wornReplacements.end(), "Expected worn outfit still be in the map");
				ASSERT(!replacement, "Expected no outfit replacement to occur");
				CHECK((&it->second), Regular, Final);
				EXPECT_WORN((&it->second), wears);
			}
		}
	}
};
#undef SETUP
#undef Death
#undef Regular
#undef Final
#undef NotFinal
#undef CHECK
