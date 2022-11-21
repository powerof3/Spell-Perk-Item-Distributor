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

	struct Data
	{
		StringFilters strings{};
		FormFilters forms{};
		LevelFilters level{};
		Traits traits{};
		Chance chance{ 100.0f };

		bool HasLevelFilters() const;
		Result PassedFilters(RE::TESNPC& a_actorbase, bool a_noPlayerLevelDistribution) const;

	private:
		Result passed_string_filters(RE::TESNPC& a_actorbase) const;
		Result passed_form_filters(RE::TESNPC& a_actorbase) const;
		Result passed_secondary_filters(RE::TESNPC& a_actorbase) const;
	};
}

using FilterData = Filter::Data;
