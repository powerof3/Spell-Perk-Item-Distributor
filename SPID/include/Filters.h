#pragma once

#include "LookupNPC.h"

namespace NewFilters
{
	template <class FilterType>
	struct filter_eval
	{
		static bool evaluate(const FilterType filter, const NPCData& a_npcData) {
			return false;
		}
	};

	template<>
	struct filter_eval<std::string>
	{
		static bool evaluate(const std::string filter, const NPCData& a_npcData) {
			return false;
		}
	};

	class Evaluatable
	{
		virtual bool evaluate(const NPCData& a_npcData)
		{
			return false;
		}
	};

	template <class FilterType>
	class FilterEntry: Evaluatable
	{
		const FilterType value;

		virtual bool evaluate(const NPCData& a_npcData) override
		{
			return filter_eval<FilterEntry>::evaluate(value, a_npcData);
		}
	};

	template <class FilterType>
	class NegatedFilterEntry : FilterEntry<FilterType>
	{
		bool evaluate(const NPCData& a_npcData) override
		{
			return !filter_eval<FilterEntry>::evaluate(value, a_npcData);
		}
	};

	template <class FilterType>
	class Filter: Evaluatable
	{
		/// Filters combined using AND logic.
		std::vector<FilterEntry<FilterType>> entries;

		virtual bool evaluate(const NPCData& a_npcData) override
		{
			return std::ranges::all_of(entries, [](auto entry) { return entry.evaluate(a_npcData); })
		}
	};

	template <class FilterType>
	class Expression: Evaluatable
	{
		/// Filters combined using OR logic.
		std::vector<Filter<T>> filters;

		virtual bool evaluate(const NPCData& a_npcData) override
		{
			virtual bool evaluate(const NPCData& a_npcData) override
			{
				return std::ranges::any_of(filters, [](auto entry) { return entry.evaluate(a_npcData); })
			}
		}
	};
}
