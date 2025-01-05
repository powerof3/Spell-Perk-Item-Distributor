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
		TestsHelper::ApplyOutfit(manager, actor, worn);                                \
		wornReplacements[actor->GetFormID()] = { original, worn, isWd, isWf, false };  \
	}
// Terms from the Table https://docs.google.com/spreadsheets/d/1JEhAql7hUURYC63k_fScZ9u8OWni6gN9jHzytNQt-m8/edit?usp=sharing
#define Death true
#define Regular false
#define Final true
#define NotFinal false

#define CHECK(replacement, d, f)                                                                                                                                     \
	ASSERT(replacement, "Expected to have a replacement")                                                                                                            \
	ASSERT(replacement->original == original, fmt::format("Expected replacement to keep track of original outfit {}, but got", *original, *(replacement->original))) \
	ASSERT(replacement->isDeathOutfit == d, fmt::format("Expected replacement to be {} outfit", d ? "Death" : "Regular"))                                            \
	ASSERT(replacement->isFinalOutfit == f, fmt::format("Expected replacement to be {} outfit", f ? "Final" : "NOT Final"))

#define EXPECT_NONE(replacement) ASSERT(!replacement, "Expected no outfit replacement to occur");

#define EXPECT_ORIGINAL(outfit)                                                                                           \
	{                                                                                                                     \
		auto it = wornReplacements.find(actor->formID);                                                                   \
		ASSERT(it == wornReplacements.end(), "Expected to have no Worn Replacement for actor");                           \
		auto actual = actor->GetActorBase()->defaultOutfit;                                                               \
		ASSERT(actual == outfit, fmt::format("Expected actor to wear original outfit {}, but got {}", *outfit, *actual)); \
		PASS;                                                                                                             \
	}
#define EXPECT_WORN(replacement, outfit, d, f)                                                                                             \
	{                                                                                                                                      \
		ASSERT(replacement, "Expected to have a Worn Replacement for actor");                                                              \
		auto distributed = replacement->distributed;                                                                                       \
		auto actual = actor->GetActorBase()->defaultOutfit;                                                                                \
		ASSERT(distributed == outfit, fmt::format("Expected Worn Replacement to have distributed {}, but got {}", *outfit, *distributed)); \
		ASSERT(actual == outfit, fmt::format("Expected actor to wear worn outfit {}, but got {}", *outfit, *actual));                      \
		CHECK(replacement, d, f);                                                                                                          \
		PASS;                                                                                                                              \
	}

