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
	bool           isLooted = false;                                                   \
	if (worn) {                                                                        \
		TestsHelper::SwapOutfit(manager, actor, original, worn);                       \
		wornReplacements[actor->GetFormID()] = { original, worn, isWd, isWf, false };  \
	}
// Terms from the Table https://docs.google.com/spreadsheets/d/1JEhAql7hUURYC63k_fScZ9u8OWni6gN9jHzytNQt-m8/edit?usp=sharing
#define Death true
#define Regular false
#define Final true
#define NotFinal false

#define CHECK(replacement, d, f)                                                                                                                                     \
	ASSERT(replacement, "Expected to have a replacement")                                                                                                            \
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
#define EXPECT_WORN(replacement, outfit, d, f)                                                                                                      \
	{                                                                                                                                               \
		ASSERT(replacement, "Expected to have a Worn Replacement for actor");                                                                       \
		auto distributed = replacement->distributed;                                                                                                \
		auto actual = actor->GetActorBase()->defaultOutfit;                                                                                         \
		ASSERT(distributed == outfit, fmt::format("Expected Worn Replacement to have distributed {}, but got {}", *outfit, *distributed));          \
		ASSERT(actual == original, fmt::format("Expected NPC's defaultOutfit to be unchanged {}, but got {}", *original, *actual));                 \
		if (isLooted) {                                                                                                                             \
			ASSERT(!actor->HasOutfitItems(outfit), fmt::format("Expected actor to have no items from outfit {}, but they have it", *outfit));       \
		} else {                                                                                                                                    \
			ASSERT(actor->HasOutfitItems(outfit), fmt::format("Expected actor to have all items from outfit {}, but they don't have it", *outfit)); \
		}                                                                                                                                           \
		CHECK(replacement, d, f);                                                                                                                   \
		PASS;																																		\
	}
#define LOOT         \
	isLooted = true; \
	TestsHelper::Loot(actor);

#define EXPECT_WORN_FIND(outfit, d, f)                                                                                                              \
	{                                                                                                                                               \
		auto it = wornReplacements.find(actor->formID);                                                                                             \
		ASSERT(it != wornReplacements.end(), "Expected to have a Worn Replacement for actor");                                                      \
		auto distributed = it->second.distributed;                                                                                                  \
		auto actual = actor->GetActorBase()->defaultOutfit;                                                                                         \
		ASSERT(distributed == outfit, fmt::format("Expected Worn Replacement to have distributed {}, but got {}", *outfit, *distributed));          \
		ASSERT(actual == original, fmt::format("Expected NPC's defaultOutfit to be unchanged {}, but got {}", *original, *actual));                 \
		if (isLooted) {                                                                                                                             \
			ASSERT(!actor->HasOutfitItems(outfit), fmt::format("Expected actor to have no items from outfit {}, but they have it", *outfit));       \
		} else {                                                                                                                                    \
			ASSERT(actor->HasOutfitItems(outfit), fmt::format("Expected actor to have all items from outfit {}, but they don't have it", *outfit)); \
		}                                                                                                                                           \
		CHECK((&it->second), d, f);                                                                                                                 \
		PASS;                                                                                                                                       \
	}

namespace Outfits
{
	using namespace Testing;

	struct TestsHelper
	{
		// Exposes private members through firend TestsHelper;
		static auto& GetWornReplacements(Manager& manager) { return manager.wornReplacements; }

		static auto& GetPendingReplacements(Manager& manager) { return manager.pendingReplacements; }

		static bool SwapOutfit(Manager& manager, RE::Actor* actor, RE::BGSOutfit* toRemove, RE::BGSOutfit* toWear)
		{
			return manager.SwapOutfit(actor, toRemove, toWear);
		}

		static auto* ResolveAndApplyWornOutfit(Manager& manager, RE::Actor* actor, bool isDying)
		{
			auto* replacement = manager.ResolveWornOutfit(actor, isDying);
			if (replacement) {
				SwapOutfit(manager, actor, replacement->previous, replacement->distributed);
			}
			return replacement;
		}

