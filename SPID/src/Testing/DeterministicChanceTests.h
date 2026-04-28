#pragma once
#include "LookupConfigs.h"
#include "LookupFilters.h"
#include "LookupNPC.h"
#include "Testing.h"
#include "TestsHelpers.h"

namespace DeterministicChance
{
	using namespace Testing;

	namespace Testing
	{
		namespace Parsing
		{
			constexpr static const char* moduleName = "DeterministicChance.Parsing";

			struct ChanceData
			{
				Chance chance{};
			};

			TEST(ParsesPercentChanceAsDecimal)
			{
				ChanceData data;
				Distribution::INI::ChanceComponentParser{}("50", data);
				EXPECT(data.chance.value == 0.5, fmt::format("Expected value 0.5 but got {}", data.chance.value));
			}

			TEST(DeterministicSuffixSetsFlag)
			{
				ChanceData data;
				Distribution::INI::ChanceComponentParser{}("50!", data);
				EXPECT(data.chance.deterministic, "Expected '50!' to set deterministic flag");
			}

			TEST(NoSuffixLeavesFlagFalse)
			{
				ChanceData data;
				Distribution::INI::ChanceComponentParser{}("50", data);
				EXPECT(!data.chance.deterministic, "Expected '50' to leave deterministic flag false");
			}

			TEST(DeterministicEntryParsesValueCorrectly)
			{
				ChanceData data;
				Distribution::INI::ChanceComponentParser{}("75!", data);
				EXPECT(data.chance.value == 0.75, fmt::format("Expected value 0.75 but got {}", data.chance.value));
			}

			TEST(ParsesFullChance)
			{
				ChanceData data;
				Distribution::INI::ChanceComponentParser{}("100", data);
				EXPECT(data.chance.value == 1.0, fmt::format("Expected value 1.0 but got {}", data.chance.value));
			}

			TEST(ParsesZeroChance)
			{
				ChanceData data;
				Distribution::INI::ChanceComponentParser{}("0", data);
				EXPECT(data.chance.value == 0.0, fmt::format("Expected value 0.0 but got {}", data.chance.value));
			}

			TEST(ParsesDecimalPercentChance)
			{
				ChanceData data;
				Distribution::INI::ChanceComponentParser{}("50.5", data);
				EXPECT(std::abs(data.chance.value - 0.505) < 1e-9, fmt::format("Expected value 0.505 but got {}", data.chance.value));
			}

			TEST(ParsesDeterministicDecimalChance)
			{
				ChanceData data;
				Distribution::INI::ChanceComponentParser{}("50.5!", data);
				ASSERT(data.chance.deterministic, "Expected '50.5!' to set deterministic flag");
				EXPECT(std::abs(data.chance.value - 0.505) < 1e-9, fmt::format("Expected value 0.505 but got {}", data.chance.value));
			}

			TEST(ParsesDecimalNearFullChance)
			{
				ChanceData data;
				Distribution::INI::ChanceComponentParser{}("99.9!", data);
				ASSERT(data.chance.deterministic, "Expected '99.9!' to set deterministic flag");
				EXPECT(std::abs(data.chance.value - 0.999) < 1e-9, fmt::format("Expected value 0.999 but got {}", data.chance.value));
			}

			TEST(SameEntryLineProducesSameLineSeed)
			{
				const std::string key = "Spell";
				const std::string value = "0x10F7F7||||||50!";

				Distribution::INI::configs.clear();
				Distribution::INI::TryParse(key, value, "test.ini");
				Distribution::INI::TryParse(key, value, "test.ini");

				auto& entries = Distribution::INI::configs[RECORD::kSpell];
				ASSERT(entries.size() == 2, fmt::format("Expected 2 parsed entries but got {}", entries.size()));
				EXPECT(entries[0].chance.lineSeed == entries[1].chance.lineSeed, "Expected identical entry lines to produce the same lineSeed");
			}

			TEST(SlightlyDifferentEntryLineProducesDifferentLineSeed)
			{
				const std::string key = "Spell";
				const std::string value1 = "0x10F7F7||||||50!";
				const std::string value2 = "0x10F7F7||||||51!";

				Distribution::INI::configs.clear();
				Distribution::INI::TryParse(key, value1, "test.ini");
				Distribution::INI::TryParse(key, value2, "test.ini");

				auto& entries = Distribution::INI::configs[RECORD::kSpell];
				ASSERT(entries.size() == 2, fmt::format("Expected 2 parsed entries but got {}", entries.size()));
				EXPECT(entries[0].chance.lineSeed != entries[1].chance.lineSeed, "Expected entry lines differing only in chance to produce different lineSeeds");
			}
		}

		namespace Filters
		{
			constexpr static const char* moduleName = "DeterministicChance.Filters";

			TEST(SameCallGivesSameResult)
			{
				constexpr int N = 10;

				auto    actor = ::Testing::Helper::Actor::GetActor();
				NPCData npcData(actor);

				Chance deterministicChance(0.5, true);
				deterministicChance.lineSeed = std::hash<std::string>{}("Spell = 0x10F7F7||||||50!");
				::FilterData filterData{ {}, {}, {}, {}, deterministicChance };

				auto expected = filterData.PassedFilters(npcData);
				for (int i = 1; i < N; ++i) {
					auto result = filterData.PassedFilters(npcData);
					ASSERT(result == expected, fmt::format("Expected deterministic chance to give the same result on call {}/{}", i + 1, N));
				}
				PASS;
			}

			TEST(ZeroChanceAlwaysFails)
			{
				auto    actor = ::Testing::Helper::Actor::GetActor();
				NPCData npcData(actor);

				Chance zeroChance(0.0, true);
				zeroChance.lineSeed = std::hash<std::string>{}("Spell = 0x10F7F7||||||0!");
				::FilterData filterData{ {}, {}, {}, {}, zeroChance };

				auto result = filterData.PassedFilters(npcData);
				EXPECT(result == Filter::Result::kFailRNG, "Expected 0% chance to always fail RNG check");
			}

			TEST(FullChanceAlwaysPasses)
			{
				auto    actor = ::Testing::Helper::Actor::GetActor();
				NPCData npcData(actor);

				::FilterData filterData{ {}, {}, {}, {}, Chance(1.0, true) };

				auto result = filterData.PassedFilters(npcData);
				EXPECT(result == Filter::Result::kPass, "Expected 100% chance to pass all filters");
			}

			TEST(DifferentActorsHaveIndependentResults)
			{
				constexpr int N = 10;

				auto    actor1 = ::Testing::Helper::Actor::GetActor();
				auto    actor2 = ::Testing::Helper::Actor::GetAnotherActor();
				NPCData npcData1(actor1);
				NPCData npcData2(actor2);

				Chance deterministicChance(0.5, true);
				deterministicChance.lineSeed = std::hash<std::string>{}("Spell = 0x10F7F7||||||50!");
				::FilterData filterData{ {}, {}, {}, {}, deterministicChance };

				// Each actor's result must be stable across N repeated calls, even if they differ from each other
				auto expected1 = filterData.PassedFilters(npcData1);
				for (int i = 1; i < N; ++i) {
					auto result = filterData.PassedFilters(npcData1);
					ASSERT(result == expected1, fmt::format("Expected actor1 to get the same result on call {}/{}", i + 1, N));
				}

				auto expected2 = filterData.PassedFilters(npcData2);
				for (int i = 1; i < N; ++i) {
					auto result = filterData.PassedFilters(npcData2);
					ASSERT(result == expected2, fmt::format("Expected actor2 to get the same result on call {}/{}", i + 1, N));
				}

				PASS;
			}
		}
	}
}
