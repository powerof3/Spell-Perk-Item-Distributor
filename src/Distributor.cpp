#include "Distributor.h"

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
static FormDataVec<RE::TESShout> shouts;
static FormDataVec<RE::TESLevSpell> levSpells;
static FormDataVec<RE::TESPackage> packages;
static FormDataVec<RE::BGSOutfit> outfits;
static FormDataVec<RE::BGSKeyword> keywords;
static FormDataVec<RE::TESBoundObject> deathItems;

bool INI::Read()
{
	logger::info("{:*^30}", "INI");

	std::vector<std::string> configs;

	auto constexpr folder = R"(Data\)";
	for (const auto& entry : std::filesystem::directory_iterator(folder)) {
		if (entry.exists() && entry.path().extension() == ".ini"sv) {
			if (const auto path = entry.path().string(); path.find("_DISTR") != std::string::npos) {
				configs.push_back(path);
			}
		}
	}

	if (configs.empty()) {
		logger::warn("	No .ini files with _DISTR suffix were found within the Data folder, aborting...");
		return false;
	}

	logger::info("	{} matching inis found", configs.size());

	for (auto& path : configs) {
		logger::info("	ini : {}", path);

		CSimpleIniA ini;
		ini.SetUnicode();
		ini.SetMultiKey();

		if (const auto rc = ini.LoadFile(path.c_str()); rc < 0) {
			logger::error("	couldn't read path");
			continue;
		}

		get_ini_data(ini, "Spell", spellsINI);
		get_ini_data(ini, "Perk", perksINI);
		get_ini_data(ini, "Item", itemsINI);
		get_ini_data(ini, "Shout", shoutsINI);
		get_ini_data(ini, "LevSpell", levSpellsINI);
		get_ini_data(ini, "Package", packagesINI);
		get_ini_data(ini, "Outfit", outfitsINI);
		get_ini_data(ini, "Keyword", keywordsINI);
		get_ini_data(ini, "DeathItem", deathItemsINI);
	}
	return true;
}

bool Lookup::GetForms()
{
	if (const auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler) {
		logger::info("{:*^30}", "LOOKUP");

		if (!spellsINI.empty()) {
			Lookup::get_forms("Spell", spellsINI, spells);
		}
		if (!perksINI.empty()) {
			get_forms("Perk", perksINI, perks);
		}
		if (!itemsINI.empty()) {
			get_forms("Item", itemsINI, items);
		}
		if (!shoutsINI.empty()) {
			get_forms("Shout", shoutsINI, shouts);
		}
		if (!levSpellsINI.empty()) {
			get_forms("LevSpell", levSpellsINI, levSpells);
		}
		if (!packagesINI.empty()) {
			get_forms("Package", packagesINI, packages);
		}
		if (!outfitsINI.empty()) {
			get_forms("Outfit", outfitsINI, outfits);
		}
		if (!keywordsINI.empty()) {
			get_forms("Keyword", keywordsINI, keywords);
		}
		if (!deathItemsINI.empty()) {
			get_forms("DeathItem", deathItemsINI, deathItems);
		}
	}

	const auto result = !spells.empty() || !perks.empty() || !items.empty() || !shouts.empty() || !levSpells.empty() || !packages.empty() || !outfits.empty() || !keywords.empty() || !deathItems.empty();

	if (result) {
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
	}
	return result;
}

void Distribute::ApplyToNPCs()
{
	if (const auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler) {
		constexpr auto apply_npc_records = [](RE::TESNPC& a_actorbase) {
			for_each_form<RE::BGSKeyword>(a_actorbase, keywords, [&](const FormCountPair<RE::BGSKeyword>& a_keywordsPair) {
				const auto keyword = a_keywordsPair.first;
				if (!a_actorbase.HasKeyword(keyword->formEditorID)) {
					return a_actorbase.AddKeyword(keyword);
				}
				return false;
			});

			for_each_form<RE::SpellItem>(a_actorbase, spells, [&](const FormCountPair<RE::SpellItem>& a_spellPair) {
				const auto spell = a_spellPair.first;
				const auto actorEffects = a_actorbase.GetSpellList();
				return actorEffects && actorEffects->AddSpell(spell);
			});

			for_each_form<RE::BGSPerk>(a_actorbase, perks, [&](const FormCountPair<RE::BGSPerk>& a_perkPair) {
				const auto perk = a_perkPair.first;
				return a_actorbase.AddPerk(perk, 1);
			});

			for_each_form<RE::TESShout>(a_actorbase, shouts, [&](const FormCountPair<RE::TESShout>& a_shoutPair) {
				const auto shout = a_shoutPair.first;
				const auto actorEffects = a_actorbase.GetSpellList();
				return actorEffects && actorEffects->AddShout(shout);
			});

			for_each_form<RE::TESLevSpell>(a_actorbase, levSpells, [&](const FormCountPair<RE::TESLevSpell>& a_levSpellPair) {
				const auto levSpell = a_levSpellPair.first;
				const auto actorEffects = a_actorbase.GetSpellList();
				return actorEffects && actorEffects->AddLevSpell(levSpell);
			});

			for_each_form<RE::BGSOutfit>(a_actorbase, outfits, [&](const FormCountPair<RE::BGSOutfit>& a_outfitsPair) {
				const auto outfit = a_outfitsPair.first;
				const auto& baseOutfit = a_actorbase.defaultOutfit;
				if (!baseOutfit || !baseOutfit->IsDynamicForm()) {
					const auto factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSOutfit>();
					if (const auto newOutfit = factory ? factory->Create() : nullptr; newOutfit) {
						newOutfit->outfitItems = outfit->outfitItems;
						a_actorbase.defaultOutfit = newOutfit;
						return true;
					}
				} else if (baseOutfit && baseOutfit->IsDynamicForm()) {
					std::ranges::copy(outfit->outfitItems, std::back_inserter(baseOutfit->outfitItems));
					return true;
				}
				return false;
			});

			for_each_form<RE::TESBoundObject>(a_actorbase, items, [&](const FormCountPair<RE::TESBoundObject>& a_itemPair) {
				const auto& [item, count] = a_itemPair;
				return a_actorbase.AddObjectToContainer(item, count, &a_actorbase);
			});

			for_each_form<RE::TESPackage>(a_actorbase, packages, [&](const FormCountPair<RE::TESPackage>& a_packagePair) {
				const auto package = a_packagePair.first;
				auto& packagesList = a_actorbase.aiPackages.packages;
				if (std::ranges::find(packagesList, package) == packagesList.end()) {
					packagesList.push_front(package);
					const auto packageList = a_actorbase.defaultPackList;
					if (packageList && !packageList->HasForm(package)) {
						packageList->AddForm(package);
						return true;
					}
				}
				return false;
			});
		};

		for (const auto& actorbase : dataHandler->GetFormArray<RE::TESNPC>()) {
			if (actorbase && !actorbase->IsPlayer()) {
				apply_npc_records(*actorbase);
			}
		}

		logger::info("{:*^30}", "RESULT");

		auto const totalNPCs = dataHandler->GetFormArray<RE::TESNPC>().size();
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
	}
}

