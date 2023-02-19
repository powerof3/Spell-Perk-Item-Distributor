#pragma once

namespace NPC
{
	inline constexpr std::string_view processedKeywordEDID{ "SPID_Processed" };
	inline RE::BGSKeyword*            processedKeyword{ nullptr };

	struct Data
	{
		explicit Data(RE::TESNPC* a_npc);

		Data(RE::Actor* a_actor, RE::TESNPC* a_npc);

		[[nodiscard]] bool ShouldProcessNPC() const;

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

	private:
		void cache_keywords();

		static bool is_child(RE::TESNPC* a_npc);

		RE::TESNPC* npc;
		RE::FormID  originalFormID;
		RE::FormID  templateFormID{ 0 };
		std::string name;
		std::string originalEDID;
		std::string templateEDID{};
		StringSet   keywords{};

		std::uint16_t level;
		RE::SEX       sex;
		bool          unique;
		bool          summonable;
		bool          child;
	};
}

using NPCData = NPC::Data;
