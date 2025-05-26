#pragma once
#include "DistributeManager.h"
#include "FormData.h"
#include "Testing.h"
#include "TestsHelpers.h"

namespace Distribute
{
	using namespace Testing;

	namespace Testing
	{
		namespace Items
		{
			constexpr static const char* moduleName = "Distribute.Items";

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

			TEST(AddItemToActor)
			{
				auto        actor{ ::Testing::Helper::Actor::GetActor() };
				auto        item{ ::Testing::Helper::Data::GetItem() };
				FilterData  filterData{ {}, {}, {}, {}, 100 };
				RandomCount idxOrCount{ 1, 1 };
				bool        isFinal{ false };
				Path        path{ "" };
				// void EmplaceForm(bool isValid, Form*, const bool& isFinal, const IndexOrCount&, const FilterData&, const Path&);
				Forms::items.EmplaceForm(true, item, isFinal, idxOrCount, filterData, path);

				::Testing::Helper::Distribution::Distribute(actor);
				auto got = ::Testing::Helper::Inventory::GetItemCount(actor, item);
				EXPECT(got == 1, fmt::format("Expected actor to have 1 item, but they have {}", got));
			}
		}
	}
}
