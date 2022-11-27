#include "LookupNPC.h"

namespace NPC
{
	Data::Data(RE::TESNPC* a_npc) :
		npc(a_npc),
		originalFormID(a_npc->GetFormID()),
		originalEDID(Cache::EditorID::GetEditorID(npc->GetFormID()))
	{}

	Data::Data(RE::Actor* a_actor, RE::TESNPC* a_npc) :
		npc(a_npc)
	{
		if (const auto extraLvlCreature = a_actor->extraList.GetByType<RE::ExtraLeveledCreature>()) {
			if (const auto originalBase = extraLvlCreature->originalBase) {
				originalFormID = originalBase->GetFormID();
				originalEDID = Cache::EditorID::GetEditorID(originalBase->GetFormID());
			}
			if (const auto templateBase = extraLvlCreature->templateBase) {
				templateFormID = templateBase->GetFormID();
				templateEDID = Cache::EditorID::GetEditorID(templateBase->GetFormID());
			}
		} else {
			originalFormID = a_npc->GetFormID();
			originalEDID = Cache::EditorID::GetEditorID(npc->GetFormID());
		}
	}

	RE::TESNPC* Data::GetNPC() const
	{
		return npc;
	}

	std::pair<std::string, std::string> Data::GetEditorID() const
	{
		return { originalEDID, templateEDID };
	}

	std::pair<RE::FormID, RE::FormID> Data::GetFormID() const
	{
		return { originalFormID, templateFormID };
	}
}