		static void Loot(RE::Actor* actor) {
			actor->RemoveOutfitItems(nullptr);
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
		namespace Basic
		{
			constexpr static const char* moduleName = "OutfitManager.Basic";

			TEST(SwapOutfitEquipsNewOutfitItemsWithoutChangingDefaultOutfit)
			{
				SETUP(Alive, None, Regular, NotFinal, Elven, None);
				TestsHelper::SwapOutfit(manager, actor, original, gets);
				auto actual = actor->GetActorBase()->defaultOutfit;
				ASSERT(actual == original, fmt::format("Expected NPC's defaultOutfit to be unchanged {}, but got {}", *original, *actual));
				ASSERT(actor->HasOutfitItems(gets), fmt::format("Expected actor to have all items from outfit {}, but they don't have it", *gets));
				PASS;
			}
		}

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
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(original, Regular, NotFinal);
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
					LOOT;
					manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_ORIGINAL(original);
				}

				TEST(B41LootedNoWearsFinalGetsNoLinksIgnore)
				{
					SETUP(Dead, None, Regular, NotFinal, Elven, None);
					LOOT;
					manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_ORIGINAL(original);
				}

				TEST(B42LootedNoWearsGetsLinksIgnore)
				{
					SETUP(Dead, None, Regular, NotFinal, Elven, Hunter);
					LOOT;
					manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
					manager.SetDefaultOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_ORIGINAL(original);
				}

				TEST(B43LootedNoWearsFinalGetsLinksIgnore)
				{
					SETUP(Dead, None, Regular, NotFinal, Elven, Hunter);
					LOOT;
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
					LOOT;
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
					LOOT;
					manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Regular, NotFinal);
				}

