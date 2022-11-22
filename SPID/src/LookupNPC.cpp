#include "LookupNPC.h"

namespace NPC
{
	Data::Data(RE::TESNPC* a_npc) :
		npc(a_npc)
	{}

	Data::Data(RE::Actor* a_actor, RE::TESNPC* a_npc) :
		npc(a_npc)
	{
		if (const auto extraLvlCreature = a_actor ? a_actor->extraList.GetByType<RE::ExtraLeveledCreature>() : nullptr) {
			if (extraLvlCreature->originalBase) {
				originalBase = extraLvlCreature->originalBase;
			}
			if (extraLvlCreature->templateBase) {
				templateBase = extraLvlCreature->templateBase;
			}
		}
	}

	RE::TESNPC* Data::GetNPC() const
	{
		return npc;
	}

	std::string Data::GetName() const
	{
		return npc->GetName();
	}

	RE::FormID Data::GetFormID() const
	{
		return originalBase ? originalBase->GetFormID() : npc->GetFormID();
	}

	std::pair<std::string, std::string> Data::GetEditorID() const
	{
		if (!originalBase || !templateBase) {
			return { Cache::EditorID::GetEditorID(npc), std::string() };
		}

		return { Cache::EditorID::GetEditorID(originalBase), Cache::EditorID::GetEditorID(templateBase) };
	}
}
