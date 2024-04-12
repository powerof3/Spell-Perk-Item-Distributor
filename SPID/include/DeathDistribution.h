#pragma once
#include "FormData.h"

namespace DeathDistribution
{
	namespace INI
	{
		/// <summary>
		/// Checks whether given entry is an on death distribuatble form and attempts to parse it.
		/// </summary>
		/// <returns>true if given entry was an on death distribuatble form. Note that returned value doesn't represent whether parsing was successful.</returns>
		bool TryParse(const std::string& key, const std::string& value, const Path&);
	}

	using namespace Forms;

	class Manager :
		public ISingleton<Manager>,
		public RE::BSTEventSink<RE::TESDeathEvent>
	{
	public:
		static void Register();

		/// <summary>
		/// Does a forms lookup similar to what Filters do.
		///
		/// As a result this method configures Manager with discovered valid On Death Distributable Forms.
		/// </summary>
		void LookupForms(RE::TESDataHandler* const dataHandler);

		void LogFormsLookup();

		bool IsEmpty();

	private:
		Distributables<RE::SpellItem>      spells{ RECORD::kSpell };
		Distributables<RE::BGSPerk>        perks{ RECORD::kPerk };
		Distributables<RE::TESBoundObject> items{ RECORD::kItem };
		Distributables<RE::TESShout>       shouts{ RECORD::kShout };
		Distributables<RE::TESLevSpell>    levSpells{ RECORD::kLevSpell };
		Distributables<RE::TESForm>        packages{ RECORD::kPackage };
		Distributables<RE::BGSOutfit>      outfits{ RECORD::kOutfit };
		Distributables<RE::BGSKeyword>     keywords{ RECORD::kKeyword };
		Distributables<RE::TESFaction>     factions{ RECORD::kFaction };
		Distributables<RE::BGSOutfit>      sleepOutfits{ RECORD::kSleepOutfit };
		Distributables<RE::TESObjectARMO>  skins{ RECORD::kSkin };

		/// <summary>
		/// Iterates over each type of LinkedForms and calls a callback with each of them.
		/// </summary>
		template <typename Func, typename... Args>
		void ForEachDistributable(Func&& func, Args&&... args);

	protected:
		RE::BSEventNotifyControl ProcessEvent(const RE::TESDeathEvent*, RE::BSTEventSource<RE::TESDeathEvent>*) override;
	};

#pragma region Implementation
	template <typename Func, typename... Args>
	void Manager::ForEachDistributable(Func&& func, Args&&... args)
	{
		func(keywords, std::forward<Args>(args)...);
		func(factions, std::forward<Args>(args)...);
		func(spells, std::forward<Args>(args)...);
		func(levSpells, std::forward<Args>(args)...);
		func(perks, std::forward<Args>(args)...);
		func(shouts, std::forward<Args>(args)...);
		func(packages, std::forward<Args>(args)...);
		func(outfits, std::forward<Args>(args)...);
		func(sleepOutfits, std::forward<Args>(args)...);
		func(skins, std::forward<Args>(args)...);
		func(items, std::forward<Args>(args)...);
	}
#pragma endregion
}
