#pragma once
#include "FormData.h"
#include "LookupNPC.h"
#include "PCLevelMultManager.h"

namespace LinkedDistribution
{
	namespace INI
	{

		struct RawLinkedItem
		{
			FormOrEditorID rawForm{};

			/// Raw filters in RawLinkedItem only use MATCH, there is no meaning for ALL or NOT, so they are ignored.
			Filters<FormOrEditorID> rawFormFilters{};

			RandomCount count{ 1, 1 };
			Chance      chance{ 100 };

			std::string path{};
		};

		using LinkedItemsVec = std::vector<RawLinkedItem>;

		inline LinkedItemsVec linkedItems{};

		namespace Parser
		{
			bool TryParse(const std::string& a_key, const std::string& a_value, const std::string& a_path);
		}
	}

	using namespace Forms;

	template<class T>
	using LinkedForms = std::unordered_map<RE::TESForm*, DataVec<T>>;

	class Manager : public ISingleton<Manager>
	{

	public:
		/// <summary>
		/// Does a forms lookup similar to what Filters do.
		///
		/// As a result this method configures Manager with discovered valid linked items.
		/// </summary>
		/// <param name="dataHandler">A DataHandler that will perform the actual lookup.</param>
		/// <param name="rawLinkedDistribution">A raw linked item entries that should be processed.</param>
		void LookupLinkedItems(RE::TESDataHandler* const dataHandler, INI::LinkedItemsVec& rawLinkedItems);


		/// <summary>
		/// Calculates DistributionSet for each linked form and calls a callback for each of them.
		/// </summary>
		/// <param name="linkedForms">A set of forms for which distribution sets should be calculated.
		///							  This is typically distributed forms accumulated during first distribution pass.</param>
		/// <param name="callback">A callback to be called with each DistributionSet. This is supposed to do the actual distribution.</param>
		void ForEachLinkedDistributionSet(const std::set<RE::TESForm*>& linkedForms, std::function<void(DistributionSet&)> callback);

	private:

		template <class Form>
		DataVec<Form>& LinkedFormsForForm(RE::TESForm* form, LinkedForms<Form>& linkedForms) const
		{
			if (auto it = linkedForms.find(form); it != linkedForms.end()) {
				return it->second;
			} else {
				static DataVec<Form> empty{};
				return empty;
			}
		}

		LinkedForms<RE::SpellItem>      spells{ RECORD::kSpell };
		LinkedForms<RE::BGSPerk>        perks{ RECORD::kPerk };
		LinkedForms<RE::TESBoundObject> items{ RECORD::kItem };
		LinkedForms<RE::TESShout>       shouts{ RECORD::kShout };
		LinkedForms<RE::TESLevSpell>    levSpells{ RECORD::kLevSpell };
		LinkedForms<RE::TESForm>        packages{ RECORD::kPackage };
		LinkedForms<RE::BGSOutfit>      outfits{ RECORD::kOutfit };
		LinkedForms<RE::BGSKeyword>     keywords{ RECORD::kKeyword };
		LinkedForms<RE::TESBoundObject> deathItems{ RECORD::kDeathItem };
		LinkedForms<RE::TESFaction>     factions{ RECORD::kFaction };
		LinkedForms<RE::BGSOutfit>      sleepOutfits{ RECORD::kSleepOutfit };
		LinkedForms<RE::TESObjectARMO>  skins{ RECORD::kSkin };
	};
}
