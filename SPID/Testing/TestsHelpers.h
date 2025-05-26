#pragma once
#include "DeathDistribution.h"
#include "FormData.h"
#include "Testing.h"

namespace DeathDistribution
{
	struct TestingHelper
	{
		static Forms::Distributables<RE::SpellItem>&      GetSpells() { return Manager::GetSingleton()->spells; }
		static Forms::Distributables<RE::BGSPerk>&        GetPerks() { return Manager::GetSingleton()->perks; }
		static Forms::Distributables<RE::TESBoundObject>& GetItems() { return Manager::GetSingleton()->items; }
		static Forms::Distributables<RE::TESShout>&       GetShouts() { return Manager::GetSingleton()->shouts; }
		static Forms::Distributables<RE::TESLevSpell>&    GetLevSpells() { return Manager::GetSingleton()->levSpells; }
		static Forms::Distributables<RE::TESForm>&        GetPackages() { return Manager::GetSingleton()->packages; }
		static Forms::Distributables<RE::BGSOutfit>&      GetOutfits() { return Manager::GetSingleton()->outfits; }
		static Forms::Distributables<RE::BGSKeyword>&     GetKeywords() { return Manager::GetSingleton()->keywords; }
		static Forms::Distributables<RE::TESFaction>&     GetFactions() { return Manager::GetSingleton()->factions; }
		static Forms::Distributables<RE::BGSOutfit>&      GetSleepOutfits() { return Manager::GetSingleton()->sleepOutfits; }
		static Forms::Distributables<RE::TESObjectARMO>&  GetSkins() { return Manager::GetSingleton()->skins; }
	};
};

namespace Testing::Helper
{
	namespace Actor
	{
		inline RE::Actor* GetActor()
		{
			return GetForm<RE::Actor>(0x198B0);
		}

		inline RE::Actor* GetAlive()
		{
			auto actor = GetActor();
			actor->Resurrect(true, true);
			return actor;
		}

		inline RE::Actor* GetDead()
		{
			auto actor = GetActor();
			actor->KillImmediate();
			return actor;
		}
	};

	namespace Distribution
	{
		inline void Distribute(RE::Actor* actor = Actor::GetActor())
		{
			Distribute::detail::distribute_on_load(actor, actor->GetActorBase());
		}

		/// <summary>
		/// Reverts distribution for a given actor.
		/// For items it assumes that distributed entries had fixed count and always removes count.min number of items.
		/// </summary>
		/// <param name="actor"></param>
		inline void Revert(RE::Actor* actor = Actor::GetActor())
		{
			for (auto& item : Forms::items.GetForms()) {
				actor->GetActorBase()->RemoveObjectFromContainer(item.form, std::get<RandomCount>(item.idxOrCount).min);
			}
			actor->GetActorBase()->RemoveKeyword(Distribute::processed);  // clean up after distribute_on_load
		}

		/// <summary>
		/// Holder to stash loaded distribution configurations during testing and restore them afterwards.
		/// </summary>
		struct Holder
		{
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

		namespace Death
		{
			inline void Distribute(RE::Actor* actor, bool isDying)
			{
				auto data = NPCData(actor, actor->GetActorBase(), isDying);
				DeathDistribution::Manager::GetSingleton()->Distribute(data);
			}

			inline Forms::Distributables<RE::SpellItem>&      GetSpells() { return DeathDistribution::TestingHelper::GetSpells(); }
			inline Forms::Distributables<RE::BGSPerk>&        GetPerks() { return DeathDistribution::TestingHelper::GetPerks(); }
			inline Forms::Distributables<RE::TESBoundObject>& GetItems() { return DeathDistribution::TestingHelper::GetItems(); }
			inline Forms::Distributables<RE::TESShout>&       GetShouts() { return DeathDistribution::TestingHelper::GetShouts(); }
			inline Forms::Distributables<RE::TESLevSpell>&    GetLevSpells() { return DeathDistribution::TestingHelper::GetLevSpells(); }
			inline Forms::Distributables<RE::TESForm>&        GetPackages() { return DeathDistribution::TestingHelper::GetPackages(); }
			inline Forms::Distributables<RE::BGSOutfit>&      GetOutfits() { return DeathDistribution::TestingHelper::GetOutfits(); }
			inline Forms::Distributables<RE::BGSKeyword>&     GetKeywords() { return DeathDistribution::TestingHelper::GetKeywords(); }
			inline Forms::Distributables<RE::TESFaction>&     GetFactions() { return DeathDistribution::TestingHelper::GetFactions(); }
			inline Forms::Distributables<RE::BGSOutfit>&      GetSleepOutfits() { return DeathDistribution::TestingHelper::GetSleepOutfits(); }
			inline Forms::Distributables<RE::TESObjectARMO>&  GetSkins() { return DeathDistribution::TestingHelper::GetSkins(); }

			inline Holder configHolder;

