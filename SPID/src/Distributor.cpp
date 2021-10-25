#include "Distributor.h"

static std::map<std::string, INIDataVec> INIs;

static FormDataVec<RE::SpellItem> spells;
static FormDataVec<RE::BGSPerk> perks;
static FormDataVec<RE::TESBoundObject> items;
static FormDataVec<RE::TESShout> shouts;
static FormDataVec<RE::TESLevSpell> levSpells;
static FormDataVec<RE::TESPackage> packages;
static FormDataVec<RE::BGSOutfit> outfits;
static FormDataVec<RE::BGSKeyword> keywords;
static FormDataVec<RE::TESBoundObject> deathItems;
static FormDataVec<RE::TESFaction> factions;

bool INI::Read()
{
	logger::info("{:*^30}", "INI");

	std::vector<std::string> configs;

	auto constexpr folder = R"(Data\)";
	for (const auto& entry : std::filesystem::directory_iterator(folder)) {
		if (entry.exists() && !entry.path().empty() && entry.path().extension() == ".ini"sv) {
			if (const auto path = entry.path().string(); path.rfind("_DISTR") != std::string::npos) {
				configs.push_back(path);
			}
		}
	}

	if (configs.empty()) {
		logger::warn("	No .ini files with _DISTR suffix were found within the Data folder, aborting...");
		return false;
	}

	logger::info("	{} matching inis found", configs.size());

	//initialize map
	for (size_t i = 0; i < RECORD::kTotal; i++) {
		INIs[RECORD::add[i]] = INIDataVec{};
	}

	for (auto& path : configs) {
		logger::info("	INI : {}", path);

		CSimpleIniA ini;
		ini.SetUnicode();
		ini.SetMultiKey();

		if (const auto rc = ini.LoadFile(path.c_str()); rc < 0) {
			logger::error("	couldn't read INI");
			continue;
		}

		if (auto values = ini.GetSection(""); values) {
			std::multimap<CSimpleIniA::Entry, std::pair<std::string, std::string>, CSimpleIniA::Entry::LoadOrder> oldFormatMap;

			for (auto& [key, entry] : *values) {
				auto [data, sanitized_str] = parse_ini(entry);
				INIs[key.pItem].emplace_back(data);
				if (sanitized_str.has_value()) {
					oldFormatMap.emplace(key, std::make_pair(entry, sanitized_str.value()));
				}
			}

			if (!oldFormatMap.empty()) {
				logger::info("		sanitizing {} entries", oldFormatMap.size());
				for (auto [key, entry] : oldFormatMap) {
					auto& [original, sanitized] = entry;
					ini.DeleteValue("", key.pItem, original.c_str());
					ini.SetValue("", key.pItem, sanitized.c_str(), key.pComment, false);
				}
				ini.SaveFile(path.c_str());
			}
		}
	}

	return true;
}

