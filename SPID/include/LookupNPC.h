#pragma once

namespace NPC
{
	struct Data
	{
		Data(RE::Actor* a_actor, RE::TESNPC* a_npc);

		[[nodiscard]] RE::TESNPC* GetNPC() const;
		[[nodiscard]] RE::Actor*  GetActor() const;

		[[nodiscard]] std::string GetName() const;

		// All these are deprecated:
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

		[[nodiscard]] bool HasKeyword(const RE::BGSKeyword* kwd) const;
		[[nodiscard]] bool InsertKeyword(const RE::BGSKeyword* kwd);

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

		RE::TESNPC*   npc;
		RE::Actor*    actor;
		std::string   name;
		RE::TESRace*  race;
		ID            originalIDs;
		ID            templateIDs{};

		std::uint16_t level;
		RE::SEX       sex;
		bool          unique;
		bool          summonable;
		bool          child;

		std::set<RE::FormID> keywords;
	};
}

using NPCData = NPC::Data;
