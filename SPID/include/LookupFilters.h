#pragma once

#include "LookupNPC.h"

enum class SECONDARY_RESULT
{
	kFail = 0,
	kFailRNG,
	kPass
};

namespace Filter
{
	inline RNG staticRNG;

    bool strings(const NPCData& a_npcData, const StringFilters& a_stringFilters);

	bool forms(const NPCData& a_npcData, const FormFilters& a_formFilters);

	SECONDARY_RESULT secondary(const NPCData& a_npcData, const LevelFilters& a_levelFilters, const Traits& a_traits, float a_chance, bool a_noPlayerLevelDistribution);
}
