#pragma once

namespace NPC
{
	struct Data
	{
		explicit Data(RE::TESNPC* a_npc);

		Data(RE::Actor* a_actor, RE::TESNPC* a_npc);

		[[nodiscard]] RE::TESNPC* GetNPC() const;

		[[nodiscard]] bool HasStringFilter(const StringVec& a_strings, bool all = false) const;
		[[nodiscard]] bool ContainsStringFilter(const StringVec& a_strings) const;

		bool InsertKeyword(std::string_view a_keyword);

		[[nodiscard]] bool HasFormFilter(const FormVec& a_forms, bool all = false) const;

		[[nodiscard]] std::uint16_t GetLevel() const;
		[[nodiscard]] RE::SEX GetSex() const;
		[[nodiscard]] bool IsUnique() const;
		[[nodiscard]] bool IsSummonable() const;
		[[nodiscard]] bool IsChild() const;

	private:
		struct kywd_cmp
		{
			bool operator()(const std::string& a_lhs, const std::string& a_rhs) const
			{
				return _stricmp(a_lhs.c_str(), a_rhs.c_str()) < 0;
			}
		};

		void cache_keywords();
		[[nodiscard]] bool contains_keyword_string(const std::string& a_string) const;

		[[nodiscard]] bool has_form(RE::TESForm* a_form) const;

		RE::TESNPC* npc;
		RE::FormID formID;
		std::string name;
		std::string originalEDID;
		std::string templateEDID;
		CustomSet<std::string, kywd_cmp> keywords{};
		std::uint16_t level;
		RE::SEX sex;
		bool unique;
		bool summonable;
		bool child;
	};
}

using NPCData = NPC::Data;