bool Lookup::GetForms()
{
	if (const auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler) {
		get_forms(dataHandler, "Spell", INIs["Spell"], spells);
		get_forms(dataHandler, "Perk", INIs["Perk"], perks);
		get_forms(dataHandler, "Item", INIs["Item"], items);
		get_forms(dataHandler, "Shout", INIs["Shout"], shouts);
		get_forms(dataHandler, "LevSpell", INIs["LevSpell"], levSpells);
		get_forms(dataHandler, "Package", INIs["Package"], packages);
		get_forms(dataHandler, "Outfit", INIs["Outfit"], outfits);
		get_forms(dataHandler, "Keyword", INIs["Keyword"], keywords);
		get_forms(dataHandler, "DeathItem", INIs["DeathItem"], deathItems);
		get_forms(dataHandler, "Faction", INIs["Faction"], factions);
	}

	const auto result = !spells.empty() || !perks.empty() || !items.empty() || !shouts.empty() || !levSpells.empty() || !packages.empty() || !outfits.empty() || !keywords.empty() || !deathItems.empty() || factions.empty();

	if (result) {
		logger::info("{:*^30}", "PROCESSING");

		logger::info("	Adding {}/{} spell(s)", spells.size(), INIs["Spell"].size());
		logger::info("	Adding {}/{} perks(s)", perks.size(), INIs["Perk"].size());
		logger::info("	Adding {}/{} items(s)", items.size(), INIs["Item"].size());
		logger::info("	Adding {}/{} shouts(s)", shouts.size(), INIs["Shout"].size());
		logger::info("	Adding {}/{} leveled spell(s)", levSpells.size(), INIs["LevSpell"].size());
		logger::info("	Adding {}/{} package(s)", packages.size(), INIs["Package"].size());
		logger::info("	Adding {}/{} outfits(s)", outfits.size(), INIs["Outfit"].size());
		logger::info("	Adding {}/{} keywords(s)", keywords.size(), INIs["Keyword"].size());
		logger::info("	Adding {}/{} death item(s)", deathItems.size(), INIs["DeathItem"].size());
		logger::info("	Adding {}/{} faction(s)", factions.size(), INIs["Faction"].size());
	}
	return result;
}

void Distribute::ApplyToNPCs()
{
	if (const auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler) {
		for (const auto& actorbase : dataHandler->GetFormArray<RE::TESNPC>()) {
			if (actorbase && !actorbase->IsPlayer()) {
				for_each_form<RE::BGSKeyword>(*actorbase, keywords, [&](const auto& a_keywordsPair) {
					const auto keyword = a_keywordsPair.first;
					if (!actorbase->HasKeyword(keyword->formEditorID)) {
						return actorbase->AddKeyword(keyword);
					}
					return false;
				});

				for_each_form<RE::SpellItem>(*actorbase, spells, [&](const auto& a_spellPair) {
					const auto spell = a_spellPair.first;
					const auto actorEffects = actorbase->GetSpellList();
					return actorEffects && actorEffects->AddSpell(spell);
				});

				for_each_form<RE::BGSPerk>(*actorbase, perks, [&](const auto& a_perkPair) {
					const auto perk = a_perkPair.first;
					return actorbase->AddPerk(perk, 1);
				});

				for_each_form<RE::TESShout>(*actorbase, shouts, [&](const auto& a_shoutPair) {
					const auto shout = a_shoutPair.first;
					const auto actorEffects = actorbase->GetSpellList();
					return actorEffects && actorEffects->AddShout(shout);
				});

				for_each_form<RE::TESLevSpell>(*actorbase, levSpells, [&](const auto& a_levSpellPair) {
					const auto levSpell = a_levSpellPair.first;
					const auto actorEffects = actorbase->GetSpellList();
					return actorEffects && actorEffects->AddLevSpell(levSpell);
				});

				for_each_form<RE::BGSOutfit>(*actorbase, outfits, [&](const auto& a_outfitPair) {
					actorbase->defaultOutfit = a_outfitPair.first;
					return true;
				});

				for_each_form<RE::TESFaction>(*actorbase, factions, [&](const auto& a_factionPair) {
					if (!actorbase->IsInFaction(a_factionPair.first)) {
						RE::FACTION_RANK faction;
						faction.faction = a_factionPair.first;
						faction.rank = 1;
						actorbase->factions.push_back(faction);
						return true;
					}
					return false;
				});

				for_each_form<RE::TESBoundObject>(*actorbase, items, [&](const auto& a_itemPair) {
					const auto& [item, count] = a_itemPair;
					return actorbase->AddObjectToContainer(item, count, actorbase);
				});

				for_each_form<RE::TESPackage>(*actorbase, packages, [&](const auto& a_packagePair) {
					const auto package = a_packagePair.first;
					auto& packagesList = actorbase->aiPackages.packages;
					if (std::ranges::find(packagesList, package) == packagesList.end()) {
						packagesList.push_front(package);
						const auto packageList = actorbase->defaultPackList;
						if (packageList && !packageList->HasForm(package)) {
							packageList->AddForm(package);
							return true;
						}
					}
					return false;
				});
			}
		}

		logger::info("{:*^30}", "RESULT");

		auto const totalNPCs = dataHandler->GetFormArray<RE::TESNPC>().size() - 1;  //ignore player
		if (!keywords.empty()) {
			list_npc_count("Keywords", keywords, totalNPCs);
		}
		if (!spells.empty()) {
			list_npc_count("Spells", spells, totalNPCs);
		}
		if (!perks.empty()) {
			list_npc_count("Perks", perks, totalNPCs);
		}
		if (!items.empty()) {
			list_npc_count("Items", items, totalNPCs);
		}
		if (!shouts.empty()) {
			list_npc_count("Shouts", shouts, totalNPCs);
		}
		if (!levSpells.empty()) {
			list_npc_count("LevSpells", levSpells, totalNPCs);
		}
		if (!packages.empty()) {
			list_npc_count("Packages", packages, totalNPCs);
		}
		if (!outfits.empty()) {
			list_npc_count("Outfits", outfits, totalNPCs);
		}
		if (!factions.empty()) {
			list_npc_count("Factions", factions, totalNPCs);
		}
	}
}

