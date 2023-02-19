#pragma once

#include "Filters.h"

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
		AndExpression filters;
		FilterEntry<Chance> chanceFilters{ Chance(100) };

		[[nodiscard]] bool   HasLevelFilters() const;
		[[nodiscard]] Result PassedFilters(const NPC::Data& a_npcData, bool a_noPlayerLevelDistribution) const;

	private:
		[[nodiscard]] bool HasLevelFiltersImpl() const;

		[[nodiscard]] Result passed_string_filters(const NPC::Data& a_npcData) const;
		[[nodiscard]] Result passed_form_filters(const NPC::Data& a_npcData) const;
		[[nodiscard]] Result passed_secondary_filters(const NPC::Data& a_npcData) const;
	};
}

using FilterData = Filter::Data;
