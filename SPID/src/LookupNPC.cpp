#include "LookupNPC.h"

namespace NPC
{
	void Data::cache_keywords()
	{
		npc->ForEachKeyword([&](const RE::BGSKeyword& a_keyword) {
			keywords.emplace(a_keyword.GetFormEditorID());
			return RE::BSContainer::ForEachResult::kContinue;
		});
		race->ForEachKeyword([&](const RE::BGSKeyword& a_keyword) {
			keywords.emplace(a_keyword.GetFormEditorID());
			return RE::BSContainer::ForEachResult::kContinue;
		});
	}

	void Data::set_as_child()
	{
		child = false;

		if (actor->IsChild()) {
			child = true;
		} else if (race->IsChildRace()) {
			child = true;
		} else if (race->formEditorID.contains("RaceChild")) {
			child = true;
		}
	}

	Data::Data(RE::Actor* a_actor, RE::TESNPC* a_npc) :
		npc(a_npc),
		actor(a_actor),
		name(a_actor->GetName()),
		level(a_npc->GetLevel()),
		sex(a_npc->GetSex()),
		unique(a_npc->IsUnique()),
		summonable(a_npc->IsSummonable())
	{
		race = a_npc->GetRace();
		if (const auto extraLvlCreature = a_actor->extraList.GetByType<RE::ExtraLeveledCreature>()) {
			if (const auto originalBase = extraLvlCreature->originalBase) {
				originalFormID = originalBase->GetFormID();
				originalEDID = Cache::EditorID::GetEditorID(originalBase);
			}
			if (const auto templateBase = extraLvlCreature->templateBase) {
				templateFormID = templateBase->GetFormID();
				templateEDID = Cache::EditorID::GetEditorID(templateBase);
				if (auto templateRace = templateBase->As<RE::TESNPC>()->GetRace()) {
					race = templateRace;
				}
			}
		} else {
			originalFormID = a_npc->GetFormID();
			originalEDID = Cache::EditorID::GetEditorID(npc);
		}
		cache_keywords();
		set_as_child();
	}

	bool Data::InsertKeyword(const char* a_keyword)
	{
		return keywords.emplace(a_keyword).second;
	}

	RE::TESNPC* Data::GetNPC() const
	{
		return npc;
	}

	StringSet Data::GetKeywords() const
	{
		return keywords;
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
		return originalEDID;
	}
	std::string Data::GetTemplateEDID() const
	{
		return templateEDID;
	}

	RE::FormID Data::GetOriginalFormID() const
	{
		return originalFormID;
	}

	RE::FormID Data::GetTemplateFormID() const
	{
		return templateFormID;
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
}
