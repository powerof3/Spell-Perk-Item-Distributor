#include "LookupForms.h"
#include "FormData.h"
#include "KeywordDependencies.h"

namespace Lookup
{
	using namespace Forms;

	bool LookupForms()
	{
		if (const auto dataHandler = RE::TESDataHandler::GetSingleton()) {
			const auto lookup_forms = [&]<class Form>(const RECORD::TYPE a_recordType, Distributables<Form>& a_map) {
				const auto& recordName = RECORD::add[a_recordType];

				a_map.LookupForms(dataHandler, recordName, INI::configs[recordName]);
				if constexpr (std::is_same_v<RE::BGSKeyword, Form>) {
					Dependencies::ResolveKeywords();
				}
				a_map.FinishLookupForms();
			};

			lookup_forms(RECORD::kKeyword, keywords);
			lookup_forms(RECORD::kSpell, spells);
			lookup_forms(RECORD::kPerk, perks);
			lookup_forms(RECORD::kItem, items);
			lookup_forms(RECORD::kShout, shouts);
			lookup_forms(RECORD::kLevSpell, levSpells);
			lookup_forms(RECORD::kPackage, packages);
			lookup_forms(RECORD::kOutfit, outfits);
			lookup_forms(RECORD::kDeathItem, deathItems);
			lookup_forms(RECORD::kFaction, factions);
			lookup_forms(RECORD::kSleepOutfit, sleepOutfits);
			lookup_forms(RECORD::kSkin, skins);
		}

		return spells || perks || items || shouts || levSpells || packages || outfits || keywords || deathItems || factions || sleepOutfits || skins;
	}

	void LogFormLookup()
	{
		logger::info("{:*^50}", "PROCESSING");

		const auto list_lookup_result = [&]<class Form>(const RECORD::TYPE a_recordType, Distributables<Form>& a_map) {
			const auto& recordName = RECORD::add[a_recordType];

			const auto all = INI::configs[recordName].size();
			const auto added = a_map.GetSize();

			// Only log entries that are actually present in INIs.
			if (all > 0) {
				logger::info("Adding {}/{} {}s", added, all, recordName);
			}
		};

		list_lookup_result(RECORD::kKeyword, keywords);
		list_lookup_result(RECORD::kSpell, spells);
		list_lookup_result(RECORD::kPerk, perks);
		list_lookup_result(RECORD::kItem, items);
		list_lookup_result(RECORD::kShout, shouts);
		list_lookup_result(RECORD::kLevSpell, levSpells);
		list_lookup_result(RECORD::kPackage, packages);
		list_lookup_result(RECORD::kOutfit, outfits);
		list_lookup_result(RECORD::kDeathItem, deathItems);
		list_lookup_result(RECORD::kFaction, factions);
		list_lookup_result(RECORD::kSleepOutfit, sleepOutfits);
		list_lookup_result(RECORD::kSkin, skins);

		// Clear INI map once lookup is done
		INI::configs.clear();

		// Clear logger's buffer to free some memory :)
		buffered_logger::clear();
	}

	void LogFilters()
	{
		logger::info("{:*^50}", "FILTERS");

		const auto list_filters = [&]<class Form>(const RECORD::TYPE a_recordType, Distributables<Form>& a_map) {
			const auto& recordName = RECORD::add[a_recordType];

			// Only log entries that are actually present in INIs.
			if (a_map) {
				logger::info("\t{}:", recordName);

				// Group all entries by form
				std::unordered_map<RE::FormID, DataVec<Form>> map;
				for (const auto& data : a_map.GetForms()) {
					if (const auto form = data.form) {
						map[form->GetFormID()].push_back(data);
					}
				}

				for (const auto& [id, vec] : map) {
					if (!vec.empty()) {
						logger::info("\t\t{}:", describe(vec.front().form));
						for (const auto& data : vec) {
							std::ostringstream ss;
							const std::string  filters = data.filters.filters->describe(ss).str();
							logger::info("\t\t\t{}", filters);
						}
					}
				}
			}
		};

		list_filters(RECORD::kKeyword, keywords);
		list_filters(RECORD::kSpell, spells);
		list_filters(RECORD::kPerk, perks);
		list_filters(RECORD::kItem, items);
		list_filters(RECORD::kShout, shouts);
		list_filters(RECORD::kLevSpell, levSpells);
		list_filters(RECORD::kPackage, packages);
		list_filters(RECORD::kOutfit, outfits);
		list_filters(RECORD::kDeathItem, deathItems);
		list_filters(RECORD::kFaction, factions);
		list_filters(RECORD::kSleepOutfit, sleepOutfits);
		list_filters(RECORD::kSkin, skins);

		// Clear logger's buffer to free some memory :)
		buffered_logger::clear();
	}

	bool DoFormLookup()
	{
		logger::info("{:*^50}", "LOOKUP");

		const auto startTime = std::chrono::steady_clock::now();
		const bool success = LookupForms();
		const auto endTime = std::chrono::steady_clock::now();

		if (success) {
			LogFormLookup();
			LogFilters();

			const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
			logger::info("Lookup took {}μs / {}ms", duration, duration / 1000.0f);
		}

		return success;
	}
}
