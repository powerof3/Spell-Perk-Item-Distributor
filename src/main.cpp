#include "main.h"
#include "version.h"

static INIDataVec spellsINI;
static INIDataVec perksINI;
static INIDataVec itemsINI;
static INIDataVec shoutsINI;
static INIDataVec levSpellsINI;
static INIDataVec packagesINI;
static INIDataVec outfitsINI;

static FormDataVec<RE::SpellItem> spells;
static FormDataVec<RE::BGSPerk> perks;
static FormDataVec<RE::TESBoundObject> items;
static FormDataVec<RE::TESLevSpell> levSpells;
static FormDataVec<RE::TESShout> shouts;
static FormDataVec<RE::TESPackage> packages;
static FormDataVec<RE::BGSOutfit> outfits;

std::unordered_set<RE::TESBoundObject*> equipOnLoadForms;

namespace UTIL
{
	using namespace SKSE::UTIL::STRING;

	std::vector<std::string> splitTrimmedString(const std::string& a_str, bool a_trim, const std::string& a_delimiter)
	{
		if (!a_str.empty() && a_str.find("NONE") == std::string::npos) {
			if (!onlySpace(a_str)) {
				auto split_strs = split(a_str.c_str(), a_delimiter);
				if (a_trim) {
					for (auto& split_str : split_strs) {
						trim(split_str);
					}
				}
				return split_strs;
			}
		}
		return std::vector<std::string>();
	}


	std::string LookupFormType(RE::FormType a_type)
	{
		switch (a_type) {
		case RE::FormType::Faction:
			return "Faction";
		case RE::FormType::Class:
			return "Class";
		case RE::FormType::CombatStyle:
			return "CombatStyle";
		case RE::FormType::Race:
			return "Race";
		case RE::FormType::Outfit:
			return "Outfit";
		default:
			return std::string();
		}
	}
}