			inline void ClearConfigs()
			{
				GetSpells().GetForms().clear();
				GetPerks().GetForms().clear();
				GetItems().GetForms().clear();
				GetShouts().GetForms().clear();
				GetLevSpells().GetForms().clear();
				GetPackages().GetForms().clear();
				GetOutfits().GetForms().clear();
				GetKeywords().GetForms().clear();
				GetFactions().GetForms().clear();
				GetSleepOutfits().GetForms().clear();
				GetSkins().GetForms().clear();
			}

			inline void SnapshotConfigs()
			{
				configHolder.spells = GetSpells();
				configHolder.perks = GetPerks();
				configHolder.items = GetItems();
				configHolder.shouts = GetShouts();
				configHolder.levSpells = GetLevSpells();
				configHolder.packages = GetPackages();
				configHolder.outfits = GetOutfits();
				configHolder.keywords = GetKeywords();
				configHolder.factions = GetFactions();
				configHolder.sleepOutfits = GetSleepOutfits();
				configHolder.skins = GetSkins();
			}

			inline void RestoreConfigs()
			{
				GetSpells() = configHolder.spells;
				GetPerks() = configHolder.perks;
				GetItems() = configHolder.items;
				GetShouts() = configHolder.shouts;
				GetLevSpells() = configHolder.levSpells;
				GetPackages() = configHolder.packages;
				GetOutfits() = configHolder.outfits;
				GetKeywords() = configHolder.keywords;
				GetFactions() = configHolder.factions;
				GetSleepOutfits() = configHolder.sleepOutfits;
				GetSkins() = configHolder.skins;
			}

		}

				inline Forms::Distributables<RE::SpellItem>&      GetSpells() { return Forms::spells; }
		inline Forms::Distributables<RE::BGSPerk>&        GetPerks() { return Forms::perks; }
		inline Forms::Distributables<RE::TESBoundObject>& GetItems() { return Forms::items; }
		inline Forms::Distributables<RE::TESShout>&       GetShouts() { return Forms::shouts; }
		inline Forms::Distributables<RE::TESLevSpell>&    GetLevSpells() { return Forms::levSpells; }
		inline Forms::Distributables<RE::TESForm>&        GetPackages() { return Forms::packages; }
		inline Forms::Distributables<RE::BGSOutfit>&      GetOutfits() { return Forms::outfits; }
		inline Forms::Distributables<RE::BGSKeyword>&     GetKeywords() { return Forms::keywords; }
		inline Forms::Distributables<RE::TESFaction>&     GetFactions() { return Forms::factions; }
		inline Forms::Distributables<RE::BGSOutfit>&      GetSleepOutfits() { return Forms::sleepOutfits; }
		inline Forms::Distributables<RE::TESObjectARMO>&  GetSkins() { return Forms::skins; }

		

		inline Holder configHolder;

		inline void ClearConfigs()
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

			Death::ClearConfigs();
		}

		inline void SnapshotConfigs()
		{
			configHolder.spells = Forms::spells;
			configHolder.perks = Forms::perks;
			configHolder.items = Forms::items;
			configHolder.shouts = Forms::shouts;
			configHolder.levSpells = Forms::levSpells;
			configHolder.packages = Forms::packages;
			configHolder.outfits = Forms::outfits;
			configHolder.keywords = Forms::keywords;
			configHolder.factions = Forms::factions;
			configHolder.sleepOutfits = Forms::sleepOutfits;
			configHolder.skins = Forms::skins;

			Death::SnapshotConfigs();
		}

		inline void RestoreConfigs()
		{
			Forms::spells = configHolder.spells;
			Forms::perks = configHolder.perks;
			Forms::items = configHolder.items;
			Forms::shouts = configHolder.shouts;
			Forms::levSpells = configHolder.levSpells;
			Forms::packages = configHolder.packages;
			Forms::outfits = configHolder.outfits;
			Forms::keywords = configHolder.keywords;
			Forms::factions = configHolder.factions;
			Forms::sleepOutfits = configHolder.sleepOutfits;
			Forms::skins = configHolder.skins;

			Death::RestoreConfigs();
		}
	}

	namespace Data
	{
		// TODO: Fill all kinds of distributable objects for easy access

		inline RE::TESBoundObject* GetItem()
		{
			return GetForm<RE::TESBoundObject>(0x139B8);  // Daedric mace
		}

		inline RE::TESBoundObject* GetBook()
		{
			return GetForm<RE::TESBoundObject>(0x1B024);  // Book
		}

		inline RE::TESLevItem* GetLeveledItem()
		{
			return GetForm<RE::TESLevItem>(0x14);  // Best Daedric Weapons
		}
	}

	namespace Outfits
	{
		// TODO: Outfit helper methods
	}

	namespace Inventory
	{
		int GetItemCount(RE::Actor* actor, RE::TESBoundObject* item)
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
	}
}
