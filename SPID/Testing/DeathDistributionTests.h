#pragma once
#include "DeathDistribution.h"
#include "FormData.h"
#include "Testing.h"
#include <stdlib.h>

namespace DeathDistribution
{
	using namespace Testing;

	struct TestsHelper : ISingleton<TestsHelper>
	{
		static RE::Actor* GetAlive()
		{
			auto actor = GetForm<RE::Actor>(0x198B0);
			actor->Resurrect(true, true);
			return actor;
		}

		static RE::Actor* GetDead()
		{
			auto actor = GetForm<RE::Actor>(0x198B0);
			actor->KillImmediate();
			return actor;
		}

		static RE::TESBoundObject* GetItem()
		{
			return GetForm<RE::TESBoundObject>(0x139B8);  // Daedric mace
		}

		static RE::TESBoundObject* GetBook()
		{
			return GetForm<RE::TESBoundObject>(0x1B024);  // Book
		}

		static RE::TESLevItem* GetLeveledItem()
		{
			return GetForm<RE::TESLevItem>(0x14);  // Best Daedric Weapons
		}

		static void Distribute(RE::Actor* actor, bool isDying)
		{
			auto data = NPCData(actor, actor->GetActorBase(), isDying);
			DeathDistribution::Manager::GetSingleton()->Distribute(data);
		}

		static Distributables<RE::TESBoundObject>& GetItems()
		{
			return DeathDistribution::Manager::GetSingleton()->items;
		}

		static void RemoveAddedItems(RE::Actor* actor = GetAlive())
		{
			for (auto& item : GetItems().GetForms()) {
				actor->GetActorBase()->RemoveObjectFromContainer(item.form, std::get<RandomCount>(item.idxOrCount).min);
			}
		}

		static int GetItemCount(RE::Actor* actor, RE::TESBoundObject* item)
		{
			int itemsCount = actor->GetActorBase()->CountObjectsInContainer(item);
			if (const auto invChanges = actor->GetInventoryChanges()) {
				if (const auto entryLists = invChanges->entryList) {
					for (const auto& entryList : *entryLists) {
						if (entryList && entryList->object && entryList->object->formID == item->formID) {
							itemsCount += entryList->countDelta;
						}
					}
				}
			}
			return itemsCount;
		}

		static void ClearConfigs()
		{
			DeathDistribution::Manager::GetSingleton()->spells.GetForms().clear();
			DeathDistribution::Manager::GetSingleton()->perks.GetForms().clear();
			DeathDistribution::Manager::GetSingleton()->items.GetForms().clear();
			DeathDistribution::Manager::GetSingleton()->shouts.GetForms().clear();
			DeathDistribution::Manager::GetSingleton()->levSpells.GetForms().clear();
			DeathDistribution::Manager::GetSingleton()->packages.GetForms().clear();
			DeathDistribution::Manager::GetSingleton()->outfits.GetForms().clear();
			DeathDistribution::Manager::GetSingleton()->keywords.GetForms().clear();
			DeathDistribution::Manager::GetSingleton()->factions.GetForms().clear();
			DeathDistribution::Manager::GetSingleton()->sleepOutfits.GetForms().clear();
			DeathDistribution::Manager::GetSingleton()->skins.GetForms().clear();
		}

		static void SnapshotConfigs()
		{
			GetSingleton()->spells = DeathDistribution::Manager::GetSingleton()->spells;
			GetSingleton()->perks = DeathDistribution::Manager::GetSingleton()->perks;
			GetSingleton()->items = DeathDistribution::Manager::GetSingleton()->items;
			GetSingleton()->shouts = DeathDistribution::Manager::GetSingleton()->shouts;
			GetSingleton()->levSpells = DeathDistribution::Manager::GetSingleton()->levSpells;
			GetSingleton()->packages = DeathDistribution::Manager::GetSingleton()->packages;
			GetSingleton()->outfits = DeathDistribution::Manager::GetSingleton()->outfits;
			GetSingleton()->keywords = DeathDistribution::Manager::GetSingleton()->keywords;
			GetSingleton()->factions = DeathDistribution::Manager::GetSingleton()->factions;
			GetSingleton()->sleepOutfits = DeathDistribution::Manager::GetSingleton()->sleepOutfits;
			GetSingleton()->skins = DeathDistribution::Manager::GetSingleton()->skins;
		}

		static void RestoreConfigs()
		{
			DeathDistribution::Manager::GetSingleton()->spells = GetSingleton()->spells;
			DeathDistribution::Manager::GetSingleton()->perks = GetSingleton()->perks;
			DeathDistribution::Manager::GetSingleton()->items = GetSingleton()->items;
			DeathDistribution::Manager::GetSingleton()->shouts = GetSingleton()->shouts;
			DeathDistribution::Manager::GetSingleton()->levSpells = GetSingleton()->levSpells;
			DeathDistribution::Manager::GetSingleton()->packages = GetSingleton()->packages;
			DeathDistribution::Manager::GetSingleton()->outfits = GetSingleton()->outfits;
			DeathDistribution::Manager::GetSingleton()->keywords = GetSingleton()->keywords;
			DeathDistribution::Manager::GetSingleton()->factions = GetSingleton()->factions;
			DeathDistribution::Manager::GetSingleton()->sleepOutfits = GetSingleton()->sleepOutfits;
			DeathDistribution::Manager::GetSingleton()->skins = GetSingleton()->skins;
		}