INIData GetINIData(const std::string& a_value)
{
	using TYPE = INI_TYPE;
	namespace STRING = SKSE::UTIL::STRING;

	auto sections = STRING::split(a_value, " | ");
	logger::info("		{}", a_value);

	INIData data;
	auto& [formIDPair_ini, strings_ini, filterIDs, level_ini, gender_ini, itemCount_ini, chance_ini] = data;

	//FORMID/ESP
	std::pair<RE::FormID, std::string> item_ID;
	try {
		auto formIDpair = STRING::split(sections.at(TYPE::kFormID).c_str(), " - ");
		auto espStr = formIDpair.at(TYPE::kESP);

		//FIX FOR MODS WITH "-" CHARACTERS
		auto const size = formIDpair.size();
		if (size > 2) {
			for (size_t i = 2; i < size; i++) {
				espStr += " - " + formIDpair.at(i);
			}
		}

		item_ID.first = std::stoul(formIDpair.at(TYPE::kFormID), nullptr, 16);
		item_ID.second = espStr;
	} catch (...) {
		item_ID.first = 0;
		item_ID.second = "Skyrim.esm";
	}
	formIDPair_ini = item_ID;

	//KEYWORDS
	try {
		strings_ini = UTIL::splitTrimmedString(sections.at(TYPE::kStrings), true);
	} catch (...) {
	}

	//FILTER FORMS
	try {
		auto split_IDs = UTIL::splitTrimmedString(sections.at(TYPE::kFilterIDs), false);
		for (auto& IDs : split_IDs) {
			filterIDs.emplace_back(std::stoul(IDs, nullptr, 16));
		}
	} catch (...) {
	}

	//LEVEL
	ActorLevel actorLevelPair = { ACTOR_LEVEL::MAX, ACTOR_LEVEL::MAX };
	SkillLevel skillLevelPair = { SKILL_LEVEL::TYPE_MAX, { SKILL_LEVEL::VALUE_MAX, SKILL_LEVEL::VALUE_MAX } };
	try {
		auto split_levels = UTIL::splitTrimmedString(sections.at(TYPE::kLevel), false);
		for (auto& levels : split_levels) {
			if (levels.find("(") != std::string::npos) {
				auto sanitizedLevel = STRING::removeNonAlphaNumeric(levels);
				auto skills = STRING::split(sanitizedLevel, " ");
				if (!skills.empty()) {
					if (skills.size() > 2) {
						auto type = to_int<std::uint32_t>(skills.at(S_LEVEL::kType));
						auto minLevel = to_int<std::uint8_t>(skills.at(S_LEVEL::kMin));
						auto maxLevel = to_int<std::uint8_t>(skills.at(S_LEVEL::kMax));
						skillLevelPair = { type, { minLevel, maxLevel } };
					} else {
						auto type = to_int<std::uint32_t>(skills.at(S_LEVEL::kType));
						auto minLevel = to_int<std::uint8_t>(skills.at(S_LEVEL::kMin));
						skillLevelPair = { type, { minLevel, SKILL_LEVEL::VALUE_MAX }};
					}
				}
			} else {
				auto split_level = STRING::split(levels, "/");
				if (split_level.size() > 1) {
					auto minLevel = to_int<std::uint16_t>(split_level.at(A_LEVEL::kMin));
					auto maxLevel = to_int<std::uint16_t>(split_level.at(A_LEVEL::kMax));
					actorLevelPair = { minLevel, maxLevel };
				} else {
					auto level = to_int<std::uint16_t>(levels);
					actorLevelPair = { level, ACTOR_LEVEL::MAX };
				}
			}
		}
	} catch (...) {
	}
	level_ini = { actorLevelPair, skillLevelPair };

	//GENDER
	gender_ini = RE::SEX::kNone;
	try {
		auto genderStr = sections.at(TYPE::kGender);
		if (!genderStr.empty() && genderStr.find("NONE") == std::string::npos) {
			genderStr.erase(remove(genderStr.begin(), genderStr.end(), ' '), genderStr.end());
			if (genderStr == "M") {
				gender_ini = RE::SEX::kMale;
			} else if (genderStr == "F") {
				gender_ini = RE::SEX::kFemale;
			} else {
				gender_ini = static_cast<RE::SEX>(to_int<std::uint32_t>(sections.at(TYPE::kGender)));
			}
		}
	} catch (...) {
	}

	//ITEMCOUNT
	itemCount_ini = { 1, false };
	try {
		auto itemCountStr = sections.at(TYPE::kItemCount);
		if (!itemCountStr.empty() && itemCountStr.find("NONE") == std::string::npos) {
			auto split_itemCount = STRING::split(itemCountStr, "/");
			if (split_itemCount.size() > 1) {
				auto itemChance = to_int<std::uint32_t>(split_itemCount.at(0));
				auto equipOnLoad = STRING::bool_cast(split_itemCount.at(1));
				itemCount_ini = { itemChance, equipOnLoad };
			} else {
				itemCount_ini.first = to_int<std::uint32_t>(itemCountStr);
			}

		}
	} catch (...) {
	}

	//CHANCE
	chance_ini = 100;
	try {
		auto chanceStr = sections.at(TYPE::kChance);
		if (!chanceStr.empty() && chanceStr.find("NONE") == std::string::npos) {
			chance_ini = to_int<std::uint32_t>(chanceStr);
		}
	} catch (...) {
	}

	return data;
}


void GetDataFromINI(const CSimpleIniA& ini, const char* a_type, INIDataVec& a_INIDataVec)
{
	CSimpleIniA::TNamesDepend values;
	ini.GetAllValues("", a_type, values);
	values.sort(CSimpleIniA::Entry::LoadOrder());

	logger::info("	{} entries found : {}", a_type, values.size());

	for (auto& value : values) {
		a_INIDataVec.emplace_back(GetINIData(value.pItem));
	}
}


bool ReadINIs()
{
	logger::info("{:*^30}", "INI");

	auto vec = SKSE::GetAllConfigPaths(R"(Data\)", "_DISTR");
	if (vec.empty()) {
		logger::warn("No .ini files with _DISTR suffix were found within the Data folder, aborting...");
		return false;
	}

	logger::info("{} matching inis found", vec.size());

	for (auto& path : vec) {
		logger::info("ini : {}", path);

		CSimpleIniA ini;
		ini.SetUnicode();
		ini.SetMultiKey();

		SI_Error rc = ini.LoadFile(path.c_str());
		if (rc < 0) {
			logger::error("	couldn't read path");
			continue;
		}

		GetDataFromINI(ini, "Spell", spellsINI);
		GetDataFromINI(ini, "Perk", perksINI);
		GetDataFromINI(ini, "Item", itemsINI);
		GetDataFromINI(ini, "Shout", shoutsINI);
		GetDataFromINI(ini, "LevSpell", levSpellsINI);
		GetDataFromINI(ini, "Package", packagesINI);
		GetDataFromINI(ini, "Outfit", outfitsINI);
	}

	return true;
}


