#pragma once

namespace NPC
{
	struct Data
	{
		Data(RE::Actor* a_actor, RE::TESNPC* a_npc);

		[[nodiscard]] RE::TESNPC* GetNPC() const;
		[[nodiscard]] RE::Actor*  GetActor() const;

		[[nodiscard]] bool HasStringFilter(const StringVec& a_strings, bool a_all = false) const;
		[[nodiscard]] bool ContainsStringFilter(const StringVec& a_strings) const;
		bool               InsertKeyword(const char* a_keyword);
		[[nodiscard]] bool HasFormFilter(const FormVec& a_forms, bool all = false) const;

		[[nodiscard]] std::uint16_t GetLevel() const;
		[[nodiscard]] RE::SEX       GetSex() const;
		[[nodiscard]] bool          IsUnique() const;
		[[nodiscard]] bool          IsSummonable() const;
		[[nodiscard]] bool          IsChild() const;

		[[nodiscard]] RE::TESRace* GetRace() const;

	private:
		struct ID
		{
			ID() = default;
			explicit ID(RE::TESActorBase* a_base);

			[[nodiscard]] bool contains(const std::string& a_str) const;

			bool operator==(const RE::TESFile* a_mod) const;
			bool operator==(const std::string& a_str) const;
			bool operator==(RE::FormID a_formID) const;

			RE::FormID  formID{ 0 };
			std::string editorID{};
		};

		void cache_keywords();

		[[nodiscard]] bool has_keyword_string(const std::string& a_string) const;
		[[nodiscard]] bool contains_keyword_string(const std::string& a_string) const;
		[[nodiscard]] bool has_form(RE::TESForm* a_form) const;

		RE::TESNPC*   npc;
		RE::Actor*    actor;
		std::string   name;
		RE::TESRace*  race;
		ID            originalIDs;
		ID            templateIDs{};
		StringSet     keywords{};
		std::uint16_t level;
		RE::SEX       sex;
		bool          unique;
		bool          summonable;
		bool          child;
	};
}

using NPCData = NPC::Data;
