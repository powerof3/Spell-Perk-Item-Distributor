#include "LookupForms.h"
#include "KeywordDependencies.h"
#include "LookupConfigs.h"

bool Lookup::GetForms()
{
	if (const auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler) {
		const auto lookup_forms = [&]<class Form>(const RECORD::TYPE a_recordType, FormMap<Form>& a_map) {
			const auto& recordName = RECORD::add[a_recordType];

		    get_forms(dataHandler, recordName, INI::configs[recordName], a_map);
			get_forms_with_level_filters(a_map);
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

		if (keywords) {
			Dependencies::ResolveKeywords();
		}
	}

	const auto result = spells || perks || items || shouts || levSpells || packages || outfits || keywords || deathItems || factions || sleepOutfits || skins;

	if (result) {
		logger::info("{:*^50}", "PROCESSING");

		const auto list_lookup_result = [&]<class Form>(const RECORD::TYPE a_recordType, FormMap<Form>& a_map) {
			const auto& recordName = RECORD::add[a_recordType];

		    logger::info("	Adding {}/{} {}(s)", a_map.forms.size(), INI::configs[recordName].size(), recordName);
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
	}
	return result;
}
