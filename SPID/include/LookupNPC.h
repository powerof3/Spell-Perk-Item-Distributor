#pragma once

namespace NPC
{
	struct Data
	{
		Data(RE::Actor* a_actor, RE::TESNPC* a_npc);

	    bool InsertKeyword(const char* a_keyword);

		[[nodiscard]] RE::TESNPC* GetNPC() const;

		[[nodiscard]] StringSet   GetKeywords() const;
		[[nodiscard]] std::string GetName() const;
		[[nodiscard]] std::string GetOriginalEDID() const;
		[[nodiscard]] std::string GetTemplateEDID() const;

		[[nodiscard]] RE::FormID GetOriginalFormID() const;
		[[nodiscard]] RE::FormID GetTemplateFormID() const;

		[[nodiscard]] std::uint16_t GetLevel() const;
		[[nodiscard]] RE::SEX       GetSex() const;
		[[nodiscard]] bool          IsUnique() const;
		[[nodiscard]] bool          IsSummonable() const;
		[[nodiscard]] bool          IsChild() const;

		[[nodiscard]] RE::TESRace* GetRace() const;

	private:
		void cache_keywords();
		void set_as_child();

		RE::TESNPC* npc;
		RE::Actor*   actor;
		RE::FormID  originalFormID;
		RE::FormID  templateFormID{ 0 };
		std::string name;
		RE::TESRace* race;
		std::string originalEDID;
		std::string templateEDID{};
		StringSet   keywords{};


		RE::TESNPC*   npc;
		RE::Actor*    actor;
		std::string   name;
		RE::TESRace*  race;
		RE::FormID    originalFormID;
		std::string   originalEDID;
		RE::FormID    templateFormID{ 0 };
		std::string   templateEDID{};
		StringSet     keywords{};

		std::uint16_t level;
		RE::SEX       sex;
		bool          unique;
		bool          summonable;
		bool          child;
	};
}

using NPCData = NPC::Data;
