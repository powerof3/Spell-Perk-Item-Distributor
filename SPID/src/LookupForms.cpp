#include "LookupConfigs.h"
#include "LookupForms.h"

bool Lookup::GetForms()
{
	if (const auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler) {
		get_forms(dataHandler, "Spell", INI::configs["Spell"], spells);
		get_forms(dataHandler, "Perk", INI::configs["Perk"], perks);
		get_forms(dataHandler, "Item", INI::configs["Item"], items);
		get_forms(dataHandler, "Shout", INI::configs["Shout"], shouts);
		get_forms(dataHandler, "LevSpell", INI::configs["LevSpell"], levSpells);
		get_forms(dataHandler, "Package", INI::configs["Package"], packages);
		get_forms(dataHandler, "Outfit", INI::configs["Outfit"], outfits);
		get_forms(dataHandler, "Keyword", INI::configs["Keyword"], keywords);
		get_forms(dataHandler, "DeathItem", INI::configs["DeathItem"], deathItems);
		get_forms(dataHandler, "Faction", INI::configs["Faction"], factions);
	}

	const auto result = !spells.empty() || !perks.empty() || !items.empty() || !shouts.empty() || !levSpells.empty() || !packages.empty() || !outfits.empty() || !keywords.empty() || !deathItems.empty() || factions.empty();

	if (result) {
		logger::info("{:*^30}", "PROCESSING");

		logger::info("	Adding {}/{} spell(s)", spells.size(), INI::configs["Spell"].size());
		logger::info("	Adding {}/{} perks(s)", perks.size(), INI::configs["Perk"].size());
		logger::info("	Adding {}/{} items(s)", items.size(), INI::configs["Item"].size());
		logger::info("	Adding {}/{} shouts(s)", shouts.size(), INI::configs["Shout"].size());
		logger::info("	Adding {}/{} leveled spell(s)", levSpells.size(), INI::configs["LevSpell"].size());
		logger::info("	Adding {}/{} package(s)", packages.size(), INI::configs["Package"].size());
		logger::info("	Adding {}/{} outfits(s)", outfits.size(), INI::configs["Outfit"].size());
		logger::info("	Adding {}/{} keywords(s)", keywords.size(), INI::configs["Keyword"].size());
		logger::info("	Adding {}/{} death item(s)", deathItems.size(), INI::configs["DeathItem"].size());
		logger::info("	Adding {}/{} faction(s)", factions.size(), INI::configs["Faction"].size());
	}
	return result;
}
