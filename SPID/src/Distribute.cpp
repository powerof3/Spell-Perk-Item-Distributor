#include "Distribute.h"
#include "LookupForms.h"

void Distribute::Distribute(RE::TESNPC* a_actorbase)
{
    for_each_form<RE::BGSKeyword>(*a_actorbase, Forms::keywords, [&](const auto& a_keywordsPair) {
		const auto keyword = a_keywordsPair.first;
		if (!a_actorbase->HasKeywordString(keyword->formEditorID)) {
			return a_actorbase->AddKeyword(keyword);
		}
		return false;
	});

	for_each_form<RE::TESFaction>(*a_actorbase, Forms::factions, [&](const auto& a_factionPair) {
		if (!a_actorbase->IsInFaction(a_factionPair.first)) {
            const RE::FACTION_RANK faction{ a_factionPair.first, 1 };
			a_actorbase->factions.push_back(faction);
			return true;
		}
		return false;
	});

	for_each_form<RE::SpellItem>(*a_actorbase, Forms::spells, [&](const auto& a_spellPair) {
		const auto spell = a_spellPair.first;
		const auto actorEffects = a_actorbase->GetSpellList();
		return actorEffects && actorEffects->AddSpell(spell);
	});

	for_each_form<RE::BGSPerk>(*a_actorbase, Forms::perks, [&](const auto& a_perkPair) {
		const auto perk = a_perkPair.first;
		return a_actorbase->AddPerk(perk, 1);
	});

	for_each_form<RE::TESShout>(*a_actorbase, Forms::shouts, [&](const auto& a_shoutPair) {
		const auto shout = a_shoutPair.first;
		const auto actorEffects = a_actorbase->GetSpellList();
		return actorEffects && actorEffects->AddShout(shout);
	});

	for_each_form<RE::TESLevSpell>(*a_actorbase, Forms::levSpells, [&](const auto& a_levSpellPair) {
		const auto levSpell = a_levSpellPair.first;
		const auto actorEffects = a_actorbase->GetSpellList();
		return actorEffects && actorEffects->AddLevSpell(levSpell);
	});

	for_each_form<RE::TESBoundObject>(*a_actorbase, Forms::items, [&](const auto& a_itemPair) {
		const auto& [item, count] = a_itemPair;
		return a_actorbase->AddObjectToContainer(item, count, a_actorbase);
	});

	for_each_form<RE::BGSOutfit>(*a_actorbase, Forms::outfits, [&](const auto& a_outfitPair) {
		a_actorbase->defaultOutfit = a_outfitPair.first;
		return true;
	});

	for_each_form<RE::TESPackage>(*a_actorbase, Forms::packages, [&](const auto& a_packagePair) {
		const auto package = a_packagePair.first;
		auto& packagesList = a_actorbase->aiPackages.packages;
		if (std::ranges::find(packagesList, package) == packagesList.end()) {
			packagesList.push_front(package);
			const auto packageList = a_actorbase->defaultPackList;
			if (packageList && !packageList->HasForm(package)) {
				packageList->AddForm(package);
				return true;
			}
		}
		return false;
	});
}

void Distribute::ApplyToNPCs()
{
	if (const auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler) {
		std::size_t totalNPCs = 0;
		for (const auto& actorbase : dataHandler->GetFormArray<RE::TESNPC>()) {
			if (actorbase && !actorbase->IsPlayer() && !actorbase->UsesTemplate()) {
				Distribute(actorbase);
				totalNPCs++;
			}
		}

		logger::info("{:*^30}", "RESULT");

		const auto list_result = [&totalNPCs]<class Form>(const std::string& a_formType, Forms::FormMap<Form>& a_forms) {
			if (a_forms) {
				list_npc_count(a_formType, a_forms, totalNPCs);
			}
		};

		list_result("Keywords", Forms::keywords);
		list_result("Spells", Forms::spells);
		list_result("Perks", Forms::perks);
		list_result("Items", Forms::items);
		list_result("Shouts", Forms::shouts);
		list_result("LevSpells", Forms::levSpells);
		list_result("Packages", Forms::packages);
		list_result("Outfits", Forms::outfits);
		list_result("Factions", Forms::factions);
	}
}

namespace Distribute
{
	void DeathItemManager::Register()
	{
		if (!Forms::deathItems) {
			return;
		}

        if (auto scripts = RE::ScriptEventSourceHolder::GetSingleton()) {
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
				for_each_form<RE::TESBoundObject>(*base, Forms::deathItems, [&](const auto& a_deathItemPair) {
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
	struct CopyFromTemplateForms
	{
		static void thunk(RE::TESActorBaseData* a_this, RE::TESActorBase** a_templateForms)
		{
			func(a_this, a_templateForms);

			if (!a_this->baseTemplateForm || !a_templateForms) {
				return;
			}

			if (const auto actorbase = stl::adjust_pointer<RE::TESNPC>(a_this, -0x30); actorbase) {
				Distribute(actorbase);
			}
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static inline size_t index{ 1 };
		static inline size_t size{ 0x4 };
	};

	void Install()
	{
		stl::write_vfunc<RE::TESNPC, CopyFromTemplateForms>();
		logger::info("	Hooked leveled actor init");
	}
}
