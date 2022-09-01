#pragma once

namespace Filter
{
	enum class SECONDARY_RESULT
	{
	    kFail = 0,
		kFailDueToRNG,
		kPass
	};

    bool strings(RE::TESNPC& a_actorbase, const FormData& a_formData);

    bool forms(RE::TESNPC& a_actorbase, const FormData& a_formData);

    SECONDARY_RESULT secondary(const RE::TESNPC& a_actorbase, const FormData& a_formData, bool a_noPlayerLevelDistribution);
}
