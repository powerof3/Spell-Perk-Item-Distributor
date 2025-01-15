#pragma once
#include "DistributeManager.h"
#include "FormData.h"
#include "Testing.h"

namespace Distribute
{
	using namespace Testing;

	struct TestsHelper : ISingleton<TestsHelper>
	{
		static RE::Actor* GetActor()
		{
			auto actor = GetForm<RE::Actor>(0x198B0);
			return actor;
		}

		static RE::TESBoundObject* GetItem()
		{
			auto item = GetForm<RE::TESBoundObject>(0x139B8);  // Daedric mace
			return item;
		}

		static void Distribute(RE::Actor* actor)
		{
			detail::distribute_on_load(actor, actor->GetActorBase());
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
			Forms::spells.GetForms().clear();
			Forms::perks.GetForms().clear();
			Forms::items.GetForms().clear();
			Forms::shouts.GetForms().clear();
			Forms::levSpells.GetForms().clear();
			Forms::packages.GetForms().clear();
			Forms::outfits.GetForms().clear();
			Forms::keywords.GetForms().clear();
			Forms::factions.GetForms().clear();
			Forms::sleepOutfits.GetForms().clear();
			Forms::skins.GetForms().clear();
		}

		static void SnapshotConfigs()
		{
			GetSingleton()->spells = Forms::spells;
			GetSingleton()->perks = Forms::perks;
			GetSingleton()->items = Forms::items;
			GetSingleton()->shouts = Forms::shouts;
			GetSingleton()->levSpells = Forms::levSpells;
			GetSingleton()->packages = Forms::packages;
			GetSingleton()->outfits = Forms::outfits;
			GetSingleton()->keywords = Forms::keywords;
			GetSingleton()->factions = Forms::factions;
			GetSingleton()->sleepOutfits = Forms::sleepOutfits;
			GetSingleton()->skins = Forms::skins;
		}

		static void RestoreConfigs()
		{
			Forms::spells = GetSingleton()->spells;
			Forms::perks = GetSingleton()->perks;
			Forms::items = GetSingleton()->items;
			Forms::shouts = GetSingleton()->shouts;
			Forms::levSpells = GetSingleton()->levSpells;
			Forms::packages = GetSingleton()->packages;
			Forms::outfits = GetSingleton()->outfits;
			Forms::keywords = GetSingleton()->keywords;
			Forms::factions = GetSingleton()->factions;
			Forms::sleepOutfits = GetSingleton()->sleepOutfits;
			Forms::skins = GetSingleton()->skins;
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
		namespace Items
		{
			constexpr static const char* moduleName = "Distribute.Items";

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

			TEST(AddItemToActor)
			{
				auto        actor{ TestsHelper::GetActor() };
				auto        item{ TestsHelper::GetItem() };
				FilterData  filterData{ {}, {}, {}, {}, 100 };
				RandomCount idxOrCount{ 1, 1 };
				bool        isFinal{ false };
				Path        path{ "" };
				// void EmplaceForm(bool isValid, Form*, const bool& isFinal, const IndexOrCount&, const FilterData&, const Path&);
				Forms::items.EmplaceForm(true, item, isFinal, idxOrCount, filterData, path);
				TestsHelper::Distribute(actor);
				auto got = TestsHelper::GetItemCount(actor, item);
				EXPECT(got == 1, fmt::format("Expected actor to have 1 item, but they have {}", got));
			}
		}
	}
}