namespace Distribute::Hook
{
	struct TESInit
	{
		static void thunk(RE::Main* a_main, RE::NiAVObject* a_worldSceneGraph)
		{
			func(a_main, a_worldSceneGraph);

			if (Lookup::GetForms()) {
				ApplyToNPCs();
				Leveled::Hook();
				DeathItem::DeathManager::Register();
			}
		}
		static inline REL::Relocation<decltype(thunk)>(func);
	};

	void Install()
	{
		logger::info("{:*^30}", "START");

		logger::info("	Hooking TESInit");

		REL::Relocation<std::uintptr_t> target{ REL::ID(35631), 0x17 };
		stl::write_thunk_call<TESInit>(target.address());
	}
}

namespace Distribute::Leveled
{
	struct SetObjectReference
	{
		static void thunk(RE::Character* a_this, RE::TESBoundObject* a_base)
		{
			func(a_this, a_base);

			const auto actorbase = a_this->GetActorBase();
			const auto baseTemplate = actorbase ? actorbase->baseTemplateForm : nullptr;

			if (baseTemplate && baseTemplate->Is(RE::FormType::LeveledNPC)) {
				for_each_form<RE::TESBoundObject>(*actorbase, items, [&](const FormCountPair<RE::TESBoundObject> a_itemPair) {
					const auto& [item, count] = a_itemPair;
					if (const auto itemCount = actorbase->CountObjectsInContainer(item); itemCount < count) {
						return item && actorbase->AddObjectToContainer(item, count, actorbase);
					}
					return false;
				});
				for_each_form<RE::BGSOutfit>(*actorbase, outfits, [&](const FormCountPair<RE::BGSOutfit>& a_outfitsPair) {
					const auto outfit = a_outfitsPair.first;
					const auto& defaultOutfit = actorbase->defaultOutfit;
					if (!defaultOutfit || !defaultOutfit->IsDynamicForm()) {
						const auto factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSOutfit>();
						if (const auto newOutfit = factory ? factory->Create() : nullptr; newOutfit) {
							newOutfit->outfitItems = outfit->outfitItems;
							actorbase->defaultOutfit = newOutfit;
							return true;
						}
					} else if (defaultOutfit && defaultOutfit->IsDynamicForm()) {
						std::ranges::copy(outfit->outfitItems, std::back_inserter(defaultOutfit->outfitItems));
						return true;
					}
					return false;
				});
			}
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	void Hook()
	{
		if (items.empty() && outfits.empty()) {
			return;
		}

		stl::write_vfunc<RE::Character, 0x084, SetObjectReference>();
		logger::info("	Hooked leveled actor init");
	}
}

namespace Distribute::DeathItem
{
	void DeathManager::Register()
	{
		if (deathItems.empty()) {
			return;
		}

		auto scripts = RE::ScriptEventSourceHolder::GetSingleton();
		if (scripts) {
			scripts->AddEventSink(GetSingleton());
			logger::info("	Registered {}"sv, typeid(DeathManager).name());
		}
	}

	DeathManager::EventResult DeathManager::ProcessEvent(const RE::TESDeathEvent* a_event, RE::BSTEventSource<RE::TESDeathEvent>*)
	{
		constexpr auto isNPC = [](auto&& a_ref) {
			return a_ref && !a_ref->IsPlayerRef();
		};

		if (a_event && a_event->dead && isNPC(a_event->actorDying)) {
			const auto actor = a_event->actorDying->As<RE::Actor>();
			const auto base = actor ? actor->GetActorBase() : nullptr;
			if (actor && base) {
				for_each_form<RE::TESBoundObject>(*base, deathItems, [&](const FormCountPair<RE::TESBoundObject> a_deathItemPair) {
					const auto& [deathItem, count] = a_deathItemPair;
					detail::add_item(actor, deathItem, count, true, 0, RE::BSScript::Internal::VirtualMachine::GetSingleton());
					return true;
				});
			}
		}

		return EventResult::kContinue;
	}
}
