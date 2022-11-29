#pragma once

#include "LookupNPC.h"

namespace Filter
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
		inline const static Level MinLevel = 0;
		inline const static Level MaxLevel = UINT8_MAX;

		Level min;
		Level max;

		LevelRange(Level min, Level max) 
		{
			auto _min = min == MaxLevel ? MinLevel : min;
			this->min = std::min(_min, max);
			this->max = std::max(_min, max);
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
	struct SkillWeightRange : SkillLevelRange {
		SkillWeightRange(Skill skill, Level min, Level max) :
			SkillLevelRange(skill, min, max) {}
	};

	/// Generic trait of the NPC.
	struct Trait{};

	struct SexTrait : Trait
	{
		RE::SEX sex;

		SexTrait(RE::SEX sex) :
			sex(sex) {};
	};

	struct UniqueTrait : Trait {};

	struct SummonableTrait : Trait {};

	struct ChildTrait : Trait {};

	struct Chance
	{
		using chance = std::uint32_t;
		
		chance maximum{100};

		Chance(chance max) :
			maximum(max) {}
	};

	/// Evaluator that determines whether given NPC matches specified filter. 
	/// These evaluators are then specialized for each type of supported filters.
	template <typename FilterType>
	struct filter_eval
	{
		static bool evaluate([[maybe_unused]] const FilterType filter, [[maybe_unused]] const NPCData& a_npcData)
		{
			return false;
		}
	};

	/// An abstract filter component that can be evaluated for specified NPC.
	struct Evaluatable
	{
		/// Evaluates whether specified NPC matches conditions defined by this Evaluatable.
		virtual bool evaluate([[maybe_unused]] const NPCData& a_npcData) = 0;
	};

	/// NegatedFilter is an entry that is always inverts the result of evaluation.
	template <class FilterType>
	struct FilterEntry : Evaluatable
	{
		FilterType value;

		FilterEntry(FilterType value) :
			value(value) {}

		FilterEntry<FilterType>& operator=(const FilterEntry<FilterType>&) = default;
		FilterEntry<FilterType>& operator=(FilterEntry<FilterType>&&) = default;

		virtual bool evaluate(const NPCData& a_npcData) override
		{
			return filter_eval<FilterType>::evaluate(value, a_npcData);
		}
	};

	/// NegatedFilter is a filter that always inverts the result of evaluation.
	/// It corresponds to `-` prefix in a config file (e.g. "-ActorTypeNPC")
	template <class FilterType>
	struct NegatedFilter : FilterEntry<FilterType>
	{
		NegatedFilter(FilterType value) :
			FilterEntry(value) {}

		virtual bool evaluate(const NPCData& a_npcData) override
		{
			return !FilterEntry<FilterType>::evaluate(a_npcData);
		}
	};

	/// Filter is a list of entries that should
	/// Entries in a single Filter are combined using AND logic.
	

	/// An expression is a combination of filters and/or nested expressions.
	/// 
	/// It corresponds to the string surrounded by `|` 
	/// (e.g. in "|ActorTypeNPC,ActorTypeDragon|" expression is "ActorTypeNPC,ActorTypeDragon")
	/// 
	/// The expression is broken down into individual Filters (separated by `,`).
	/// Filters are combined using OR logic.
	struct Expression : Evaluatable
	{
		std::vector<Evaluatable> entries;

		template <class FilterType>
		void for_each_filter(std::function<void(FilterType&)> a_callback)
		{
			for (auto& eval : entries) {
				if (auto& filter = static_cast<Expression&>(eval)) {
					filter.entries.for_each_filter<FilterType>(a_callback);
				} else if (auto& entry = static_cast<FilterEntry<FilterType>&>(eval)) {
					a_callback(entry);
				}
			}
		}

		template <class FilterType, class MappedFilterType>
		Expression map(std::function<MappedFilterType(FilterType&)> mapper)
		{
			Expression result = *this->copy();
			for (auto eval : entries) {
				if (auto& filter = static_cast<Expression&>(eval)) {
					result.entries.push_back(filter.map<FilterType, MappedFilterType>(mapper));
				} else if (auto& entry = static_cast<FilterEntry<FilterType>&>(eval)) {
					result.entries.push_back(mapper(entry));
				}
			}

			return result;
		}

		protected:

			virtual Expression* copy() { return this; }
	};

	/// Entries are combined using AND logic.
	struct AndExpression : Expression
	{
		virtual bool evaluate(const NPCData& a_npcData) override
		{
			return entries.empty() || std::ranges::all_of(entries, [&](Evaluatable& entry) { return entry.evaluate(a_npcData); });
		}

		virtual Expression* copy() override {
			return new AndExpression();
		}
	};

	/// Entries are combined using OR logic.
	struct OrExpression : Expression
	{
		virtual bool evaluate(const NPCData& a_npcData) override
		{
			return entries.empty() || std::ranges::any_of(entries, [&](Evaluatable& entry) { return entry.evaluate(a_npcData); });
		}

		virtual Expression* copy() override
		{
			return new OrExpression();
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
