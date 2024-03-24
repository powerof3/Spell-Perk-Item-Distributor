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

		bool TryParse(const std::string& a_key, const std::string& a_value, const std::string& a_path);
	}

	using namespace Forms;

	class Manager;

	template <class Form>
	struct LinkedForms
	{
		friend Manager; // allow Manager to later modify forms directly.

		using Map = std::unordered_map<RE::TESForm*, DataVec<Form>>;

		LinkedForms(RECORD::TYPE type) :
			type(type)
		{}

		RECORD::TYPE GetType() const { return type; }
		const Map&   GetForms() const { return forms; }

	private:
		RECORD::TYPE type;
		Map          forms{};		

		void Link(Form* form, const FormVec& linkedForms, const RandomCount& count, const Chance& chance, const std::string& path);
	};

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
		void LookupLinkedItems(RE::TESDataHandler* const dataHandler, INI::LinkedItemsVec& rawLinkedItems = INI::linkedItems);

		void LogLinkedItemsLookup();

		/// <summary>
		/// Calculates DistributionSet for each linked form and calls a callback for each of them.
		/// </summary>
		/// <param name="linkedForms">A set of forms for which distribution sets should be calculated.
		///							  This is typically distributed forms accumulated during first distribution pass.</param>
		/// <param name="callback">A callback to be called with each DistributionSet. This is supposed to do the actual distribution.</param>
		void ForEachLinkedDistributionSet(const std::set<RE::TESForm*>& linkedForms, std::function<void(DistributionSet&)> callback);

	private:
		template <class Form>
		DataVec<Form>& LinkedFormsForForm(RE::TESForm* form, LinkedForms<Form>& linkedForms) const;

		LinkedForms<RE::SpellItem>      spells{ RECORD::kSpell };
		LinkedForms<RE::BGSPerk>        perks{ RECORD::kPerk };
		LinkedForms<RE::TESBoundObject> items{ RECORD::kItem };
		LinkedForms<RE::TESShout>       shouts{ RECORD::kShout };
		LinkedForms<RE::TESLevSpell>    levSpells{ RECORD::kLevSpell };
		LinkedForms<RE::TESForm>        packages{ RECORD::kPackage };
		LinkedForms<RE::BGSOutfit>      outfits{ RECORD::kOutfit };
		LinkedForms<RE::BGSKeyword>     keywords{ RECORD::kKeyword };
		LinkedForms<RE::TESFaction>     factions{ RECORD::kFaction };
		LinkedForms<RE::TESObjectARMO>  skins{ RECORD::kSkin };

		/// <summary>
		/// Iterates over each type of LinkedForms and calls a callback with each of them.
		/// </summary>
		template <typename Func, typename... Args>
		void ForEachLinkedForms(Func&& func, const Args&&... args);

	};

#pragma region Implementation
	template <class Form>
	DataVec<Form>& Manager::LinkedFormsForForm(RE::TESForm* form, LinkedForms<Form>& linkedForms) const
	{
		if (auto it = linkedForms.forms.find(form); it != linkedForms.forms.end()) {
			return it->second;
		} else {
			static DataVec<Form> empty{};
			return empty;
		}
	}

	template <typename Func, typename... Args>
	void Manager::ForEachLinkedForms(Func&& func, const Args&&... args)
	{
		func(keywords, std::forward<Args>(args)...);
		func(spells, std::forward<Args>(args)...);
		func(levSpells, std::forward<Args>(args)...);
		func(perks, std::forward<Args>(args)...);
		func(shouts, std::forward<Args>(args)...);
		func(items, std::forward<Args>(args)...);
		func(outfits, std::forward<Args>(args)...);
		func(factions, std::forward<Args>(args)...);
		func(packages, std::forward<Args>(args)...);
		func(skins, std::forward<Args>(args)...);
	}

	template <class Form>
	void LinkedForms<Form>::Link(Form* form, const FormVec& linkedForms, const RandomCount& count, const Chance& chance, const std::string& path)
	{
		for (const auto& linkedForm : linkedForms) {
			if (std::holds_alternative<RE::TESForm*>(linkedForm)) {
				auto& distributableForms = forms[std::get<RE::TESForm*>(linkedForm)];
				// Note that we don't use Data.index here, as these linked items doesn't have any leveled filters
				// and as such do not to track their index.
				distributableForms.emplace_back(0, form, count, FilterData({}, {}, {}, {}, chance), path);
			}
		}
	}
#pragma endregion
}
