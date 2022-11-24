#pragma once

namespace NPC
{
	struct Data;
}

namespace Filter
{
	inline RNG staticRNG;

	enum class Result
	{
		kFail = 0,
		kFailRNG,
		kPass
	};

	namespace detail
	{
		struct form
		{
			static bool get_type(RE::TESNPC* a_npc, RE::TESForm* a_filter);
			static bool matches(RE::TESNPC* a_npc, RE::FormID a_formID, const FormVec& a_forms, bool a_matchesAll = false);
		};
		struct name
		{
			static bool contains(const std::string& a_name, const StringVec& a_strings);
			static bool matches(const std::string& a_name, const StringVec& a_strings);
		};
		struct keyword
		{
			static bool contains(RE::TESNPC* a_npc, const StringVec& a_strings);
			static bool matches(RE::TESNPC* a_npc, const StringVec& a_strings, bool a_matchesAll = false);
		};
	}

	struct Data
	{
		StringFilters strings{};
		FormFilters forms{};
		LevelFilters level{};
		Traits traits{};
		Chance chance{ 100 };

		[[nodiscard]] bool HasLevelFilters() const;
		[[nodiscard]] Result PassedFilters(const NPC::Data& a_npcData, bool a_noPlayerLevelDistribution) const;

	private:
		[[nodiscard]] Result passed_string_filters(const NPC::Data& a_npcData) const;
		[[nodiscard]] Result passed_form_filters(const NPC::Data& a_npcData) const;
		[[nodiscard]] Result passed_secondary_filters(const NPC::Data& a_npcData) const;
	};
}

using FilterData = Filter::Data;
