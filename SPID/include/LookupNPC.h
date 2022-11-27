#pragma once

namespace NPC
{
	struct Data
	{
		explicit Data(RE::TESNPC* a_npc);

		Data(RE::Actor* a_actor, RE::TESNPC* a_npc);

		[[nodiscard]] RE::TESNPC* GetNPC() const;
		std::pair<std::string, std::string> GetEditorID() const;
		std::pair<RE::FormID, RE::FormID> GetFormID() const;

	private:
		RE::TESNPC* npc;
		RE::FormID originalFormID;
		RE::FormID templateFormID{0};
		std::string originalEDID;
		std::string templateEDID{};
	};
}

using NPCData = NPC::Data;