bool LookupAllForms()
{
	auto dataHandler = RE::TESDataHandler::GetSingleton();
	if (dataHandler) {
		logger::info("{:*^30}", "LOOKUP");

		if (!spellsINI.empty()) {
			LookupForms("Spell", spellsINI, spells);
		}
		if (!perksINI.empty()) {
			LookupForms("Perk", perksINI, perks);
		}
		if (!itemsINI.empty()) {
			LookupForms("Item", itemsINI, items);
		}
		if (!shoutsINI.empty()) {
			LookupForms("Shout", shoutsINI, shouts);
		}
		if (!levSpellsINI.empty()) {
			LookupForms("LevSpell", levSpellsINI, levSpells);
		}
		if (!packagesINI.empty()) {
			LookupForms("Package", packagesINI, packages);
		}
		if (outfitsINI.empty()) {
			LookupForms("Outfit", outfitsINI, outfits);
		}
	}
	return !spells.empty() || !perks.empty() || !items.empty() || !shouts.empty() || !levSpells.empty() || !packages.empty() || !outfits.empty() ?
				 true :
				 false;
}


void ApplyNPCRecords()
{
	auto dataHandler = RE::TESDataHandler::GetSingleton();
	if (dataHandler) {
		for (const auto& actorbase : dataHandler->GetFormArray<RE::TESNPC>()) {
			if (actorbase && !actorbase->IsPlayer()) {
				ForEachForm<RE::SpellItem>(actorbase, spells, [&](const FormCountPair<RE::SpellItem> a_spellPair) {
					auto spell = a_spellPair.first;
					if (spell) {
						auto actorEffects = actorbase->GetOrCreateSpellList();
						if (actorEffects && actorEffects->AddSpell(spell)) {
							return true;
						}
					}
					return false;
				});
				ForEachForm<RE::BGSPerk>(actorbase, perks, [&](const FormCountPair<RE::BGSPerk> a_perkPair) {
					auto perk = a_perkPair.first;
					if (perk && actorbase->AddPerk(perk)) {
						return true;
					}
					return false;
				});
				ForEachForm<RE::TESBoundObject>(actorbase, items, [&](const FormCountPair<RE::TESBoundObject> a_itemPair) {
					auto item = a_itemPair.first;
					auto count = a_itemPair.second;
					if (item && actorbase->AddObjectToContainer(item, count, actorbase)) {
						return true;
					}
					return false;
				});
				ForEachForm<RE::TESShout>(actorbase, shouts, [&](const FormCountPair<RE::TESShout> a_shoutPair) {
					auto shout = a_shoutPair.first;
					if (shout) {
						auto actorEffects = actorbase->GetOrCreateSpellList();
						if (actorEffects && actorEffects->AddShout(shout)) {
							return true;
						}
					}
					return false;
				});
				ForEachForm<RE::TESLevSpell>(actorbase, levSpells, [&](const FormCountPair<RE::TESLevSpell> a_levSpellPair) {
					auto levSpell = a_levSpellPair.first;
					if (levSpell) {
						auto actorEffects = actorbase->GetOrCreateSpellList();
						if (actorEffects && actorEffects->AddLevSpell(levSpell)) {
							return true;
						}
					}
					return false;
				});
				ForEachForm<RE::TESPackage>(actorbase, packages, [&](const FormCountPair<RE::TESPackage> a_packagePair) {
					auto package = a_packagePair.first;
					if (package) {
						auto& packagesList = actorbase->aiPackages.packages;
						if (std::find(packagesList.begin(), packagesList.end(), package) == packagesList.end()) {
							packagesList.push_front(package);
							auto packageList = actorbase->defaultPackList;
							if (packageList && !packageList->HasForm(package)) {
								packageList->AddForm(package);
							}
							return true;
						}
					}
					return false;
				});
				ForEachForm<RE::BGSOutfit>(actorbase, outfits, [&](const FormCountPair<RE::BGSOutfit> a_outfitsPair) {
					auto outfit = a_outfitsPair.first;
					if (outfit) {
						actorbase->defaultOutfit = outfit;
						return true;
					}
					return false;
				});
			}
		}

		logger::info("{:*^30}", "RESULT");

		auto const totalNPCs = dataHandler->GetFormArray<RE::TESNPC>().size();
		if (!spells.empty()) {
			ListNPCCount("Spells", spells, totalNPCs);
		}
		if (!perks.empty()) {
			ListNPCCount("Perks", perks, totalNPCs);
		}
		if (!items.empty()) {
			ListNPCCount("Items", items, totalNPCs);
		}
		if (!shouts.empty()) {
			ListNPCCount("Shouts", shouts, totalNPCs);
		}
		if (!levSpells.empty()) {
			ListNPCCount("LevSpells", levSpells, totalNPCs);
		}
		if (!packages.empty()) {
			ListNPCCount("Packages", packages, totalNPCs);
		}
		if (!outfits.empty()) {
			ListNPCCount("Outfits", outfits, totalNPCs);
		}
	}
}


