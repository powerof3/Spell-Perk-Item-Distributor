#pragma once
#include "FormData.h"

namespace LinkedDistribution
{
	namespace INI {

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

	template<class Form>
	using DataSet = std::set<Forms::Data<Form>>;

	template<class T>
	using LinkedForms = std::unordered_map<RE::TESForm*, DataSet<T>>;

	class Manager : public ISingleton<Manager>
	{
	private:
		template <class Form>
		const DataSet<Form>& LinkedFormsForForm(const RE::TESForm* form, const LinkedForms<Form>& linkedForms) const
		{
			if (const auto it = linkedForms.find(form); it != linkedForms.end()) {
				return it->second;
			} else {
				static std::set<RE::TESForm*> empty{};
				return empty;
			}
		}

	public:
		/// <summary>
		/// Does a forms lookup similar to what Filters do.
		///
		/// As a result this method configures Manager with discovered valid linked items.
		/// </summary>
		/// <param name="dataHandler">A DataHandler that will perform the actual lookup.</param>
		/// <param name="rawLinkedDistribution">A raw linked item entries that should be processed.</param>
		void LookupLinkedItems(RE::TESDataHandler* const dataHandler, INI::LinkedItemsVec& rawLinkedItems);



	private:

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