#define EXPECT_WORN_FIND(outfit, d, f)                                                                                                     \
	{                                                                                                                                      \
		auto it = wornReplacements.find(actor->formID);                                                                                    \
		ASSERT(it != wornReplacements.end(), "Expected to have a Worn Replacement for actor");                                             \
		auto distributed = it->second.distributed;                                                                                         \
		auto actual = actor->GetActorBase()->defaultOutfit;                                                                                \
		ASSERT(distributed == outfit, fmt::format("Expected Worn Replacement to have distributed {}, but got {}", *outfit, *distributed)); \
		ASSERT(actual == outfit, fmt::format("Expected actor to wear worn outfit {}, but got {}", *outfit, *actual));                      \
		CHECK((&it->second), d, f);                                                                                                        \
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

		static bool ApplyOutfit(Manager& manager, RE::Actor* actor, RE::BGSOutfit* outfit)
		{
			return manager.ApplyOutfit(actor, outfit);
		}

		static auto* ResolveAndApplyWornOutfit(Manager& manager, RE::Actor* actor, bool isDying)
		{
			auto* replacement = manager.ResolveWornOutfit(actor, isDying);
			if (replacement) {
				ApplyOutfit(manager, actor, replacement->distributed);
			}
			return replacement;
		}

		static void Loot(RE::Actor* actor) {
			auto outfit = actor->GetActorBase()->defaultOutfit;
			actor->RemoveOutfitItems(outfit);
		}

		static RE::BGSOutfit* GetElvenOutfit() { return GetForm<RE::BGSOutfit>(0x57A22); }

		static RE::BGSOutfit* GetGuardOutfit() { return GetForm<RE::BGSOutfit>(0xFF282); }

		static RE::BGSOutfit* GetHunterOutfit() { return GetForm<RE::BGSOutfit>(0x106988); }

		static RE::BGSOutfit* GetFarmOutfit() { return GetForm<RE::BGSOutfit>(0x1DC10); }

		static RE::BGSOutfit* GetNoneOutfit() { return nullptr; }

		static RE::Actor* GetAlive() { 
			auto actor = GetForm<RE::Actor>(0x198B0);
			actor->Resurrect(true, true);
			return actor; 
		}

		static RE::Actor* GetDead() { 
			auto actor = GetForm<RE::Actor>(0x198B0);
			actor->ResetInventory(false);
			actor->KillImmediate();                      
			return actor; 
		}
	};

	namespace Testing
	{
		namespace RegularDistribution
		{
			namespace Alive
			{
				constexpr static const char* moduleName = "OutfitManager.RegularDistribution.Alive";

				TEST(B05NoWearsNoGetsNoLinksIgnore)
				{
					SETUP(Alive, None, Regular, NotFinal, None, None);
					manager.SetDefaultOutfit(NPCData(actor), nullptr, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_ORIGINAL(original);
				}

				TEST(B06NoWearsGetsNoLinksUpdate)
				{
					SETUP(Alive, None, Regular, NotFinal, Elven, None);
					manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_WORN(replacement, gets, Regular, NotFinal);
				}

				TEST(B07NoWearsFinalGetsNoLinksUpdate)
				{
					SETUP(Alive, None, Regular, NotFinal, Elven, None);
					manager.SetDefaultOutfit(NPCData(actor), gets, Final);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_WORN(replacement, gets, Regular, Final);
				}

				TEST(B08NoWearsGetsLinksOverwrite)
				{
					SETUP(Alive, None, Regular, NotFinal, Elven, Hunter);
					manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
					manager.SetDefaultOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_WORN(replacement, links, Regular, NotFinal);
				}

				TEST(B09NoWearsFinalGetsLinksUpdate)
				{
					SETUP(Alive, None, Regular, NotFinal, Elven, Hunter);
					manager.SetDefaultOutfit(NPCData(actor), gets, Final);
					manager.SetDefaultOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_WORN(replacement, gets, Regular, Final);
				}

				TEST(B10WearsNoGetsNoLinksRevert)
				{
					SETUP(Alive, Guard, Regular, NotFinal, None, None);
					manager.SetDefaultOutfit(NPCData(actor), nullptr, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_ORIGINAL(original);
				}

				TEST(B11FinalWearsNoGetsNoLinksForward)
				{
					SETUP(Alive, Guard, Regular, Final, None, None);
					manager.SetDefaultOutfit(NPCData(actor), nullptr, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Regular, Final);
				}

				TEST(B12WearsGetsNoLinksUpdate)
				{
					SETUP(Alive, Guard, Regular, NotFinal, Elven, None);
					manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_WORN(replacement, gets, Regular, NotFinal);
				}

				TEST(B13WearsFinalGetsNoLinksUpdate)
				{
					SETUP(Alive, Guard, Regular, NotFinal, Elven, None);
					manager.SetDefaultOutfit(NPCData(actor), gets, Final);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_WORN(replacement, gets, Regular, Final);
				}

				TEST(B14FinalWearsGetsNoLinksForward)
				{
					SETUP(Alive, Guard, Regular, Final, Elven, None);
					manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Regular, Final);
				}

				TEST(B15FinalWearsFinalGetsNoLinksForward)
				{
					SETUP(Alive, Guard, Regular, Final, Elven, None);
					manager.SetDefaultOutfit(NPCData(actor), gets, Final);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Regular, Final);
				}

				TEST(B16WearsGetsLinksOverwrite)
				{
					SETUP(Alive, Guard, Regular, NotFinal, Elven, Hunter);
					manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
					manager.SetDefaultOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_WORN(replacement, links, Regular, NotFinal);
				}

				TEST(B17WearsFinalGetsLinksUpdate)
				{
					SETUP(Alive, Guard, Regular, NotFinal, Elven, Hunter);
					manager.SetDefaultOutfit(NPCData(actor), gets, Final);
					manager.SetDefaultOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_WORN(replacement, gets, Regular, Final);
				}

				TEST(B18FinalWearsGetsLinksForward)
				{
					SETUP(Alive, Guard, Regular, Final, Elven, Hunter);
					manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
					manager.SetDefaultOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Regular, Final);
				}

				TEST(B19FinalWearsFinalGetsLinksForward)
				{
					SETUP(Alive, Guard, Regular, Final, Elven, Hunter);
					manager.SetDefaultOutfit(NPCData(actor), gets, Final);
					manager.SetDefaultOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Regular, Final);
				}
			}

			namespace Dead
			{
				constexpr static const char* moduleName = "OutfitManager.RegularDistribution.Dead";

				CLEANUP
				{
					TestsHelper::GetAlive();  // resurrect the actor that we used for tests, to reset their inventory
				}

				TEST(B35NoWearsNoGetsNoLinksPersist)
				{
					SETUP(Dead, None, Regular, NotFinal, None, None);
					manager.SetDefaultOutfit(NPCData(actor), nullptr, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_WORN(replacement, original, Regular, NotFinal);
				}

				TEST(B36NoWearsGetsNoLinksUpdate)
				{
					SETUP(Dead, None, Regular, NotFinal, Elven, None);
					manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_WORN(replacement, gets, Regular, NotFinal);
				}

				TEST(B37NoWearsFinalGetsNoLinksUpdate)
				{
					SETUP(Dead, None, Regular, NotFinal, Elven, None);
					manager.SetDefaultOutfit(NPCData(actor), gets, Final);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_WORN(replacement, gets, Regular, Final);
				}

				TEST(B38NoWearsGetsLinksOverwrite)
				{
					SETUP(Dead, None, Regular, NotFinal, Elven, Hunter);
					manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
					manager.SetDefaultOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_WORN(replacement, links, Regular, NotFinal);
				}

				TEST(B39NoWearsFinalGetsLinksUpdate)
				{
					SETUP(Dead, None, Regular, NotFinal, Elven, Hunter);
					manager.SetDefaultOutfit(NPCData(actor), gets, Final);
					manager.SetDefaultOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_WORN(replacement, gets, Regular, Final);
				}

				TEST(B40LootedNoWearsGetsNoLinksIgnore)
				{
					SETUP(Dead, None, Regular, NotFinal, Elven, None);
					TestsHelper::Loot(actor);
					manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_ORIGINAL(original);
				}

				TEST(B41LootedNoWearsFinalGetsNoLinksIgnore)
				{
					SETUP(Dead, None, Regular, NotFinal, Elven, None);
					TestsHelper::Loot(actor);
					manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_ORIGINAL(original);
				}

				TEST(B42LootedNoWearsGetsLinksIgnore)
				{
					SETUP(Dead, None, Regular, NotFinal, Elven, Hunter);
					TestsHelper::Loot(actor);
					manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
					manager.SetDefaultOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_ORIGINAL(original);
				}

				TEST(B43LootedNoWearsFinalGetsLinksIgnore)
				{
					SETUP(Dead, None, Regular, NotFinal, Elven, Hunter);
					TestsHelper::Loot(actor);
					manager.SetDefaultOutfit(NPCData(actor), gets, Final);
					manager.SetDefaultOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_ORIGINAL(original);
				}

				TEST(B44WearsNoGetsNoLinksForward)
				{
					SETUP(Dead, Guard, Regular, NotFinal, None, None);
					manager.SetDefaultOutfit(NPCData(actor), nullptr, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Regular, NotFinal);
				}

				TEST(B45LootedWearsNoGetsNoLinksForward)
				{
					SETUP(Dead, Guard, Regular, NotFinal, None, None);
					TestsHelper::Loot(actor);
					manager.SetDefaultOutfit(NPCData(actor), nullptr, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Regular, NotFinal);
				}

				TEST(B46FinalWearsNoGetsNoLinksForward)
				{
					SETUP(Dead, Guard, Regular, Final, None, None);
					manager.SetDefaultOutfit(NPCData(actor), nullptr, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Regular, Final);
				}

				TEST(B47WearsGetsNoLinksForward)
				{
					SETUP(Dead, Guard, Regular, NotFinal, Elven, None);
					manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Regular, NotFinal);
				}

				TEST(B48WearsFinalGetsNoLinksForward)
				{
					SETUP(Dead, Guard, Regular, NotFinal, Elven, None);
					manager.SetDefaultOutfit(NPCData(actor), gets, Final);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Regular, NotFinal);
				}

				TEST(B49LootedWearsGetsNoLinksForward)
				{
					SETUP(Dead, Guard, Regular, NotFinal, Elven, None);
					TestsHelper::Loot(actor);
					manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Regular, NotFinal);
				}

				TEST(B50LootedWearsFinalGetsNoLinksForward)
				{
					SETUP(Dead, Guard, Regular, NotFinal, Elven, None);
					TestsHelper::Loot(actor);
					manager.SetDefaultOutfit(NPCData(actor), gets, Final);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Regular, NotFinal);
				}

				TEST(B51FinalWearsGetsNoLinksForward)
				{
					SETUP(Dead, Guard, Regular, Final, Elven, None);
					manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Regular, Final);
				}

				TEST(B52FinalWearsFinalGetsNoLinksForward)
				{
					SETUP(Dead, Guard, Regular, Final, Elven, None);
					manager.SetDefaultOutfit(NPCData(actor), gets, Final);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Regular, Final);
				}

				TEST(B53WearsGetsLinksForward)
				{
					SETUP(Dead, Guard, Regular, NotFinal, Elven, Hunter);
					manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
					manager.SetDefaultOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Regular, NotFinal);
				}

				TEST(B54WearsFinalGetsLinksForward)
				{
					SETUP(Dead, Guard, Regular, NotFinal, Elven, Hunter);
					manager.SetDefaultOutfit(NPCData(actor), gets, Final);
					manager.SetDefaultOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Regular, NotFinal);
				}

				TEST(B55LootedWearsGetsLinksForward)
				{
					SETUP(Dead, Guard, Regular, NotFinal, Elven, Hunter);
					TestsHelper::Loot(actor);
					manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
					manager.SetDefaultOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Regular, NotFinal);
				}

				TEST(B56LootedWearsFinalGetsLinksForward)
				{
					SETUP(Dead, Guard, Regular, NotFinal, Elven, Hunter);
					TestsHelper::Loot(actor);
					manager.SetDefaultOutfit(NPCData(actor), gets, Final);
					manager.SetDefaultOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Regular, NotFinal);
				}

				TEST(B57FinalWearsGetsLinksForward)
				{
					SETUP(Dead, Guard, Regular, Final, Elven, Hunter);
					manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
					manager.SetDefaultOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Regular, Final);
				}

				TEST(B58FinalWearsFinalGetsLinksForward)
				{
					SETUP(Dead, Guard, Regular, Final, Elven, Hunter);
					manager.SetDefaultOutfit(NPCData(actor), gets, Final);
					manager.SetDefaultOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Regular, Final);
				}

				TEST(B59DeathWearsNoGetsNoLinksForward)
				{
					SETUP(Dead, Guard, Death, NotFinal, None, None);
					manager.SetDefaultOutfit(NPCData(actor), nullptr, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Death, NotFinal);
				}

				TEST(B60FinalDeathWearsNoGetsNoLinksForward)
				{
					SETUP(Dead, Guard, Death, Final, None, None);
					manager.SetDefaultOutfit(NPCData(actor), nullptr, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Death, Final);
				}

				TEST(B61DeathWearsGetsNoLinksForward)
				{
					SETUP(Dead, Guard, Death, NotFinal, Elven, None);
					manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Death, NotFinal);
				}

				TEST(B62DeathWearsFinalGetsNoLinksForward)
				{
					SETUP(Dead, Guard, Death, NotFinal, Elven, None);
					manager.SetDefaultOutfit(NPCData(actor), gets, Final);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Death, NotFinal);
				}

				TEST(B63FinalDeathWearsGetsNoLinksForward)
				{
					SETUP(Dead, Guard, Death, Final, Elven, None);
					manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Death, Final);
				}

				TEST(B64FinalDeathWearsFinalGetsNoLinksForward)
				{
					SETUP(Dead, Guard, Death, Final, Elven, None);
					manager.SetDefaultOutfit(NPCData(actor), gets, Final);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Death, Final);
				}

				TEST(B65DeathWearsGetsLinksForward)
				{
					SETUP(Dead, Guard, Death, NotFinal, Elven, Hunter);
					manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
					manager.SetDefaultOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Death, NotFinal);
				}

				TEST(B66DeathWearsFinalGetsLinksForward)
				{
					SETUP(Dead, Guard, Death, NotFinal, Elven, Hunter);
					manager.SetDefaultOutfit(NPCData(actor), gets, Final);
					manager.SetDefaultOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Death, NotFinal);
				}

				TEST(B67FinalDeathWearsGetsLinksForward)
				{
					SETUP(Dead, Guard, Death, Final, Elven, Hunter);
					manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
					manager.SetDefaultOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Death, Final);
				}

				TEST(B68FinalDeathWearsFinalGetsLinksForward)
				{
					SETUP(Dead, Guard, Death, Final, Elven, Hunter);
					manager.SetDefaultOutfit(NPCData(actor), gets, Final);
					manager.SetDefaultOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Death, Final);
				}

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