namespace Distribute
{
	void DeathItemManager::Register()
	{
		if (deathItems.empty()) {
			return;
		}

		auto scripts = RE::ScriptEventSourceHolder::GetSingleton();
		if (scripts) {
			scripts->AddEventSink(GetSingleton());
			logger::info("	Registered {}"sv, typeid(DeathItemManager).name());
		}
	}

	DeathItemManager::EventResult DeathItemManager::ProcessEvent(const RE::TESDeathEvent* a_event, RE::BSTEventSource<RE::TESDeathEvent>*)
	{
		constexpr auto is_NPC = [](auto&& a_ref) {
			return a_ref && !a_ref->IsPlayerRef();
		};

		if (a_event && a_event->dead && is_NPC(a_event->actorDying)) {
			const auto actor = a_event->actorDying->As<RE::Actor>();
			const auto base = actor ? actor->GetActorBase() : nullptr;
			if (actor && base) {
				for_each_form<RE::TESBoundObject>(*base, deathItems, [&](const auto& a_deathItemPair) {
					const auto& [deathItem, count] = a_deathItemPair;
					detail::add_item(actor, deathItem, count, true, 0, RE::BSScript::Internal::VirtualMachine::GetSingleton());
					return true;
				});
			}
		}

		return EventResult::kContinue;
	}
}

namespace Distribute::LeveledActor
{
	struct SetObjectReference
	{
		static void thunk(RE::Character* a_this, RE::TESBoundObject* a_base)
		{
			func(a_this, a_base);

			const auto actorbase = a_this->GetActorBase();
			const auto baseTemplate = actorbase ? actorbase->baseTemplateForm : nullptr;

			if (baseTemplate && baseTemplate->Is(RE::FormType::LeveledNPC)) {
				for_each_form<RE::TESBoundObject>(*actorbase, items, [&](const auto& a_itemPair) {
					const auto& [item, count] = a_itemPair;
					if (const auto itemCount = actorbase->CountObjectsInContainer(item); itemCount < count) {
						return item && actorbase->AddObjectToContainer(item, count, actorbase);
					}
					return false;
				});

				for_each_form<RE::BGSOutfit>(*actorbase, outfits, [&](const auto& a_outfitsPair) {
					actorbase->defaultOutfit = a_outfitsPair.first;
					return true;
				});
			}
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	void Install()
	{
		if (items.empty() && outfits.empty()) {
			return;
		}

		stl::write_vfunc<RE::Character, 0x084, SetObjectReference>();
		logger::info("	Hooked leveled actor init");
	}
}
