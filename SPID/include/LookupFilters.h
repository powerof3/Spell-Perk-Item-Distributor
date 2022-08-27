#pragma once

namespace Filter
{
	bool strings(RE::TESNPC& a_actorbase, const FormData& a_formData);

    bool forms(RE::TESNPC& a_actorbase, const FormData& a_formData);

    bool secondary(const RE::TESNPC& a_actorbase, const FormData& a_formData, bool a_noPlayerLevelDistribution);
}
