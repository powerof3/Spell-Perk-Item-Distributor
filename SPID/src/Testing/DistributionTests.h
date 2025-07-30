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

				::Testing::Helper::Distribution::GetItems().EmplaceForm(true, item, isFinal, idxOrCount, filterData, path);

				::Testing::Helper::Distribution::Distribute(actor);
				auto got = ::Testing::Helper::Inventory::GetItemCount(actor, item);
				EXPECT(got == 1, fmt::format("Expected actor to have 1 item, but they have {}", got));
			}
		}

		namespace Packages
		{
			constexpr static const char* moduleName = "Distribute.Packages";

			inline RE::BSSimpleList<RE::TESPackage*> originalPackages;

			BEFORE_ALL
			{
				::Testing::Helper::Distribution::SnapshotConfigs();
			}

			AFTER_ALL
			{
				::Testing::Helper::Distribution::RestoreConfigs();
				originalPackages = {};
			}

			BEFORE_EACH
			{
				originalPackages = ::Testing::Helper::Actor::GetActor()->GetActorBase()->aiPackages.packages;
				::Testing::Helper::Distribution::ClearConfigs();
			}

			AFTER_EACH
			{
				::Testing::Helper::Actor::GetActor()->GetActorBase()->aiPackages.packages = originalPackages;
				::Testing::Helper::Distribution::Revert();
			}

			TEST(InsertPackagesAt0)
			{
				auto            actor{ ::Testing::Helper::Actor::GetActor() };
				RE::TESPackage* package{ ::Testing::Helper::Package::GetPackage() };
				FilterData      filterData{ {}, {}, {}, {}, 100 };
				Index           idxOrCount{ 0 };
				bool            isFinal{ false };
				Path            path{ "" };

				::Testing::Helper::Distribution::GetPackages().EmplaceForm(true, package, isFinal, idxOrCount, filterData, path);
				::Testing::Helper::Distribution::Distribute(actor);

				auto newPackages = actor->GetActorBase()->aiPackages.packages;
				auto oldVec = ::Testing::Helper::ToVector(originalPackages);
				oldVec.insert(oldVec.begin() + idxOrCount, package);
				auto newVec = ::Testing::Helper::ToVector(newPackages);
				EXPECT(oldVec == newVec, "Expected package to be inserted at the front");
			}

			TEST(InsertPackageInTheMiddle)
			{
				auto            actor{ ::Testing::Helper::Actor::GetActor() };
				RE::TESPackage* package{ ::Testing::Helper::Package::GetPackage() };
				FilterData      filterData{ {}, {}, {}, {}, 100 };
				Index           idxOrCount{ 2 };
				bool            isFinal{ false };
				Path            path{ "" };

				auto oldPackages = actor->GetActorBase()->aiPackages.packages;
				::Testing::Helper::Distribution::GetPackages().EmplaceForm(true, package, isFinal, idxOrCount, filterData, path);
				::Testing::Helper::Distribution::Distribute(actor);

				auto newPackages = actor->GetActorBase()->aiPackages.packages;
				auto oldVec = ::Testing::Helper::ToVector(originalPackages);
				oldVec.insert(oldVec.begin() + idxOrCount, package);
				auto newVec = ::Testing::Helper::ToVector(newPackages);
				EXPECT(oldVec == newVec, fmt::format("Expected package to be inserted at index {}", idxOrCount));
			}

			TEST(AppendPackageWhenIndexExceedsSize)
			{
				auto            actor{ ::Testing::Helper::Actor::GetActor() };
				RE::TESPackage* package{ ::Testing::Helper::Package::GetPackage() };
				FilterData      filterData{ {}, {}, {}, {}, 100 };
				Index           idxOrCount{ 100 };
				bool            isFinal{ false };
				Path            path{ "" };

				::Testing::Helper::Distribution::GetPackages().EmplaceForm(true, package, isFinal, idxOrCount, filterData, path);
				::Testing::Helper::Distribution::Distribute(actor);

				auto newPackages = actor->GetActorBase()->aiPackages.packages;
				auto oldVec = ::Testing::Helper::ToVector(originalPackages);
				oldVec.push_back(package);
				auto newVec = ::Testing::Helper::ToVector(newPackages);
				EXPECT(oldVec == newVec, "Expected package to be appended to the back");
			}

			TEST(PlacePackageAtTheFrontWhenListIsEmpty)
			{
				auto            actor{ ::Testing::Helper::Actor::GetActor() };
				RE::TESPackage* package{ ::Testing::Helper::Package::GetPackage() };
				FilterData      filterData{ {}, {}, {}, {}, 100 };
				Index           idxOrCount{ 1 };
				bool            isFinal{ false };
				Path            path{ "" };

				actor->GetActorBase()->aiPackages.packages.clear();
				::Testing::Helper::Distribution::GetPackages().EmplaceForm(true, package, isFinal, idxOrCount, filterData, path);
				::Testing::Helper::Distribution::Distribute(actor);

				auto newPackages = actor->GetActorBase()->aiPackages.packages;
				std::vector<RE::TESPackage*> oldVec{ package };
				auto                         newVec = ::Testing::Helper::ToVector(newPackages);
				EXPECT(oldVec == newVec, "Expected package to be inserted at the front");
			}

			TEST(SkipPackageIfItIsAlreadyInTheList)
			{
				auto            actor{ ::Testing::Helper::Actor::GetActor() };
				RE::TESPackage* package{ actor->GetActorBase()->aiPackages.packages.front() };
				FilterData      filterData{ {}, {}, {}, {}, 100 };
				Index           idxOrCount{ 1 };
				bool            isFinal{ false };
				Path            path{ "" };


				::Testing::Helper::Distribution::GetPackages().EmplaceForm(true, package, isFinal, idxOrCount, filterData, path);
				::Testing::Helper::Distribution::Distribute(actor);

				auto newPackages = actor->GetActorBase()->aiPackages.packages;
				auto oldVec = ::Testing::Helper::ToVector(originalPackages);
				auto newVec = ::Testing::Helper::ToVector(newPackages);
				EXPECT(oldVec == newVec, "Expected package to be inserted at the front");
			}
		}
	}
}