	private:
		Forms::Distributables<RE::SpellItem>      spells{ RECORD::kSpell };
		Forms::Distributables<RE::BGSPerk>        perks{ RECORD::kPerk };
		Forms::Distributables<RE::TESBoundObject> items{ RECORD::kItem };
		Forms::Distributables<RE::TESShout>       shouts{ RECORD::kShout };
		Forms::Distributables<RE::TESLevSpell>    levSpells{ RECORD::kLevSpell };
		Forms::Distributables<RE::TESForm>        packages{ RECORD::kPackage };
		Forms::Distributables<RE::BGSOutfit>      outfits{ RECORD::kOutfit };
		Forms::Distributables<RE::BGSKeyword>     keywords{ RECORD::kKeyword };
		Forms::Distributables<RE::TESFaction>     factions{ RECORD::kFaction };
		Forms::Distributables<RE::BGSOutfit>      sleepOutfits{ RECORD::kSleepOutfit };
		Forms::Distributables<RE::TESObjectARMO>  skins{ RECORD::kSkin };
	};

	namespace Testing
	{
		namespace Dead
		{
			constexpr static const char* moduleName = "DeathDistribution.Dead";

			BEFORE_ALL
			{
				TestsHelper::SnapshotConfigs();
			}

			AFTER_ALL
			{
				TestsHelper::RestoreConfigs();
			}

			BEFORE_EACH
			{
				TestsHelper::ClearConfigs();
			}

			AFTER_EACH
			{
				TestsHelper::RemoveAddedItems();
				TestsHelper::GetAlive()->GetActorBase()->RemoveKeyword(Distribute::processed);  // clean up after distribute_on_load
			}

			TEST(AddItemToDeadActor)
			{
				auto        actor{ TestsHelper::GetDead() };
				auto        item{ TestsHelper::GetItem() };
				FilterData  filterData{ {}, {}, {}, {}, 100 };
				RandomCount idxOrCount{ 2, 2 };
				bool        isFinal{ false };
				Path        path{ "" };
				TestsHelper::GetItems().EmplaceForm(true, item, isFinal, idxOrCount, filterData, path);
		
				TestsHelper::Distribute(actor, false);
						
				auto got = TestsHelper::GetItemCount(actor, item);
				EXPECT(got == idxOrCount.min, fmt::format("Expected actor to have {} items, but they have {}", idxOrCount.min, got));
			}

			TEST(AddItemToDeadActorAfterRegularDistribution)
			{
				auto        actor{ TestsHelper::GetDead() };
				auto        item{ TestsHelper::GetItem() };
				FilterData  filterData{ {}, {}, {}, {}, 100 };
				RandomCount idxOrCount{ 2, 2 };
				bool        isFinal{ false };
				Path        path{ "" };
				TestsHelper::GetItems().EmplaceForm(true, item, isFinal, idxOrCount, filterData, path);

				Distribute::detail::distribute_on_load(actor, actor->GetActorBase());
				TestsHelper::Distribute(actor, false);

				auto got = TestsHelper::GetItemCount(actor, item);
				EXPECT(got == idxOrCount.min, fmt::format("Expected actor to have {} items, but they have {}", idxOrCount.min, got));
			}
		}

		namespace Dying
		{
			constexpr static const char* moduleName = "DeathDistribution.Dying";

			BEFORE_ALL
			{
				TestsHelper::SnapshotConfigs();
			}

			AFTER_ALL
			{
				TestsHelper::RestoreConfigs();
			}

			BEFORE_EACH
			{
				TestsHelper::ClearConfigs();
			}

			AFTER_EACH
			{
				TestsHelper::RemoveAddedItems();
				TestsHelper::GetAlive()->GetActorBase()->RemoveKeyword(Distribute::processed);  // clean up after distribute_on_load
			}

			TEST(AddItemToDyingActor)
			{
				auto        actor{ TestsHelper::GetAlive() };
				auto        item{ TestsHelper::GetItem() };
				FilterData  filterData{ {}, {}, {}, {}, 100 };
				RandomCount idxOrCount{ 2, 2 };
				bool        isFinal{ false };
				Path        path{ "" };
				TestsHelper::GetItems().EmplaceForm(true, item, isFinal, idxOrCount, filterData, path);

				TestsHelper::Distribute(actor, true);

				auto got = TestsHelper::GetItemCount(actor, item);
				EXPECT(got == idxOrCount.min, fmt::format("Expected actor to have {} items, but they have {}", idxOrCount.min, got));
			}

			TEST(AddItemToDyingActorAfterRegularDistribution)
			{
				auto        actor{ TestsHelper::GetAlive() };
				auto        item{ TestsHelper::GetItem() };
				FilterData  filterData{ {}, {}, {}, {}, 100 };
				RandomCount idxOrCount{ 2, 2 };
				bool        isFinal{ false };
				Path        path{ "" };
				TestsHelper::GetItems().EmplaceForm(true, item, isFinal, idxOrCount, filterData, path);

				Distribute::detail::distribute_on_load(actor, actor->GetActorBase());
				TestsHelper::Distribute(actor, true);

				auto got = TestsHelper::GetItemCount(actor, item);
				EXPECT(got == idxOrCount.min, fmt::format("Expected actor to have {} items, but they have {}", idxOrCount.min, got));
			}
		}
	}
}
