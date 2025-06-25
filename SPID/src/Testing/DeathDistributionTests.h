#pragma once
#include "DeathDistribution.h"
#include "FormData.h"
#include "Testing.h"
#include "TestsHelpers.h"

namespace DeathDistribution
{
	using namespace Testing;

	namespace Testing
	{
		namespace Dead
		{
			constexpr static const char* moduleName = "DeathDistribution.Dead";

			BEFORE_ALL
			{
				::Testing::Helper::Distribution::SnapshotConfigs();
			}

			AFTER_ALL
			{
				::Testing::Helper::Distribution::RestoreConfigs();
			}

			BEFORE_EACH
			{
				::Testing::Helper::Distribution::ClearConfigs();
			}

			AFTER_EACH
			{
				::Testing::Helper::Distribution::Revert();
			}

			TEST(AddItemToDeadActor)
			{
				auto        actor{ ::Testing::Helper::Actor::GetDead() };
				auto        item{ ::Testing::Helper::Data::GetItem() };
				FilterData  filterData{ {}, {}, {}, {}, 100 };
				RandomCount idxOrCount{ 2, 2 };
				bool        isFinal{ false };
				Path        path{ "" };
				::Testing::Helper::Distribution::Death::GetItems().EmplaceForm(true, item, isFinal, idxOrCount, filterData, path);

				::Testing::Helper::Distribution::Death::Distribute(actor, false);

				auto got = ::Testing::Helper::Inventory::GetItemCount(actor, item);
				EXPECT(got == idxOrCount.min, fmt::format("Expected actor to have {} items, but they have {}", idxOrCount.min, got));
			}

			TEST(AddItemToDeadActorAfterRegularDistribution)
			{
				auto        actor{ ::Testing::Helper::Actor::GetDead() };
				auto        item{ ::Testing::Helper::Data::GetItem() };
				FilterData  filterData{ {}, {}, {}, {}, 100 };
				RandomCount idxOrCount{ 2, 2 };
				bool        isFinal{ false };
				Path        path{ "" };
				::Testing::Helper::Distribution::Death::GetItems().EmplaceForm(true, item, isFinal, idxOrCount, filterData, path);

				::Testing::Helper::Distribution::Distribute(actor);
				::Testing::Helper::Distribution::Death::Distribute(actor, false);

				auto got = ::Testing::Helper::Inventory::GetItemCount(actor, item);
				EXPECT(got == idxOrCount.min, fmt::format("Expected actor to have {} items, but they have {}", idxOrCount.min, got));
			}
		}

		namespace Dying
		{
			constexpr static const char* moduleName = "DeathDistribution.Dying";

			BEFORE_ALL
			{
				::Testing::Helper::Distribution::SnapshotConfigs();
			}

			AFTER_ALL
			{
				::Testing::Helper::Distribution::RestoreConfigs();
			}

			BEFORE_EACH
			{
				::Testing::Helper::Distribution::ClearConfigs();
			}

			AFTER_EACH
			{
				::Testing::Helper::Distribution::Revert();
			}

			TEST(AddItemToDyingActor)
			{
				auto        actor{ ::Testing::Helper::Actor::GetAlive() };
				auto        item{ ::Testing::Helper::Data::GetItem() };
				FilterData  filterData{ {}, {}, {}, {}, 100 };
				RandomCount idxOrCount{ 2, 2 };
				bool        isFinal{ false };
				Path        path{ "" };
				::Testing::Helper::Distribution::Death::GetItems().EmplaceForm(true, item, isFinal, idxOrCount, filterData, path);

				::Testing::Helper::Distribution::Death::Distribute(actor, true);

				auto got = ::Testing::Helper::Inventory::GetItemCount(actor, item);
				EXPECT(got == idxOrCount.min, fmt::format("Expected actor to have {} items, but they have {}", idxOrCount.min, got));
			}

			TEST(AddItemToDyingActorAfterRegularDistribution)
			{
				auto        actor{ ::Testing::Helper::Actor::GetAlive() };
				auto        item{ ::Testing::Helper::Data::GetItem() };
				FilterData  filterData{ {}, {}, {}, {}, 100 };
				RandomCount idxOrCount{ 2, 2 };
				bool        isFinal{ false };
				Path        path{ "" };
				::Testing::Helper::Distribution::Death::GetItems().EmplaceForm(true, item, isFinal, idxOrCount, filterData, path);

				::Testing::Helper::Distribution::Distribute(actor);
				::Testing::Helper::Distribution::Death::Distribute(actor, true);

				auto got = ::Testing::Helper::Inventory::GetItemCount(actor, item);
				EXPECT(got == idxOrCount.min, fmt::format("Expected actor to have {} items, but they have {}", idxOrCount.min, got));
			}
		}
	}
}
