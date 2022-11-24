#pragma once

namespace NPC
{
	struct Data
	{
		explicit Data(RE::TESNPC* a_npc);

		Data(RE::Actor* a_actor, RE::TESNPC* a_npc);

		[[nodiscard]] RE::TESNPC* GetNPC() const;

		[[nodiscard]] RE::FormID GetFormID() const;
		[[nodiscard]] std::string GetName() const;
		[[nodiscard]] std::pair<std::string, std::string> GetEditorID() const;
		[[nodiscard]] std::uint16_t GetLevel() const;
		[[nodiscard]] bool IsChild() const;

	private:
		RE::TESNPC* npc;
		RE::FormID formID;
		std::string name;
		std::string originalEDID;
		std::string templateEDID;
		std::uint16_t level;
		bool child;
	};
}

using NPCData = NPC::Data;
