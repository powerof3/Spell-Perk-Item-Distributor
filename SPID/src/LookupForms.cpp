#include "LookupForms.h"
#include "FormData.h"
#include "KeywordDependencies.h"

namespace Lookup
{
	using namespace Forms;

	bool LookupForms()
	{
		if (const auto dataHandler = RE::TESDataHandler::GetSingleton()) {
			const auto lookup_forms = [&]<class Form>(const Configs::Type type, Distributables<Form>& a_map) {
				for (const auto & config : Configs::configs) {
					a_map.LookupForms(dataHandler, type, config);    
				}
				
				if constexpr (std::is_same_v<RE::BGSKeyword, Form>) {
					Dependencies::ResolveKeywords();
				}
				a_map.FinishLookupForms();
			};

			lookup_forms(Configs::Type::kKeyword, keywords);
			lookup_forms(Configs::Type::kSpell, spells);
			lookup_forms(Configs::Type::kPerk, perks);
			lookup_forms(Configs::Type::kItem, items);
			lookup_forms(Configs::Type::kShout, shouts);
			lookup_forms(Configs::Type::kLevSpell, levSpells);
			lookup_forms(Configs::Type::kPackage, packages);
			lookup_forms(Configs::Type::kOutfit, outfits);
			lookup_forms(Configs::Type::kDeathItem, deathItems);
			lookup_forms(Configs::Type::kFaction, factions);
			lookup_forms(Configs::Type::kSleepOutfit, sleepOutfits);
			lookup_forms(Configs::Type::kSkin, skins);
		}

		return spells || perks || items || shouts || levSpells || packages || outfits || keywords || deathItems || factions || sleepOutfits || skins;
	}

	void LogFormLookup()
	{
		logger::info("{:*^50}", "PROCESSING");

		const auto list_lookup_result = [&]<class Form>(const RECORD::TYPE a_recordType, Distributables<Form>& a_map) {
			const auto& recordName = RECORD::add[a_recordType];

			const auto all = std::accumulate(Configs::configs.begin(), Configs::configs.end(), 0, [a_recordType](const auto count, const auto& config) {
				return count + config[a_recordType].size();
			});
			const auto added = a_map.GetSize();

			// Only log entries that are actually present in INIs.
			if (all > 0) {
				logger::info("Adding {}/{} {}s", added, all, recordName);
			}
		};

		list_lookup_result(Configs::Type::kKeyword, keywords);
		list_lookup_result(Configs::Type::kSpell, spells);
		list_lookup_result(Configs::Type::kPerk, perks);
		list_lookup_result(Configs::Type::kItem, items);
		list_lookup_result(Configs::Type::kShout, shouts);
		list_lookup_result(Configs::Type::kLevSpell, levSpells);
		list_lookup_result(Configs::Type::kPackage, packages);
		list_lookup_result(Configs::Type::kOutfit, outfits);
		list_lookup_result(Configs::Type::kDeathItem, deathItems);
		list_lookup_result(Configs::Type::kFaction, factions);
		list_lookup_result(Configs::Type::kSleepOutfit, sleepOutfits);
		list_lookup_result(Configs::Type::kSkin, skins);

		// Clear INI map once lookup is done
		Configs::configs.clear();

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

		list_filters(Configs::Type::kKeyword, keywords);
		list_filters(Configs::Type::kSpell, spells);
		list_filters(Configs::Type::kPerk, perks);
		list_filters(Configs::Type::kItem, items);
		list_filters(Configs::Type::kShout, shouts);
		list_filters(Configs::Type::kLevSpell, levSpells);
		list_filters(Configs::Type::kPackage, packages);
		list_filters(Configs::Type::kOutfit, outfits);
		list_filters(Configs::Type::kDeathItem, deathItems);
		list_filters(Configs::Type::kFaction, factions);
		list_filters(Configs::Type::kSleepOutfit, sleepOutfits);
		list_filters(Configs::Type::kSkin, skins);

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
