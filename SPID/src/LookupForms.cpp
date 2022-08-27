#include "LookupForms.h"
#include "KeywordDependencies.h"
#include "LookupConfigs.h"

bool Lookup::GetForms()
{
	if (const auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler) {
		const auto lookup_forms = [&]<class Form>(const std::string& a_formType, FormMap<Form>& a_map) {
			get_forms(dataHandler, a_formType, INI::configs[a_formType], a_map);
			get_forms_with_level_filters(a_map);
		};

		lookup_forms("Spell", spells);
		lookup_forms("Perk", perks);
		lookup_forms("Item", items);
		lookup_forms("Shout", shouts);
		lookup_forms("LevSpell", levSpells);
		lookup_forms("Package", packages);
		lookup_forms("Outfit", outfits);
		lookup_forms("Keyword", keywords);
		lookup_forms("DeathItem", deathItems);
		lookup_forms("Faction", factions);

		if (keywords) {
			Dependencies::ResolveKeywords();
		}
	}

	const auto result = spells || perks || items || shouts || levSpells || packages || outfits || keywords || deathItems || factions;

	if (result) {
		logger::info("{:*^30}", "PROCESSING");

		const auto list_lookup_result = [&]<class Form>(const std::string& a_formType, FormMap<Form>& a_map) {
			logger::info("	Adding {}/{} {}(s)", a_map.forms.size(), INI::configs[a_formType].size(), a_formType);
		};

		list_lookup_result("Spell", spells);
		list_lookup_result("Perk", perks);
		list_lookup_result("Item", items);
		list_lookup_result("Shout", shouts);
		list_lookup_result("LevSpell", levSpells);
		list_lookup_result("Package", packages);
		list_lookup_result("Outfit", outfits);
		list_lookup_result("Keyword", keywords);
		list_lookup_result("DeathItem", deathItems);
		list_lookup_result("Faction", factions);
	}
	return result;
}