				TEST(B50LootedWearsFinalGetsNoLinksForward)
				{
					SETUP(Dead, Guard, Regular, NotFinal, Elven, None);
					LOOT;
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
					LOOT;
					manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
					manager.SetDefaultOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Regular, NotFinal);
				}

				TEST(B56LootedWearsFinalGetsLinksForward)
				{
					SETUP(Dead, Guard, Regular, NotFinal, Elven, Hunter);
					LOOT;
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

		namespace DeathDistribution
		{
			namespace Dying
			{
				constexpr static const char* moduleName = "OutfitManager.DeathDistribution.Dying";

				TEST(J20NoWearsNoGetsNoLinksPersist)
				{
					SETUP(Alive, None, Regular, NotFinal, None, None);
					manager.SetDeathOutfit(NPCData(actor, true), nullptr, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, true);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(original, Death, NotFinal);
				}

				TEST(J21NoWearsGetsNoLinksUpdate)
				{
					SETUP(Alive, None, Regular, NotFinal, Elven, None);
					manager.SetDeathOutfit(NPCData(actor, true), gets, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, true);
					EXPECT_WORN(replacement, gets, Death, NotFinal);
				}

				TEST(J22NoWearsFinalGetsNoLinksUpdate)
				{
					SETUP(Alive, None, Regular, NotFinal, Elven, None);
					manager.SetDeathOutfit(NPCData(actor, true), gets, Final);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, true);
					EXPECT_WORN(replacement, gets, Death, Final);
				}

				TEST(J23NoWearsGetsLinksOverwrite)
				{
					SETUP(Alive, None, Regular, NotFinal, Elven, Hunter);
					manager.SetDeathOutfit(NPCData(actor, true), gets, NotFinal);
					manager.SetDeathOutfit(NPCData(actor, true), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, true);
					EXPECT_WORN(replacement, links, Death, NotFinal);
				}

				TEST(J24NoWearsFinalGetsLinksUpdate)
				{
					SETUP(Alive, None, Regular, NotFinal, Elven, Hunter);
					manager.SetDeathOutfit(NPCData(actor, true), gets, Final);
					manager.SetDeathOutfit(NPCData(actor, true), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, true);
					EXPECT_WORN(replacement, gets, Death, Final);
				}

				TEST(J25WearsNoGetsNoLinksPersist)
				{
					SETUP(Alive, Guard, Regular, NotFinal, None, None);
					manager.SetDeathOutfit(NPCData(actor, true), nullptr, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, true);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Death, NotFinal);
				}

				TEST(J26FinalWearsNoGetsNoLinksPersist)
				{
					SETUP(Alive, Guard, Regular, Final, None, None);
					manager.SetDeathOutfit(NPCData(actor, true), nullptr, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, true);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Death, Final);
				}

				TEST(J27WearsGetsNoLinksUpdate)
				{
					SETUP(Alive, Guard, Regular, NotFinal, Elven, None);
					manager.SetDeathOutfit(NPCData(actor, true), gets, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, true);
					EXPECT_WORN(replacement, gets, Death, NotFinal);
				}

				TEST(J28WearsFinalGetsNoLinksUpdate)
				{
					SETUP(Alive, Guard, Regular, NotFinal, Elven, None);
					manager.SetDeathOutfit(NPCData(actor, true), gets, Final);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, true);
					EXPECT_WORN(replacement, gets, Death, Final);
				}

				TEST(J29FinalWearsGetsNoLinksUpdate)
				{
					SETUP(Alive, Guard, Regular, Final, Elven, None);
					manager.SetDeathOutfit(NPCData(actor, true), gets, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, true);
					EXPECT_WORN(replacement, gets, Death, NotFinal);
				}

				TEST(J30FinalWearsFinalGetsNoLinksUpdate)
				{
					SETUP(Alive, Guard, Regular, Final, Elven, None);
					manager.SetDeathOutfit(NPCData(actor, true), gets, Final);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, true);
					EXPECT_WORN(replacement, gets, Death, Final);
				}

				TEST(J31WearsGetsLinksOverwrite)
				{
					SETUP(Alive, Guard, Regular, NotFinal, Elven, Hunter);
					manager.SetDeathOutfit(NPCData(actor, true), gets, NotFinal);
					manager.SetDeathOutfit(NPCData(actor, true), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, true);
					EXPECT_WORN(replacement, links, Death, NotFinal);
				}

				TEST(J32WearsFinalGetsLinksUpdate)
				{
					SETUP(Alive, Guard, Regular, NotFinal, Elven, Hunter);
					manager.SetDeathOutfit(NPCData(actor, true), gets, Final);
					manager.SetDeathOutfit(NPCData(actor, true), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, true);
					EXPECT_WORN(replacement, gets, Death, Final);
				}

				TEST(J33FinalWearsGetsLinksOverwrite)
				{
					SETUP(Alive, Guard, Regular, Final, Elven, Hunter);
					manager.SetDeathOutfit(NPCData(actor, true), gets, NotFinal);
					manager.SetDeathOutfit(NPCData(actor, true), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, true);
					EXPECT_WORN(replacement, links, Death, NotFinal);
				}

				TEST(J34FinalWearsFinalGetsLinksUpdate)
				{
					SETUP(Alive, Guard, Regular, Final, Elven, Hunter);
					manager.SetDeathOutfit(NPCData(actor, true), gets, Final);
					manager.SetDeathOutfit(NPCData(actor, true), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, true);
					EXPECT_WORN(replacement, gets, Death, Final);
				}
			}

			namespace Dead
			{
				constexpr static const char* moduleName = "OutfitManager.DeathDistribution.Dead";

				CLEANUP
				{
					TestsHelper::GetAlive();  // resurrect the actor that we used for tests, to reset their inventory
				}

				TEST(J35NoWearsNoGetsNoLinksPersist)
				{
					SETUP(Dead, None, Regular, NotFinal, None, None);
					manager.SetDeathOutfit(NPCData(actor), nullptr, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(original, Death, NotFinal);
				}

				TEST(J36NoWearsGetsNoLinksUpdate)
				{
					SETUP(Dead, None, Regular, NotFinal, Elven, None);
					manager.SetDeathOutfit(NPCData(actor), gets, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_WORN(replacement, gets, Death, NotFinal);
				}

				TEST(J37NoWearsFinalGetsNoLinksUpdate)
				{
					SETUP(Dead, None, Regular, NotFinal, Elven, None);
					manager.SetDeathOutfit(NPCData(actor), gets, Final);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_WORN(replacement, gets, Death, Final);
				}

				TEST(J38NoWearsGetsLinksOverwrite)
				{
					SETUP(Dead, None, Regular, NotFinal, Elven, Hunter);
					manager.SetDeathOutfit(NPCData(actor), gets, NotFinal);
					manager.SetDeathOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_WORN(replacement, links, Death, NotFinal);
				}

				TEST(J39NoWearsFinalGetsLinksUpdate)
				{
					SETUP(Dead, None, Regular, NotFinal, Elven, Hunter);
					manager.SetDeathOutfit(NPCData(actor), gets, Final);
					manager.SetDeathOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_WORN(replacement, gets, Death, Final);
				}

				TEST(J40LootedNoWearsGetsNoLinksIgnore)
				{
					SETUP(Dead, None, Regular, NotFinal, Elven, None);
					LOOT;
					manager.SetDeathOutfit(NPCData(actor), gets, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_ORIGINAL(original);
				}

				TEST(J41LootedNoWearsFinalGetsNoLinksIgnore)
				{
					SETUP(Dead, None, Regular, NotFinal, Elven, None);
					LOOT;
					manager.SetDeathOutfit(NPCData(actor), gets, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_ORIGINAL(original);
				}

				TEST(J42LootedNoWearsGetsLinksIgnore)
				{
					SETUP(Dead, None, Regular, NotFinal, Elven, Hunter);
					LOOT;
					manager.SetDeathOutfit(NPCData(actor), gets, NotFinal);
					manager.SetDeathOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_ORIGINAL(original);
				}

				TEST(J43LootedNoWearsFinalGetsLinksIgnore)
				{
					SETUP(Dead, None, Regular, NotFinal, Elven, Hunter);
					LOOT;
					manager.SetDeathOutfit(NPCData(actor), gets, Final);
					manager.SetDeathOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_ORIGINAL(original);
				}

				TEST(J44WearsNoGetsNoLinksPersist)
				{
					SETUP(Dead, Guard, Regular, NotFinal, None, None);
					manager.SetDeathOutfit(NPCData(actor), nullptr, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Death, NotFinal);
				}

				TEST(J45LootedWearsNoGetsNoLinksPersist)
				{
					SETUP(Dead, Guard, Regular, NotFinal, None, None);
					LOOT;
					manager.SetDeathOutfit(NPCData(actor), nullptr, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Death, NotFinal);
				}

				TEST(J46FinalWearsNoGetsNoLinksPersist)
				{
					SETUP(Dead, Guard, Regular, Final, None, None);
					manager.SetDeathOutfit(NPCData(actor), nullptr, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Death, Final);
				}

				TEST(J47WearsGetsNoLinksUpdate)
				{
					SETUP(Dead, Guard, Regular, NotFinal, Elven, None);
					manager.SetDeathOutfit(NPCData(actor), gets, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_WORN(replacement, gets, Death, NotFinal);
				}

				TEST(J48WearsFinalGetsNoLinksUpdsate)
				{
					SETUP(Dead, Guard, Regular, NotFinal, Elven, None);
					manager.SetDeathOutfit(NPCData(actor), gets, Final);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_WORN(replacement, gets, Death, Final);
				}

				TEST(J49LootedWearsGetsNoLinksForward)
				{
					SETUP(Dead, Guard, Regular, NotFinal, Elven, None);
					LOOT;
					manager.SetDeathOutfit(NPCData(actor), gets, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Regular, NotFinal);
				}

				TEST(J50LootedWearsFinalGetsNoLinksForward)
				{
					SETUP(Dead, Guard, Regular, NotFinal, Elven, None);
					LOOT;
					manager.SetDeathOutfit(NPCData(actor), gets, Final);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Regular, NotFinal);
				}

				TEST(J51FinalWearsGetsNoLinksUpdate)
				{
					SETUP(Dead, Guard, Regular, Final, Elven, None);
					manager.SetDeathOutfit(NPCData(actor), gets, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_WORN(replacement, gets, Death, NotFinal);
				}

				TEST(J52FinalWearsFinalGetsNoLinksUpdate)
				{
					SETUP(Dead, Guard, Regular, Final, Elven, None);
					manager.SetDeathOutfit(NPCData(actor), gets, Final);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_WORN(replacement, gets, Death, Final);
				}

				TEST(J53WearsGetsLinksOverwrite)
				{
					SETUP(Dead, Guard, Regular, NotFinal, Elven, Hunter);
					manager.SetDeathOutfit(NPCData(actor), gets, NotFinal);
					manager.SetDeathOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_WORN(replacement, links, Death, NotFinal);
				}

				TEST(J54WearsFinalGetsLinksUpdate)
				{
					SETUP(Dead, Guard, Regular, NotFinal, Elven, Hunter);
					manager.SetDeathOutfit(NPCData(actor), gets, Final);
					manager.SetDeathOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_WORN(replacement, gets, Death, Final);
				}

				TEST(J55LootedWearsGetsLinksForward)
				{
					SETUP(Dead, Guard, Regular, NotFinal, Elven, Hunter);
					LOOT;
					manager.SetDeathOutfit(NPCData(actor), gets, NotFinal);
					manager.SetDeathOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Regular, NotFinal);
				}

				TEST(J56LootedWearsFinalGetsLinksForward)
				{
					SETUP(Dead, Guard, Regular, NotFinal, Elven, Hunter);
					LOOT;
					manager.SetDeathOutfit(NPCData(actor), gets, Final);
					manager.SetDeathOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Regular, NotFinal);
				}

				TEST(J57FinalWearsGetsLinksOverwrite)
				{
					SETUP(Dead, Guard, Regular, Final, Elven, Hunter);
					manager.SetDeathOutfit(NPCData(actor), gets, NotFinal);
					manager.SetDeathOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_WORN(replacement, links, Death, NotFinal);
				}

				TEST(J58FinalWearsFinalGetsLinksUpdate)
				{
					SETUP(Dead, Guard, Regular, Final, Elven, Hunter);
					manager.SetDeathOutfit(NPCData(actor), gets, Final);
					manager.SetDeathOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_WORN(replacement, gets, Death, Final);
				}

				TEST(J59DeathWearsNoGetsNoLinksForward)
				{
					SETUP(Dead, Guard, Death, NotFinal, None, None);
					manager.SetDeathOutfit(NPCData(actor), nullptr, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Death, NotFinal);
				}

				TEST(J60FinalDeathWearsNoGetsNoLinksForward)
				{
					SETUP(Dead, Guard, Death, Final, None, None);
					manager.SetDeathOutfit(NPCData(actor), nullptr, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Death, Final);
				}

				TEST(J61DeathWearsGetsNoLinksForward)
				{
					SETUP(Dead, Guard, Death, NotFinal, Elven, None);
					manager.SetDeathOutfit(NPCData(actor), gets, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Death, NotFinal);
				}

				TEST(J62DeathWearsFinalGetsNoLinksForward)
				{
					SETUP(Dead, Guard, Death, NotFinal, Elven, None);
					manager.SetDeathOutfit(NPCData(actor), gets, Final);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Death, NotFinal);
				}

				TEST(J63FinalDeathWearsGetsNoLinksForward)
				{
					SETUP(Dead, Guard, Death, Final, Elven, None);
					manager.SetDeathOutfit(NPCData(actor), gets, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Death, Final);
				}

				TEST(J64FinalDeathWearsFinalGetsNoLinksForward)
				{
					SETUP(Dead, Guard, Death, Final, Elven, None);
					manager.SetDeathOutfit(NPCData(actor), gets, Final);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Death, Final);
				}

				TEST(J65DeathWearsGetsLinksForward)
				{
					SETUP(Dead, Guard, Death, NotFinal, Elven, Hunter);
					manager.SetDeathOutfit(NPCData(actor), gets, NotFinal);
					manager.SetDeathOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Death, NotFinal);
				}

				TEST(J66DeathWearsFinalGetsLinksForward)
				{
					SETUP(Dead, Guard, Death, NotFinal, Elven, Hunter);
					manager.SetDeathOutfit(NPCData(actor), gets, Final);
					manager.SetDeathOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Death, NotFinal);
				}

				TEST(J67FinalDeathWearsGetsLinksForward)
				{
					SETUP(Dead, Guard, Death, Final, Elven, Hunter);
					manager.SetDeathOutfit(NPCData(actor), gets, NotFinal);
					manager.SetDeathOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Death, Final);
				}

				TEST(J68FinalDeathWearsFinalGetsLinksForward)
				{
					SETUP(Dead, Guard, Death, Final, Elven, Hunter);
					manager.SetDeathOutfit(NPCData(actor), gets, Final);
					manager.SetDeathOutfit(NPCData(actor), links, NotFinal);
					auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
					EXPECT_NONE(replacement);
					EXPECT_WORN_FIND(wears, Death, Final);
				}
			}
		}

		// Tests how layered distribution works (i.e. when both Regular and Death distribution attempt to give an outfit)
		namespace MixedDistribution
		{
			constexpr static const char* moduleName = "OutfitManager.MixedDistribution.Dead";

			CLEANUP
			{
				TestsHelper::GetAlive();  // resurrect the actor that we used for tests, to reset their inventory
			}

			TEST(B35_J44EmptyDeathDistributionOnTopOfEmptyRegular)
			{
				SETUP(Dead, None, Regular, NotFinal, None, None);
				manager.SetDefaultOutfit(NPCData(actor), nullptr, NotFinal);
				manager.SetDeathOutfit(NPCData(actor), nullptr, NotFinal);
				auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
				EXPECT_NONE(replacement);
				EXPECT_WORN_FIND(original, Death, NotFinal);
			}

			TEST(B35_J47DeathDistributionOnTopOfEmptyRegular)
			{
				SETUP(Dead, None, Regular, NotFinal, Elven, None);
				manager.SetDefaultOutfit(NPCData(actor), nullptr, NotFinal);
				manager.SetDeathOutfit(NPCData(actor), gets, NotFinal);
				auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
				EXPECT_WORN(replacement, gets, Death, NotFinal);
			}

			TEST(B36_J44EmptyDeathDistributionOnTopOfRegular)
			{
				SETUP(Dead, None, Regular, NotFinal, Elven, None);
				manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
				manager.SetDeathOutfit(NPCData(actor), nullptr, NotFinal);
				auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
				EXPECT_WORN(replacement, gets, Death, NotFinal);
			}

			TEST(B36_J47DeathDistributionOnTopOfRegular)
			{
				SETUP(Dead, None, Regular, NotFinal, Elven, Hunter);
				manager.SetDefaultOutfit(NPCData(actor), gets, NotFinal);
				manager.SetDeathOutfit(NPCData(actor), links, NotFinal);
				auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
				EXPECT_WORN(replacement, links, Death, NotFinal);
			}

			TEST(B37_J44EmptyDeathDistributionOnTopOfFinalRegular)
			{
				SETUP(Dead, None, Regular, NotFinal, Elven, None);
				manager.SetDefaultOutfit(NPCData(actor), gets, Final);
				manager.SetDeathOutfit(NPCData(actor), nullptr, NotFinal);
				auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
				EXPECT_WORN(replacement, gets, Death, Final);
			}

			TEST(B37_J51DeathDistributionOnTopOfFinalRegular)
			{
				SETUP(Dead, None, Regular, NotFinal, Elven, Hunter);
				manager.SetDefaultOutfit(NPCData(actor), gets, Final);
				manager.SetDeathOutfit(NPCData(actor), links, NotFinal);
				auto* replacement = TestsHelper::ResolveAndApplyWornOutfit(manager, actor, false);
				EXPECT_WORN(replacement, links, Death, NotFinal);
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
