#pragma once

namespace NPC
{
	inline std::once_flag  init;
	inline RE::TESFaction* potentialFollowerFaction;

	struct Data
	{
		Data(RE::Actor* a_actor, RE::TESNPC* a_npc);
		~Data() = default;

		[[nodiscard]] RE::TESNPC* GetNPC() const;
		[[nodiscard]] RE::Actor*  GetActor() const;

		[[nodiscard]] bool HasStringFilter(const StringVec& a_strings, bool a_all = false) const;
		[[nodiscard]] bool ContainsStringFilter(const StringVec& a_strings) const;
		bool               InsertKeyword(const char* a_keyword);
		[[nodiscard]] bool HasFormFilter(const FormVec& a_forms, bool all = false) const;

		/// <summary>
		/// Checks whether given NPC already has another form that is mutually exclusive with the given form,
		/// according to the exclusion groups configuration.
		/// </summary>
		/// <param name="otherForm">A Form that needs to be checked.</param>
		/// <returns></returns>
		[[nodiscard]] bool HasMutuallyExclusiveForm(RE::TESForm* otherForm) const;

		[[nodiscard]] std::uint16_t GetLevel() const;
		[[nodiscard]] bool          IsChild() const;
		[[nodiscard]] bool          IsLeveled() const;
		[[nodiscard]] bool          IsTeammate() const;

		[[nodiscard]] RE::TESRace* GetRace() const;

	private:
		struct ID
		{
			ID() = default;
			explicit ID(const RE::TESActorBase* a_base);
			~ID() = default;

			[[nodiscard]] bool contains(const std::string& a_str) const;

			bool operator==(const RE::TESFile* a_mod) const;
			bool operator==(const std::string& a_str) const;
			bool operator==(RE::FormID a_formID) const;

			RE::FormID  formID{ 0 };
			std::string editorID{};
		};

		[[nodiscard]] bool has_keyword_string(const std::string& a_string) const;
		[[nodiscard]] bool has_form(RE::TESForm* a_form) const;

		RE::TESNPC*     npc;
		RE::Actor*      actor;
		RE::TESRace*    race;
		std::vector<ID> IDs;
		std::string     name;
		StringSet       keywords{};
		std::uint16_t   level;
		bool            child;
		bool            teammate;
		bool            leveled;
	};
}

using NPCData = NPC::Data;
