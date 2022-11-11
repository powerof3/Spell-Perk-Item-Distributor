#pragma once

enum class SECONDARY_RESULT
{
	kFail = 0,
	kFailRNG,
	kPass
};

namespace Filter
{
	inline RNG staticRNG;

    bool strings(RE::TESNPC& a_actorbase, const StringFilters& a_stringFilters);

	bool forms(RE::TESNPC& a_actorbase, const FormFilters& a_formFilters);

	SECONDARY_RESULT secondary(const RE::TESNPC& a_actorbase, const LevelFilters& a_levelFilters, const Traits& a_traits, float a_chance, bool a_noPlayerLevelDistribution);
}
