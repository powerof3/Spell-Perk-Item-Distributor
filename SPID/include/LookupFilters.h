#pragma once

namespace NPC
{
	struct Data;
}

namespace Filter
{
	inline RNG staticRNG{};

	enum class Result
	{
		kFail = 0,
		kFailRNG,
		kPass
	};

	struct Data
	{
		Data(StringFilters a_strings, FormFilters a_formFilters, LevelFilters a_level, Traits a_traits, Chance a_chance);

		StringFilters strings{};
		FormFilters   forms{};
		LevelFilters  level{};
		Traits        traits{};
		Chance        chance{ 100 };

		bool hasLeveledFilters;

		[[nodiscard]] bool   HasLevelFilters() const;
		[[nodiscard]] Result PassedFilters(const NPC::Data& a_npcData) const;

	private:
		[[nodiscard]] bool HasLevelFiltersImpl() const;

		[[nodiscard]] Result passed_string_filters(const NPC::Data& a_npcData) const;
		[[nodiscard]] Result passed_form_filters(const NPC::Data& a_npcData) const;
		[[nodiscard]] Result passed_secondary_filters(const NPC::Data& a_npcData) const;
	};
}

using FilterData = Filter::Data;
