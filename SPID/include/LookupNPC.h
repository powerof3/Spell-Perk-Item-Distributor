#pragma once

namespace NPC
{
	struct Data
	{
		explicit Data(RE::TESNPC* a_npc);

		Data(RE::Actor* a_actor, RE::TESNPC* a_npc);

		[[nodiscard]] RE::TESNPC* GetNPC() const;
		[[nodiscard]] std::string GetName() const;
		[[nodiscard]] RE::FormID GetFormID() const;
		[[nodiscard]] std::pair<std::string, std::string> GetEditorID() const;

	private:
		// final generated NPC
		RE::TESNPC* npc;
		// base placed in world
		RE::TESActorBase* originalBase{ nullptr };
		// base resolved at runtime
		RE::TESActorBase* templateBase{ nullptr };
	};
}

using NPCData = NPC::Data;
