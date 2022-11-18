#pragma once

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
			static bool get_type(RE::TESNPC& a_actorbase, RE::TESForm* a_filter);
			static bool matches(RE::TESNPC& a_actorbase, const FormVec& a_forms, bool a_matchesAll = false);
		};

		struct keyword
		{
			static bool contains(RE::TESNPC& a_actorbase, const StringVec& a_strings);
			static bool matches(RE::TESNPC& a_actorbase, const StringVec& a_strings, bool a_matchesAll = false);
		};
	}

	Result PassedFilters(RE::TESNPC& a_actorbase, const FilterData& a_filters, bool a_noPlayerLevelDistribution);
}
