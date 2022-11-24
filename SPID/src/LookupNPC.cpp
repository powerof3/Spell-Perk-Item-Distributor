#include "LookupNPC.h"

namespace NPC
{
	Data::Data(RE::TESNPC* a_npc) :
		npc(a_npc),
		formID(a_npc->GetFormID()),
		name(a_npc->GetName()),
		originalEDID(Cache::EditorID::GetEditorID(npc)),
		level(a_npc->GetLevel()),
		child(a_npc->GetRace() ? a_npc->GetRace()->IsChildRace() : false)
	{}

	Data::Data(RE::Actor* a_actor, RE::TESNPC* a_npc) :
		npc(a_npc),
		name(a_npc->GetName()),
		level(a_npc->GetLevel()),
		child(a_npc->GetRace() ? a_npc->GetRace()->IsChildRace() : false)
	{
		if (const auto extraLvlCreature = a_actor->extraList.GetByType<RE::ExtraLeveledCreature>()) {
			if (const auto originalBase = extraLvlCreature->originalBase) {
				originalEDID = Cache::EditorID::GetEditorID(originalBase);
			}
			if (const auto templateBase = extraLvlCreature->templateBase) {
				formID = templateBase->GetFormID();
				templateEDID = Cache::EditorID::GetEditorID(templateBase);
			}
		} else {
			formID = a_npc->GetFormID();
			originalEDID = Cache::EditorID::GetEditorID(npc);
		}
	}

	RE::TESNPC* Data::GetNPC() const
	{
		return npc;
	}

	RE::FormID Data::GetFormID() const
	{
		return formID;
	}

	std::string Data::GetName() const
	{
		return name;
	}

	std::pair<std::string, std::string> Data::GetEditorID() const
	{
		return { originalEDID, templateEDID };
	}

	std::uint16_t Data::GetLevel() const
	{
		return level;
	}

	bool Data::IsChild() const
	{
		return child;
	}
}
