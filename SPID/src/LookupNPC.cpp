#include "LookupNPC.h"

namespace NPC
{
	Data::ID::ID(RE::TESActorBase* a_base) :
		formID(a_base->GetFormID()),
		editorID(EditorID::GetEditorID(a_base))
	{}

	bool Data::ID::contains(const std::string& a_str) const
	{
		return string::icontains(editorID, a_str);
	}

	bool Data::ID::operator==(const RE::TESFile* a_mod) const
	{
		return a_mod->IsFormInMod(formID);
	}

	bool Data::ID::operator==(const std::string& a_str) const
	{
		return string::iequals(editorID, a_str);
	}

	bool Data::ID::operator==(RE::FormID a_formID) const
	{
		return formID == a_formID;
	}

	Data::Data(RE::Actor* a_actor, RE::TESNPC* a_npc) :
		npc(a_npc),
		actor(a_actor),
		name(actor->GetName()),
		race(npc->GetRace()),
		level(npc->GetLevel()),
		sex(npc->GetSex()),
		unique(npc->IsUnique()),
		summonable(npc->IsSummonable()),
		child(actor->IsChild() || race->formEditorID.contains("RaceChild"))
	{
		npc->ForEachKeyword([&](const RE::BGSKeyword& a_keyword) {
			keywords.emplace(a_keyword.GetFormEditorID());
			return RE::BSContainer::ForEachResult::kContinue;
		});
		race->ForEachKeyword([&](const RE::BGSKeyword& a_keyword) {
			keywords.emplace(a_keyword.GetFormEditorID());
			return RE::BSContainer::ForEachResult::kContinue;
		});
		if (const auto extraLvlCreature = actor->extraList.GetByType<RE::ExtraLeveledCreature>()) {
			if (const auto originalBase = extraLvlCreature->originalBase) {
				originalIDs = ID(originalBase);
			}
			if (const auto templateBase = extraLvlCreature->templateBase) {
				templateIDs = ID(templateBase);
				if (const auto templateRace = templateBase->As<RE::TESNPC>()->GetRace()) {
					race = templateRace;
				}
			}
		} else {
			originalIDs = ID(npc);
		}
		if (!potentialFollowerFaction) {
			potentialFollowerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0005C84D);
		}
		teammate = actor->IsPlayerTeammate() || npc->IsInFaction(potentialFollowerFaction);
	}

	RE::TESNPC* Data::GetNPC() const
	{
		return npc;
	}

	RE::Actor* Data::GetActor() const
	{
		return actor;
	}

	bool Data::has_keyword_string(const std::string& a_string) const
	{
		return std::any_of(keywords.begin(), keywords.end(), [&](const auto& keyword) {
			return string::iequals(keyword, a_string);
		});
	}

	bool Data::HasStringFilter(const StringVec& a_strings, bool a_all) const
	{
		if (a_all) {
			return std::ranges::all_of(a_strings, [&](const auto& str) {
				return has_keyword_string(str) || string::iequals(name, str) || originalIDs == str || templateIDs == str;
			});
		} else {
			return std::ranges::any_of(a_strings, [&](const auto& str) {
				return has_keyword_string(str) || string::iequals(name, str) || originalIDs == str || templateIDs == str;
			});
		}
	}

	bool Data::ContainsStringFilter(const StringVec& a_strings) const
	{
		return std::ranges::any_of(a_strings, [&](const auto& str) {
			return string::icontains(name, str) ||
			       originalIDs.contains(str) ||
			       templateIDs.contains(str) ||
			       std::any_of(keywords.begin(), keywords.end(), [&](const auto& keyword) {
					   return string::icontains(keyword, str);
				   });
		});
	}

	bool Data::InsertKeyword(const char* a_keyword)
	{
		return keywords.emplace(a_keyword).second;
	}

	bool Data::has_form(RE::TESForm* a_form) const
	{
		switch (a_form->GetFormType()) {
		case RE::FormType::CombatStyle:
			return npc->GetCombatStyle() == a_form;
		case RE::FormType::Class:
			return npc->npcClass == a_form;
		case RE::FormType::Faction:
			{
				const auto faction = a_form->As<RE::TESFaction>();
				return npc->IsInFaction(faction);
			}
		case RE::FormType::Race:
			return GetRace() == a_form;
		case RE::FormType::Outfit:
			return npc->defaultOutfit == a_form;
		case RE::FormType::NPC:
			{
				const auto filterFormID = a_form->GetFormID();
				return npc == a_form || originalIDs == filterFormID || templateIDs == filterFormID;
			}
		case RE::FormType::VoiceType:
			return npc->voiceType == a_form;
		case RE::FormType::Spell:
			{
				const auto spell = a_form->As<RE::SpellItem>();
				return npc->GetSpellList()->GetIndex(spell).has_value();
			}
		case RE::FormType::Armor:
			return npc->skin == a_form;
		case RE::FormType::Location:
			{
				const auto location = a_form->As<RE::BGSLocation>();
				return actor->GetEditorLocation() == location;
			}
		case RE::FormType::FormList:
			{
				bool result = false;

				const auto list = a_form->As<RE::BGSListForm>();
				list->ForEachForm([&](RE::TESForm& a_formInList) {
					if (result = has_form(&a_formInList); result) {
						return RE::BSContainer::ForEachResult::kStop;
					}
					return RE::BSContainer::ForEachResult::kContinue;
				});

				return result;
			}
		default:
			return false;
		}
	}

	bool Data::HasFormFilter(const FormVec& a_forms, bool all) const
	{
		const auto has_form_or_file = [&](const std::variant<RE::TESForm*, const RE::TESFile*>& a_formFile) {
			if (std::holds_alternative<RE::TESForm*>(a_formFile)) {
				const auto form = std::get<RE::TESForm*>(a_formFile);
				return form && has_form(form);
			}
			if (std::holds_alternative<const RE::TESFile*>(a_formFile)) {
				const auto file = std::get<const RE::TESFile*>(a_formFile);
				return file && (originalIDs == file || templateIDs == file);
			}
			return false;
		};

		if (all) {
			return std::ranges::all_of(a_forms, has_form_or_file);
		} else {
			return std::ranges::any_of(a_forms, has_form_or_file);
		}
	}

	std::uint16_t Data::GetLevel() const
	{
		return level;
	}

	RE::SEX Data::GetSex() const
	{
		return sex;
	}

	bool Data::IsUnique() const
	{
		return unique;
	}

	bool Data::IsSummonable() const
	{
		return summonable;
	}

	bool Data::IsChild() const
	{
		return child;
	}

	bool Data::IsLeveled() const
	{
		return templateIDs.formID != 0;
	}

	bool Data::IsTeammate() const
	{
		return teammate;
	}

	RE::TESRace* Data::GetRace() const
	{
		return race;
	}
}
