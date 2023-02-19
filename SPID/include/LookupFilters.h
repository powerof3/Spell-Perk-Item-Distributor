#pragma once

#include "LookupNPC.h"

namespace Filter
{
	enum class Result
	{
		kFail = 0,
		kFailRNG,
		kPass
	};
}

// ------------- Filterable types --------------
namespace Filter
{
	/// String value for a filter that represents a wildcard.
	struct StringValue
	{
		std::string value;

	protected:
		StringValue(std::string value) :
			value(value) {}
	};

	/// String value for a filter that represents a wildcard.
	/// The value must be without asterisks (e.g. filter "*Vampire" should be trimmed to "Vampire")
	struct Wildcard : StringValue
	{
		Wildcard(std::string value) :
			StringValue(value) {}
	};

	/// String value for a filter that represents an exact match.
	/// The value is stored as-is.
	struct Match : StringValue
	{
		Match(std::string value) :
			StringValue(value) {}
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
	struct SkillLevelRange : LevelRange
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
	struct SkillWeightRange : SkillLevelRange
	{
		SkillWeightRange(Skill skill, Level min, Level max) :
			SkillLevelRange(skill, min, max) {}
	};

	/// Generic trait of the NPC.
	struct Trait
	{};

	struct SexTrait : Trait
	{
		RE::SEX sex;

		SexTrait(RE::SEX sex) :
			sex(sex){};
	};

	struct UniqueTrait : Trait
	{};

	struct SummonableTrait : Trait
	{};

	struct ChildTrait : Trait
	{};

	struct Chance
	{
		using chance = std::uint32_t;

		chance maximum{ 100 };

		Chance(chance max) :
			maximum(max) {}
	};
}

// ---------------- Expressions ----------------
namespace Filter
{
	/// Evaluator that determines whether given NPC matches specified filter.
	/// These evaluators are then specialized for each type of supported filters.
	template <typename FilterType>
	struct filter_eval
	{
		static Result evaluate([[maybe_unused]] const FilterType filter, [[maybe_unused]] const NPCData& a_npcData)
		{
			return Result::kFail;
		}
	};

	/// An abstract filter component that can be evaluated for specified NPC.
	struct Evaluatable
	{
		/// Evaluates whether specified NPC matches conditions defined by this Evaluatable.
		virtual Result evaluate([[maybe_unused]] const NPCData& a_npcData) const
		{
			return Result::kFail;
		}
	};

	/// NegatedFilter is an entry that is always inverts the result of evaluation.
	template <class FilterType>
	struct FilterEntry : Evaluatable
	{
		FilterType value;

		FilterEntry(FilterType value) :
			value(value) {}

		// Probably should be deleted?
		FilterEntry<FilterType>& operator=(const FilterEntry<FilterType>&) = default;
		FilterEntry<FilterType>& operator=(FilterEntry<FilterType>&&) = default;

		virtual Result evaluate(const NPCData& a_npcData) const override
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
			FilterEntry<FilterType>(value) {}

		virtual Result evaluate(const NPCData& a_npcData) const override
		{
			switch (filter_eval<FilterType>::evaluate(FilterEntry<FilterType>::value, a_npcData)) {
			case Result::kFail:
			case Result::kFailRNG:
				return Result::kPass;
			default:
			case Result::kPass:
				return Result::kFail;
			}
		}
	};

	/// An expression is a combination of filters and/or nested expressions.
	///
	/// To determine how entries in the expression are combined use either AndExpression or OrExpression.
	struct Expression : Evaluatable
	{
		std::vector<Evaluatable> entries{};

		template <class FilterType>
		void for_each_filter(std::function<void(const FilterType&)> a_callback) const
		{
			for (auto& eval : entries) {
				if (auto filter = static_cast<const Expression*>(&eval)) {
					filter->for_each_filter<FilterType>(a_callback);
				} else if (auto entry = static_cast<const FilterEntry<FilterType>*>(&eval)) {
					a_callback(entry->value);
				}
			}
		}

		template <class FilterType>
		bool contains(std::function<bool(const FilterType&)> comparator) const
		{
			for (auto& eval : entries) {
				if (auto filter = static_cast<const Expression*>(&eval); filter->contains<FilterType>(comparator)) {
					return true;
				} else if (auto entry = static_cast<const FilterEntry<FilterType>*>(&eval); comparator(entry->value)) {
					return true;
				}
			}
			return false;
		}
	};

	/// Entries are combined using AND logic.
	struct AndExpression : Expression
	{
		virtual Result evaluate(const NPCData& a_npcData) const override
		{
			for (auto& entry : entries) {
				auto res = entry.evaluate(a_npcData);
				if (res != Result::kPass) {
					return res;
				}
			}
			return Result::kPass;
		}
	};

	/// Entries are combined using OR logic.
	struct OrExpression : Expression
	{
		virtual Result evaluate(const NPCData& a_npcData) const override
		{
			Result failure = Result::kFail;

			for (auto& entry : entries) {
				auto res = entry.evaluate(a_npcData);
				if (res == Result::kPass) {
					return Result::kPass;
				} else if (res > failure) {
					// we rely on Result cases being ordered in order of priotities.
					// Hence if there was a kFailRNG it will always be the result of this evaluation.
					failure = res;
				}
			}
			return entries.empty() ? Result::kPass : failure;
		}
	};
}

// ------------------- Data --------------------
namespace Filter
{
	struct Data
	{
		Data(AndExpression filters);

		AndExpression filters;
		bool          hasLeveledFilters;

		[[nodiscard]] bool   HasLevelFilters() const;
		[[nodiscard]] Result PassedFilters(const NPC::Data& a_npcData) const;

	private:
		[[nodiscard]] bool HasLevelFiltersImpl() const;
	};
}

using FilterData = Filter::Data;

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