void OnInit(SKSE::MessagingInterface::Message* a_msg)
{
	switch (a_msg->type) {
	case SKSE::MessagingInterface::kDataLoaded:
		{
			if (LookupAllForms()) {
				logger::info("{:*^30}", "PROCESSING");

				logger::info("	Adding {}/{} spell(s)", spells.size(), spellsINI.size());
				logger::info("	Adding {}/{} perks(s)", perks.size(), perksINI.size());
				logger::info("	Adding {}/{} items(s)", items.size(), itemsINI.size());
				logger::info("	Adding {}/{} shouts(s)", shouts.size(), shoutsINI.size());
				logger::info("	Adding {}/{} leveled spell(s)", levSpells.size(), levSpellsINI.size());
				logger::info("	Adding {}/{} package(s)", packages.size(), packagesINI.size());
				logger::info("	Adding {}/{} outfits(s)", outfits.size(), outfitsINI.size());
				ApplyNPCRecords();
			} else {
				logger::error("No distributables were located within game files...");
			}
		}
		break;
	}
}


extern "C" DLLEXPORT bool APIENTRY SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
	try {
		auto path = logger::log_directory().value() / "po3_SpellPerkItemDistributor.log";
		auto log = spdlog::basic_logger_mt("global log", path.string(), true);
		log->flush_on(spdlog::level::info);

#ifndef NDEBUG
		log->set_level(spdlog::level::debug);
		log->sinks().push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
#else
		log->set_level(spdlog::level::info);

#endif

		spdlog::set_default_logger(log);
		spdlog::set_pattern("[%H:%M:%S:%e] [%l] %v");

		logger::info("po3_SpellPerkItemDistributor v{}", SPID_VERSION_VERSTRING);

		a_info->infoVersion = SKSE::PluginInfo::kVersion;
		a_info->name = "powerofthree's Spell Perk Distributor";
		a_info->version = SPID_VERSION_MAJOR;

		if (a_skse->IsEditor()) {
			logger::critical("Loaded in editor, marking as incompatible");
			return false;
		}

		const auto ver = a_skse->RuntimeVersion();
		if (ver < SKSE::RUNTIME_1_5_39) {
			logger::critical("Unsupported runtime version {}", ver.string());
			return false;
		}
	} catch (const std::exception& e) {
		logger::critical(e.what());
		return false;
	} catch (...) {
		logger::critical("caught unknown exception");
		return false;
	}

	return true;
}


extern "C" DLLEXPORT bool APIENTRY SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	try {
		logger::info("SpellPerkItem Distributor loaded");

		SKSE::Init(a_skse);
		if (ReadINIs()) {
			auto messaging = SKSE::GetMessagingInterface();
			if (!messaging->RegisterListener("SKSE", OnInit)) {
				logger::critical("Failed to register messaging listener!\n");
				return false;
			}
		}
	} catch (const std::exception& e) {
		logger::critical(e.what());
		return false;
	} catch (...) {
		logger::critical("caught unknown exception");
		return false;
	}

	return true;
}
