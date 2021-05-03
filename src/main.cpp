#include "main.h"
#include "version.h"

static INIDataVec spellsINI;
static INIDataVec perksINI;
static INIDataVec itemsINI;
static INIDataVec shoutsINI;
static INIDataVec levSpellsINI;
static INIDataVec packagesINI;
static INIDataVec outfitsINI;
static INIDataVec keywordsINI;
static INIDataVec deathItemsINI;

static FormDataVec<RE::SpellItem> spells;
static FormDataVec<RE::BGSPerk> perks;
static FormDataVec<RE::TESBoundObject> items;
static FormDataVec<RE::TESLevSpell> levSpells;
static FormDataVec<RE::TESShout> shouts;
static FormDataVec<RE::TESPackage> packages;
static FormDataVec<RE::BGSOutfit> outfits;
static FormDataVec<RE::BGSKeyword> keywords;
static FormDataVec<RE::TESLevItem> deathItems;

static std::set<std::pair<RE::FormID, RE::FormID>> templateSet;

namespace UTIL
{
	auto splitTrimmedString(const std::string& a_str, const bool a_trim, const std::string& a_delimiter) -> std::vector<std::string>
	{
		if (!a_str.empty() && a_str.find("NONE"sv) == std::string::npos) {
			if (!onlySpace(a_str)) {
				auto split_strs = split(a_str, a_delimiter);
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


	auto LookupFormType(const RE::FormType a_type) -> std::string
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


	bool FormIDToForm(const FormIDVec& a_formIDVec, FormVec& a_formVec, const std::string& a_type)
	{
		if (!a_formIDVec.empty()) {
			logger::info("			Starting {} lookup", a_type);
			for (auto filterID : a_formIDVec) {
				auto filterForm = RE::TESForm::LookupByID(filterID);
				if (filterForm) {
					auto formType = filterForm->GetFormType();
					auto const type = LookupFormType(formType);
					if (!type.empty()) {
						logger::info("				{} [0x{:08X}] SUCCESS", type, filterID);
						a_formVec.push_back(filterForm);
					} else {
						logger::error("				[0x{:08X}]) FAIL - wrong formtype ({})", filterID, formType);
						return false;
					}
				} else {
					logger::error("					[0x{:08X}] FAIL - form doesn't exist", filterID);
					return false;
				}
			}
		}
		return true;
	}


	auto GetNameMatch(const std::string& a_name, const StringVec& a_strings) -> bool
	{
		return std::any_of(a_strings.begin(), a_strings.end(), [&a_name](const std::string& str) {
			return insenstiveStringCompare(str, a_name);
		});
	}


	auto GetFilterFormMatch(RE::TESNPC& a_actorbase, const FormVec& a_forms) -> bool
	{
		for (auto& form : a_forms) {
			if (form) {
				switch (form->GetFormType()) {
				case RE::FormType::CombatStyle:
					{
						const auto combatStyle = form->As<RE::TESCombatStyle>();
						if (combatStyle && a_actorbase.GetCombatStyle() == combatStyle) {
							return true;
						}
					}
					break;
				case RE::FormType::Class:
					{
						const auto npcClass = form->As<RE::TESClass>();
						if (npcClass && a_actorbase.IsInClass(npcClass)) {
							return true;
						}
					}
					break;
				case RE::FormType::Faction:
					{
						const auto faction = form->As<RE::TESFaction>();
						if (faction && a_actorbase.IsInFaction(faction)) {
							return true;
						}
					}
					break;
				case RE::FormType::Race:
					{
						const auto race = form->As<RE::TESRace>();
						if (race && a_actorbase.GetRace() == race) {
							return true;
						}
					}
					break;
				case RE::FormType::Outfit:
					{
						const auto outfit = form->As<RE::BGSOutfit>();
						if (outfit && a_actorbase.defaultOutfit == outfit) {
							return true;
						}
					}
					break;
				default:
					break;
				}
			}
		}
		return false;
	}


	auto GetKeywordsMatch(RE::TESNPC& a_actorbase, const StringVec& a_strings) -> bool
	{
		return std::any_of(a_strings.begin(), a_strings.end(), [&a_actorbase](const auto& str) {
			return a_actorbase.HasKeyword(str);
		});
	}
}


auto GetINIData(const std::string& a_value) -> INIData
{
	using TYPE = INI_TYPE;
	namespace STRING = SKSE::STRING;

	auto sections = STRING::split(a_value, "|");
	for (auto& section : sections) {
		STRING::trim(section);
	}

	INIData data;
	auto& [formIDPair_ini, strings_ini, filterIDs_ini, level_ini, gender_ini, itemCount_ini, chance_ini] = data;

	//[FORMID/ESP] / string
	std::variant<FormIDPair, std::string> item_ID;
	try {
		auto& formSection = sections.at(TYPE::kFormID);
		if (formSection.find(" - ") != std::string::npos) {
			FormIDPair pair;

			auto formIDpair = STRING::split(formSection, " - ");
			auto espStr = formIDpair.at(TYPE::kESP);

			//FIX FOR MODS WITH "-" CHARACTERS
			auto const size = formIDpair.size();
			if (size > 2) {
				for (size_t i = 2; i < size; i++) {
					espStr += " - " + formIDpair[i];
				}
			}

			pair.first = std::stoul(formIDpair.at(TYPE::kFormID), nullptr, 16);
			pair.second = espStr;

			item_ID.emplace<FormIDPair>(pair);
		} else {
			item_ID.emplace<std::string>(formSection);
		}
	} catch (...) {
		FormIDPair pair = { 0, "Skyrim.esm" };
		item_ID.emplace<FormIDPair>(pair);
	}
	formIDPair_ini = item_ID;

	//KEYWORDS
	try {
		auto& [strings, strings_NOT] = strings_ini;

		auto split_str = UTIL::splitTrimmedString(sections.at(TYPE::kStrings), false);
		for (auto& str : split_str) {
			auto it = str.find("NOT"sv);
			if (it != std::string::npos) {
				str.erase(it, 3);
				strings_NOT.emplace_back(STRING::trim(str));
			} else {
				strings.emplace_back(STRING::trim(str));
			}
		}
	} catch (...) {
	}

	//FILTER FORMS
	try {
		auto& [filterIDs, filterIDs_NOT] = filterIDs_ini;

		auto split_IDs = UTIL::splitTrimmedString(sections.at(TYPE::kFilterIDs), false);
		for (auto& IDs : split_IDs) {
			auto it = IDs.find("NOT"sv);
			if (it != std::string::npos) {
				IDs.erase(it, 3);
				filterIDs_NOT.emplace_back(std::stoul(IDs, nullptr, 16));
			} else {
				filterIDs.emplace_back(std::stoul(IDs, nullptr, 16));
			}
		}
	} catch (...) {
	}

	//LEVEL
	ActorLevel actorLevelPair = { ACTOR_LEVEL::MAX, ACTOR_LEVEL::MAX };
	SkillLevel skillLevelPair = { SKILL_LEVEL::TYPE_MAX, { SKILL_LEVEL::VALUE_MAX, SKILL_LEVEL::VALUE_MAX } };
	try {
		auto split_levels = UTIL::splitTrimmedString(sections.at(TYPE::kLevel), false);
		for (auto& levels : split_levels) {
			if (levels.find('(') != std::string::npos) {
				auto sanitizedLevel = STRING::removeNonAlphaNumeric(levels);
				auto skills = STRING::split(sanitizedLevel, " ");
				if (!skills.empty()) {
					if (skills.size() > 2) {
						auto type = STRING::to_num<std::uint32_t>(skills.at(S_LEVEL::kType));
						auto minLevel = STRING::to_num<std::uint8_t>(skills.at(S_LEVEL::kMin));
						auto maxLevel = STRING::to_num<std::uint8_t>(skills.at(S_LEVEL::kMax));
						skillLevelPair = { type, { minLevel, maxLevel } };
					} else {
						auto type = STRING::to_num<std::uint32_t>(skills.at(S_LEVEL::kType));
						auto minLevel = STRING::to_num<std::uint8_t>(skills.at(S_LEVEL::kMin));
						skillLevelPair = { type, { minLevel, SKILL_LEVEL::VALUE_MAX } };
					}
				}
			} else {
				auto split_level = split(levels, "/");
				if (split_level.size() > 1) {
					auto minLevel = STRING::to_num<std::uint16_t>(split_level.at(A_LEVEL::kMin));
					auto maxLevel = STRING::to_num<std::uint16_t>(split_level.at(A_LEVEL::kMax));
					actorLevelPair = { minLevel, maxLevel };
				} else {
					auto level = STRING::to_num<std::uint16_t>(levels);
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
		if (!genderStr.empty() && genderStr.find("NONE"sv) == std::string::npos) {
			genderStr.erase(remove(genderStr.begin(), genderStr.end(), ' '), genderStr.end());
			if (genderStr == "M") {
				gender_ini = RE::SEX::kMale;
			} else if (genderStr == "F") {
				gender_ini = RE::SEX::kFemale;
			} else {
				gender_ini = static_cast<RE::SEX>(STRING::to_num<std::uint32_t>(sections.at(TYPE::kGender)));
			}
		}
	} catch (...) {
	}

	//ITEMCOUNT
	itemCount_ini = 1;
	try {
		auto itemCountStr = sections.at(TYPE::kItemCount);
		if (!itemCountStr.empty() && itemCountStr.find("NONE"sv) == std::string::npos) {
			itemCount_ini = STRING::to_num<std::uint32_t>(itemCountStr);
		}
	} catch (...) {
	}

	//CHANCE
	chance_ini = 100;
	try {
		auto chanceStr = sections.at(TYPE::kChance);
		if (!chanceStr.empty() && chanceStr.find("NONE"sv) == std::string::npos) {
			chance_ini = STRING::to_num<float>(chanceStr);
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


auto ReadINIs() -> bool
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

		const auto rc = ini.LoadFile(path.c_str());
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
		GetDataFromINI(ini, "Keyword", keywordsINI);
		GetDataFromINI(ini, "DeathItem", deathItemsINI);
	}

	return true;
}


auto LookupAllForms() -> bool
{
	if (const auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler) {
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
		if (!outfitsINI.empty()) {
			LookupForms("Outfit", outfitsINI, outfits);
		}
		if (!keywordsINI.empty()) {
			LookupForms("Keyword", keywordsINI, keywords);
		}
		if (!deathItemsINI.empty()) {
			LookupForms("DeathItem", deathItemsINI, deathItems);
		}
	}
	return !spells.empty() || !perks.empty() || !items.empty() || !shouts.empty() || !levSpells.empty() || !packages.empty() || !outfits.empty() || !keywords.empty() || !deathItems.empty();
}


void ApplyNPCRecords(RE::TESNPC& actorbase)
{
	ForEachForm<RE::BGSKeyword>(actorbase, keywords, [&](const FormCountPair<RE::BGSKeyword> a_keywordsPair) {
		const auto keyword = a_keywordsPair.first;
		if (keyword && !actorbase.HasKeyword(keyword->formEditorID)) {
			return actorbase.AddKeyword(keyword);
		}
		return false;
	});

	ForEachForm<RE::SpellItem>(actorbase, spells, [&](const FormCountPair<RE::SpellItem> a_spellPair) {
		if (const auto spell = a_spellPair.first; spell) {
			auto actorEffects = actorbase.GetOrCreateSpellList();
			return actorEffects && actorEffects->AddSpell(spell);
		}
		return false;
	});

	ForEachForm<RE::BGSPerk>(actorbase, perks, [&](const FormCountPair<RE::BGSPerk> a_perkPair) {
		const auto perk = a_perkPair.first;
		return perk && actorbase.AddPerk(perk);
	});

	ForEachForm<RE::TESShout>(actorbase, shouts, [&](const FormCountPair<RE::TESShout> a_shoutPair) {
		if (const auto shout = a_shoutPair.first; shout) {
			auto actorEffects = actorbase.GetOrCreateSpellList();
			return actorEffects && actorEffects->AddShout(shout);
		}
		return false;
	});

	ForEachForm<RE::TESLevSpell>(actorbase, levSpells, [&](const FormCountPair<RE::TESLevSpell> a_levSpellPair) {
		if (const auto levSpell = a_levSpellPair.first; levSpell) {
			auto actorEffects = actorbase.GetOrCreateSpellList();
			return actorEffects && actorEffects->AddLevSpell(levSpell);
		}
		return false;
	});

	ForEachForm<RE::BGSOutfit>(actorbase, outfits, [&](const FormCountPair<RE::BGSOutfit> a_outfitsPair) {
		if (const auto outfit = a_outfitsPair.first; outfit) {
			actorbase.defaultOutfit = outfit;
			return true;
		}
		return false;
	});

	ForEachForm<RE::TESBoundObject>(actorbase, items, [&](const FormCountPair<RE::TESBoundObject> a_itemPair) {
		const auto& [item, count] = a_itemPair;
		return item && actorbase.AddObjectToContainer(item, count, &actorbase);
	});

	ForEachForm<RE::TESLevItem>(actorbase, deathItems, [&](const FormCountPair<RE::TESLevItem> a_deathItemsPair) {
		if (const auto deathItem = a_deathItemsPair.first; deathItem) {
			actorbase.deathItem = deathItem;
			return true;
		}
		return false;
	});

	ForEachForm<RE::TESPackage>(actorbase, packages, [&](const FormCountPair<RE::TESPackage> a_packagePair) {
		if (const auto package = a_packagePair.first; package) {
			auto& packagesList = actorbase.aiPackages.packages;
			if (std::find(packagesList.begin(), packagesList.end(), package) == packagesList.end()) {
				packagesList.push_front(package);
				auto packageList = actorbase.defaultPackList;
				if (packageList && !packageList->HasForm(package)) {
					packageList->AddForm(package);
				}
				return true;
			}
		}
		return false;
	});
}

void ApplyToUniqueNPCs()
{
	if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler) {
		for (const auto& actorbase : dataHandler->GetFormArray<RE::TESNPC>()) {
			if (actorbase && !actorbase->IsPlayer()) {
				ApplyNPCRecords(*actorbase);
			}
		}

		logger::info("{:*^30}", "RESULT");

		auto const totalNPCs = dataHandler->GetFormArray<RE::TESNPC>().size();
		if (!keywords.empty()) {
			ListNPCCount("Keywords", keywords, totalNPCs);
		}
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
		if (!deathItems.empty()) {
			ListNPCCount("DeathItems", deathItems, totalNPCs);
		}
	}
}


class LeveledActor
{
public:
	static void Hook()
	{
		auto& trampoline = SKSE::GetTrampoline();
		
		REL::Relocation<std::uintptr_t> CalculateHitTargetForWeaponSwing{ REL::ID(14256) };
		_SetInventory = trampoline.write_call<5>(CalculateHitTargetForWeaponSwing.address() + 0x238, SetInventory);

		logger::info("Hooked leveled actor init");
	}

private:
	static void SetInventory(RE::Actor* a_this, bool a_skipExtra)
	{
		if (const auto actorbase = a_this->GetActorBase(); actorbase) {
			if (const auto baseTemplate = actorbase->baseTemplateForm; baseTemplate && baseTemplate->Is(RE::FormType::LeveledNPC)) {
				ForEachForm<RE::BGSOutfit>(*actorbase, outfits, [&](const FormCountPair<RE::BGSOutfit> a_outfitsPair) {
					if (const auto outfit = a_outfitsPair.first; outfit) {
						actorbase->defaultOutfit = outfit;
						return true;
					}
					return false;
				});

				ForEachForm<RE::TESBoundObject>(*actorbase, items, [&](const FormCountPair<RE::TESBoundObject> a_itemPair) {
					const auto& [item, count] = a_itemPair;
					return item && actorbase->AddObjectToContainer(item, count, actorbase);
				});					
			}
		}

		_SetInventory(a_this, a_skipExtra);
	}
	static inline REL::Relocation<decltype(SetInventory)> _SetInventory;
};


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
				logger::info("	Adding {}/{} keywords(s)", keywords.size(), keywordsINI.size());
				logger::info("	Adding {}/{} death item(s)", deathItems.size(), deathItemsINI.size());

				ApplyToUniqueNPCs();
				LeveledActor::Hook();
			}
		}
		break;
	default:
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

		set_default_logger(log);
		spdlog::set_pattern("[%H:%M:%S] [%l] %v");

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
		SKSE::AllocTrampoline(1 << 5);

		if (ReadINIs()) {
			auto messaging = SKSE::GetMessagingInterface();
			if (!messaging->RegisterListener("SKSE", OnInit)) {
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
