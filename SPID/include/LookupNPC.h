#pragma once

namespace NPC
{
	struct Data
	{
		explicit Data(RE::TESNPC* a_npc);

		Data(RE::Actor* a_actor, RE::TESNPC* a_npc);

		[[nodiscard]] RE::TESNPC* GetNPC() const;

		[[nodiscard]] bool HasStringFilter(const StringVec& a_strings, bool a_all = false) const;
		[[nodiscard]] bool ContainsStringFilter(const StringVec& a_strings) const;

		bool InsertKeyword(const char* a_keyword);

		[[nodiscard]] bool HasFormFilter(const FormVec& a_forms, bool all = false) const;

		[[nodiscard]] std::uint16_t GetLevel() const;
		[[nodiscard]] RE::SEX GetSex() const;
		[[nodiscard]] bool IsUnique() const;
		[[nodiscard]] bool IsSummonable() const;
		[[nodiscard]] bool IsChild() const;

	private:
		void cache_keywords();

		[[nodiscard]] bool has_keyword_string(const std::string& a_string) const;
		[[nodiscard]] bool contains_keyword_string(const std::string& a_string) const;

		[[nodiscard]] bool has_form(RE::TESForm* a_form) const;

		RE::TESNPC* npc;
		RE::FormID originalFormID;
		RE::FormID templateFormID{ 0 };
		std::string name;
		std::string originalEDID;
		std::string templateEDID{};
		Set<std::string> keywords{};
		std::uint16_t level;
		RE::SEX sex;
		bool unique;
		bool summonable;
		bool child;
	};
}

using NPCData = NPC::Data;
