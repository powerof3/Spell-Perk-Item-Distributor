#include "LookupNPC.h"

// ID
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
}

// Data
namespace NPC
{
	void Data::cache_keywords()
	{
		npc->ForEachKeyword([&](const RE::BGSKeyword& a_keyword) {
			keywords.emplace(a_keyword.formID);
			return RE::BSContainer::ForEachResult::kContinue;
		});
		race->ForEachKeyword([&](const RE::BGSKeyword& a_keyword) {
			keywords.emplace(a_keyword.formID);
			return RE::BSContainer::ForEachResult::kContinue;
		});
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
		cache_keywords();
	}

	RE::TESNPC* Data::GetNPC() const
	{
		return npc;
	}

	RE::Actor* Data::GetActor() const
	{
		return actor;
	}

	std::string Data::GetName() const
	{
		return name;
	}

	RE::TESRace* Data::GetRace() const
	{
		return race;
	}

	std::string Data::GetOriginalEDID() const
	{
		return originalIDs.editorID;
	}

	std::string Data::GetTemplateEDID() const
	{
		return templateIDs.editorID;
	}

	RE::FormID Data::GetOriginalFormID() const
	{
		return originalIDs.formID;
	}

	RE::FormID Data::GetTemplateFormID() const
	{
		return templateIDs.formID;
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

	bool Data::HasKeyword(const RE::BGSKeyword* kwd) const
	{
		return kwd && keywords.contains(kwd->formID);
	}

	bool Data::InsertKeyword(const RE::BGSKeyword* kwd)
	{
		return kwd && keywords.emplace(kwd->formID).second;
	}
}
