#pragma once

#include "LookupNPC.h"

namespace NewFilters
{
	/// String value for a filter that represents a wildcard.
	struct Wildcard
	{
		/// Filter value without asterisks (e.g. filter "*Vampire" is trimmed to "Vampire")
		std::string value;
	};

	/// Value that represents a range of acceptable Actor levels.
	struct LevelRange
	{
		using Level = std::uint8_t;
		Level min;
		Level max;

		LevelRange(Level min, Level max) 
		{
			auto _min = std::min(min, max);
			this->min = _min == UINT8_MAX ? 0 : _min;
			this->max = std::max(min, max);
		}
	};

	/// Value that represents a range of acceptable Skill Level for particular skill.
	struct SkillLevelRange: LevelRange
	{
		using Skills = RE::TESNPC::Skills;
		using Skill = std::uint32_t;

		Skill skill;
		
		SkillLevelRange(Skill skill, Level min, Level max) :
			LevelRange(min, max)
		{
			this->skill = skill;
		}
	};

	/// Similar to SkillLevelRange, but operates with Skill Weights of NPC's Class instead.
	struct SkillWeightRange : SkillLevelRange {};

	/// Generic trait of the NPC.

	template <class Value>
	struct Trait
	{
		Value value;

		Trait(Value value) :
			value(value) {}
	};

	struct SexTrait : Trait<RE::SEX>
	{
		SexTrait(RE::SEX sex) :
			Trait(sex) {}
	};

	struct UniqueTrait : Trait<bool>
	{
		UniqueTrait(bool isUnique) :
			Trait(isUnique) {}
	};

	struct SummonableTrait : Trait<bool>
	{
		SummonableTrait(bool isSummonable) :
			Trait(isSummonable) {}
	};

	struct ChildTrait : Trait<bool>
	{
		ChildTrait(bool isChild) :
			Trait(isChild) {}
	};

	struct Chance
	{
		using chance = std::uint32_t;
		
		chance maximum{100};

		Chance(chance max) :
			maximum(max) {}
	};

	/// Evaluator that determines whether given NPC matches specified filter. 
	/// These evaluators are then specialized for each type of supported filters.
	template <class FilterType>
	struct filter_eval
	{
		static bool evaluate(const FilterType filter, [[maybe_unused]] const NPCData& a_npcData)
		{
			return false;
		}
	};

	/// An abstract filter component that can be evaluated for specified NPC.
	struct Evaluatable
	{
		/// Evaluates whether specified NPC matches conditions defined by this Evaluatable.
		virtual bool evaluate(const NPCData& a_npcData) = 0;
	};

	/// NegatedFilterEntry is an entry that is always inverts the result of evaluation.
	template <class FilterType>
	struct FilterEntry : Evaluatable
	{
		const FilterType value;

		FilterEntry(FilterType value) :
			value(value) {}

		virtual bool evaluate(const NPCData& a_npcData) override
		{
			return filter_eval<FilterType>::evaluate(value, a_npcData);
		}
	};

	/// NegatedFilterEntry is an entry that is always inverts the result of evaluation.
	/// It corresponds to `-` prefix in a config file (e.g. "-ActorTypeNPC")
	template <class FilterType>
	struct NegatedFilterEntry : FilterEntry<FilterType>
	{

		NegatedFilterEntry(NegatedFilterEntry value) :
			FilterEntry(value) {}

		virtual bool evaluate(const NPCData& a_npcData) override
		{
			return !filter_eval<FilterType>::evaluate(this->value, a_npcData);
		}
	};

	/// Filter is a list of entries that should
	/// Entries in a single Filter are combined using AND logic.
	struct Filter : Evaluatable
	{
		/// Entries are combined using AND logic.
		std::vector<Evaluatable> entries;

		virtual bool evaluate(const NPCData& a_npcData) override
		{
			return entries.empty() || std::ranges::all_of(entries, [&](Evaluatable& entry) { return entry.evaluate(a_npcData); });
		}
	};

	/// An expression is a whole filters section in a config file.
	/// It corresponds to the string surrounded by `|` 
	/// (e.g. in "|ActorTypeNPC,ActorTypeDragon|" expression is "ActorTypeNPC,ActorTypeDragon")
	/// 
	/// The expression is broken down into individual Filters (separated by `,`).
	/// Filters are combined using OR logic.
	struct Expression : Evaluatable
	{
		/// Filters are combined using OR logic.
		std::vector<Evaluatable> filters;

		virtual bool evaluate(const NPCData& a_npcData) override
		{
			return filters.empty() || std::ranges::any_of(filters, [&](Evaluatable& entry) { return entry.evaluate(a_npcData); });
		}
	};
}


/*
Input: key1,-key2,-key3+key4,-*key5+key6+-key7
Step 1: split ","
 key1
 -key2
 -key3+key4
 -*key5+key6+-key7

 Step 2: split "+"
 -key3
 key4

 -*key5
 key6
 -key7
*/
