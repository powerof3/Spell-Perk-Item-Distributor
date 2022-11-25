#pragma once

namespace NPC
{
	struct Data
	{
		explicit Data(RE::TESNPC* a_npc);

		Data(RE::Actor* a_actor, RE::TESNPC* a_npc);

		[[nodiscard]] RE::TESNPC* GetNPC() const;

		[[nodiscard]] RE::FormID GetFormID() const;
		[[nodiscard]] std::uint16_t GetLevel() const;
		[[nodiscard]] RE::SEX GetSex() const;
		[[nodiscard]] bool IsUnique() const;
		[[nodiscard]] bool IsSummonable() const;
		[[nodiscard]] bool IsChild() const;

		[[nodiscard]] bool HasStringFilter(const StringVec& a_strings) const;
		[[nodiscard]] bool ContainsStringFilter(const StringVec& a_strings) const;
		[[nodiscard]] bool HasAllKeywords(const StringVec& a_strings) const;

	private:
		[[nodiscard]] void cache_keywords(RE::TESNPC* a_npc);
		[[nodiscard]] bool has_keyword(const std::string& a_string) const;
		[[nodiscard]] bool contains_keyword(const std::string& a_string) const;

		RE::TESNPC* npc;
		RE::FormID formID;
		std::string name;
		std::string originalEDID;
		std::string templateEDID;
		Set<std::string> keywords;
		std::uint16_t level;
		RE::SEX sex;
		bool unique;
		bool summonable;
		bool child;
	};
}

using NPCData = NPC::Data;
