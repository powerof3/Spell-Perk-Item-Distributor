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

	struct FilterValue
	{
		virtual ~FilterValue() = default;
	protected:
		FilterValue() = default;
	};

	template<typename FormIDType>
    struct FormFilterValue: FilterValue
    {
		FormIDType form;

		FormFilterValue(const FormIDType& form) :
			form(form) {}
    };

	/// String value for a filter that represents any string.
	struct StringValue: FilterValue
	{
		std::string value;

	protected:
		StringValue(const std::string& value) :
			value(value) {}
	};

	/// String value for a filter that represents a wildcard.
	/// The value must be without asterisks (e.g. filter "*Vampire" should be trimmed to "Vampire")
	struct Wildcard : StringValue
	{
		Wildcard(const std::string& value) :
			StringValue(value) {}
	};

	/// String value for a filter that represents an exact match.
	/// The value is stored as-is.
	struct Match : StringValue
	{
		Match(const std::string& value) :
			StringValue(value) {}
	};

	/// Value that represents a range of acceptable Actor levels.
	struct LevelRange : FilterValue
	{
		using Level = std::uint8_t;
		inline constexpr static Level MinLevel = 0;
		inline constexpr static Level MaxLevel = UINT8_MAX;

		Level min;
		Level max;

		LevelRange(const Level min, const Level max)
		{
			const auto _min = min == MaxLevel ? MinLevel : min;
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

		SkillLevelRange(const Skill skill, const Level min, const Level max) :
			LevelRange(min, max)
		{
			this->skill = skill;
		}
	};

	/// Similar to SkillLevelRange, but operates with Skill Weights of NPC's Class instead.
	struct SkillWeightRange : SkillLevelRange
	{
		SkillWeightRange(const Skill skill, const Level min, const Level max) :
			SkillLevelRange(skill, min, max) {}
	};

	/// Generic trait of the NPC.
	struct Trait : FilterValue
	{
	protected:
		Trait() = default;
	};

	struct SexTrait : Trait
	{
		RE::SEX sex;

		SexTrait(RE::SEX sex) :
			sex(sex){}
	};

	struct UniqueTrait : Trait
	{};

	struct SummonableTrait : Trait
	{};

	struct ChildTrait : Trait
	{};

	struct Chance : FilterValue
	{
		using chance = std::uint32_t;

		chance maximum{ 100 };

		Chance(chance max) :
			maximum(max) {}
	};
}

// ------- Specialized filter evaluators --------
namespace Filter
{
	/// Evaluator that determines whether given NPC matches specified filter.
	/// These evaluators are then specialized for each type of supported filters.
	template <typename FilterType>
	struct filter_eval
	{
		static Result evaluate([[maybe_unused]] const FilterType& filter, [[maybe_unused]] const NPCData& a_npcData)
		{
			return Result::kFail;
		}
	};

	// ---------------- Strings ----------------

	template <>
	struct filter_eval<Match>
	{
		static Result evaluate(const Match& filter, const NPCData& a_npcData)
		{
			if (a_npcData.GetKeywords().contains(filter.value) ||
				string::iequals(a_npcData.GetName(), filter.value) ||
				string::iequals(a_npcData.GetOriginalEDID(), filter.value) ||
				string::iequals(a_npcData.GetTemplateEDID(), filter.value)) {
				return Result::kPass;
			}

			return Result::kFail;
		}
	};

	template <>
	struct filter_eval<Wildcard>
	{
		static Result evaluate(const Wildcard& filter, const NPCData& a_npcData)
		{
			// Utilize regex here. (will also support one-sided wildcards (e.g. *Name and Name*)
			// std::regex regex(filter.value, std::regex_constants::icase);
			// std::regex_match(keyword, regex);
			if (std::ranges::any_of(a_npcData.GetKeywords(), [&](auto keyword) { return string::icontains(keyword, filter.value); }) ||
				string::icontains(a_npcData.GetName(), filter.value) ||
				string::icontains(a_npcData.GetOriginalEDID(), filter.value) ||
				string::icontains(a_npcData.GetTemplateEDID(), filter.value)) {
				return Result::kPass;
			}
			return Result::kFail;
		}
	};

	// ---------------- Forms ----------------

	template <>
	struct filter_eval<FormOrMod>
	{
		static Result evaluate(const FormOrMod& filter, const NPCData& a_npcData)
		{
			if (std::holds_alternative<RE::TESForm*>(filter)) {
				if (const auto form = std::get<RE::TESForm*>(filter); form && has_form(form, a_npcData)) {
					return Result::kPass;
				}
			} else if (std::holds_alternative<const RE::TESFile*>(filter)) {
				if (const auto file = std::get<const RE::TESFile*>(filter); file && (file->IsFormInMod(a_npcData.GetOriginalFormID()) || file->IsFormInMod(a_npcData.GetTemplateFormID()))) {
					return Result::kPass;
				}
			}
			return Result::kFail;
		}

	private:
		/// Determines whether given form is associated with an NPC.
		static bool has_form(RE::TESForm* a_form, const NPCData& a_npcData)
		{
			const auto npc = a_npcData.GetNPC();
			switch (a_form->GetFormType()) {
			case RE::FormType::CombatStyle:
				return npc->GetCombatStyle() == a_form;
			case RE::FormType::Class:
				return npc->npcClass == a_form;
			case RE::FormType::Faction:
				{
					const auto faction = a_form->As<RE::TESFaction>();
					return npc->IsInFaction(faction);
				}
			case RE::FormType::Race:
				return a_npcData.GetRace() == a_form;
			case RE::FormType::Outfit:
				return npc->defaultOutfit == a_form;
			case RE::FormType::NPC:
				{
					const auto filterFormID = a_form->GetFormID();
					return npc == a_form || a_npcData.GetOriginalFormID() == filterFormID || a_npcData.GetTemplateFormID() == filterFormID;
				}
			case RE::FormType::VoiceType:
				return npc->voiceType == a_form;
			case RE::FormType::Spell:
				{
					const auto spell = a_form->As<RE::SpellItem>();
					return npc->GetSpellList()->GetIndex(spell).has_value();
				}
			case RE::FormType::Armor:
				return npc->skin == a_form;
			case RE::FormType::FormList:
				{
					bool result = false;

					const auto list = a_form->As<RE::BGSListForm>();
					list->ForEachForm([&](RE::TESForm& a_formInList) {
						if (result = has_form(&a_formInList, a_npcData); result) {
							return RE::BSContainer::ForEachResult::kStop;
						}
						return RE::BSContainer::ForEachResult::kContinue;
					});

					return result;
				}
			default:
				return false;
			}
		}
	};

	// ---------------- Levels ----------------

	template <>
	struct filter_eval<LevelRange>
	{
		static Result evaluate(const LevelRange& filter, const NPCData& a_npcData)
		{
			const auto actorLevel = a_npcData.GetLevel();
			if (actorLevel >= filter.min && actorLevel <= filter.max) {
				return Result::kPass;
			}
			return Result::kFail;
		}
	};

	template <>
	struct filter_eval<SkillLevelRange>
	{
		static Result evaluate(const SkillLevelRange& filter, const NPCData& a_npcData)
		{
			const auto npc = a_npcData.GetNPC();
			const auto skillLevel = npc->playerSkills.values[filter.skill];
			if (skillLevel >= filter.min && skillLevel <= filter.max) {
				return Result::kPass;
			}
			return Result::kFail;
		}
	};

	template <>
	struct filter_eval<SkillWeightRange>
	{
		static Result evaluate(const SkillLevelRange& filter, const NPCData& a_npcData)
		{
			if (const auto skillWeight = weight(filter, a_npcData); skillWeight >= filter.min && skillWeight <= filter.max) {
				return Result::kPass;
			}
			return Result::kFail;
		}

	private:
		static std::optional<std::uint8_t> weight(const SkillLevelRange& filter, const NPCData& a_npcData)
		{
			if (const auto npcClass = a_npcData.GetNPC()->npcClass) {
				const auto& skillWeights = npcClass->data.skillWeights;
				using Skills = SkillLevelRange::Skills;
				switch (filter.skill) {
				case Skills::kOneHanded:
					return skillWeights.oneHanded;
				case Skills::kTwoHanded:
					return skillWeights.twoHanded;
				case Skills::kMarksman:
					return skillWeights.archery;
				case Skills::kBlock:
					return skillWeights.block;
				case Skills::kSmithing:
					return skillWeights.smithing;
				case Skills::kHeavyArmor:
					return skillWeights.heavyArmor;
				case Skills::kLightArmor:
					return skillWeights.lightArmor;
				case Skills::kPickpocket:
					return skillWeights.pickpocket;
				case Skills::kLockpicking:
					return skillWeights.lockpicking;
				case Skills::kSneak:
					return skillWeights.sneak;
				case Skills::kAlchemy:
					return skillWeights.alchemy;
				case Skills::kSpecchcraft:
					return skillWeights.speech;
				case Skills::kAlteration:
					return skillWeights.alteration;
				case Skills::kConjuration:
					return skillWeights.conjuration;
				case Skills::kDestruction:
					return skillWeights.destruction;
				case Skills::kIllusion:
					return skillWeights.illusion;
				case Skills::kRestoration:
					return skillWeights.restoration;
				case Skills::kEnchanting:
					return skillWeights.enchanting;
				default:
					return std::nullopt;
				}
			}
			return std::nullopt;
		}
	};

	// ---------------- Traits ----------------

	template <>
	struct filter_eval<SexTrait>
	{
		static Result evaluate([[maybe_unused]] const SexTrait& filter, const NPCData& a_npcData)
		{
			return a_npcData.GetSex() == filter.sex ? Result::kPass : Result::kFail;
		}
	};

	template <>
	struct filter_eval<UniqueTrait>
	{
		static Result evaluate([[maybe_unused]] const UniqueTrait& filter, const NPCData& a_npcData)
		{
			return a_npcData.IsUnique() ? Result::kPass : Result::kFail;
		}
	};

	template <>
	struct filter_eval<SummonableTrait>
	{
		static Result evaluate([[maybe_unused]] const SummonableTrait& filter, const NPCData& a_npcData)
		{
			return a_npcData.IsSummonable() ? Result::kPass : Result::kFail;
		}
	};

	template <>
	struct filter_eval<ChildTrait>
	{
		static Result evaluate([[maybe_unused]] const ChildTrait& filter, const NPCData& a_npcData)
		{
			return a_npcData.IsChild() ? Result::kPass : Result::kFail;
		}
	};

	template <>
	struct filter_eval<Chance>
	{
		inline static RNG staticRNG;

		static Result evaluate(const Chance& filter, [[maybe_unused]] const NPCData& a_npcData)
		{
			if (filter.maximum >= 100) {
				return Result::kPass;
			}

			const auto rng = staticRNG.Generate<Chance::chance>(0, 100);
			return rng > filter.maximum ? Result::kPass : Result::kFailRNG;
		}
	};
}


// ------- Specialized filter descriptors --------
namespace Filter
{

	template <typename Filter>
	struct filter_descriptor
	{
		static std::ostringstream& describe(std::ostringstream& os, [[maybe_unused]] const Filter& filter)
		{
			os << "@UNKNOWN@";
			return os;
		}
	};

	template <>
	struct filter_descriptor<Match>
	{
		static std::ostringstream& describe(std::ostringstream& os, const Match& filter)
		{
			os << "='" << filter.value << "'";
			return os;
		}
	};

	template <>
	struct filter_descriptor<Wildcard>
	{
		static std::ostringstream& describe(std::ostringstream& os, const Wildcard& filter)
		{
			os << "~'" << filter.value << "'";
			return os;
		}
	};

	template <>
	struct filter_descriptor<FormOrMod>
	{
		static std::ostringstream& describe(std::ostringstream& os, const FormOrMod& filter)
		{
			if (std::holds_alternative<RE::TESForm*>(filter)) {
				if (const auto form = std::get<RE::TESForm*>(filter); form) {
					if (const std::string& edid = form->GetFormEditorID(); !edid.empty()) {
						os << edid;
					} else {
						os << std::setfill('0')
						   << std::setw(sizeof(RE::FormID) * 2)
						   << std::uppercase
						   << std::hex
						   << form->GetFormID();
					}
				}
			} else if (std::holds_alternative<const RE::TESFile*>(filter)) {
				if (const auto file = std::get<const RE::TESFile*>(filter); file) {
					os << file->fileName;
				}
			}
			return os;
		}
	};

	template <>
	struct filter_descriptor<LevelRange>
	{
		static std::ostringstream& describe(std::ostringstream& os, const LevelRange& filter)
		{
			return describeLevel(os, "LVL", filter);
		}

		static std::ostringstream& describeLevel(std::ostringstream& os, const std::string& name, const LevelRange& range)
		{
            const int min = range.min;
            const int max = range.max;
			if (range.min == LevelRange::MinLevel && range.max == LevelRange::MaxLevel) {
				os << name << "=ANY";
			} else if (range.min == range.max) {
				os << name << "=" << range.min;
			} else if (range.min == LevelRange::MinLevel) {
				os << name << "=" << max << "-";
			} else if (range.max == LevelRange::MaxLevel) {
				os << name << "=" << min << "+";
			} else {
				os << name << "=" << min << "-" << max;
			}
			return os;
		}
	};

	template <>
	struct filter_descriptor<SkillLevelRange>
	{
		using Base = filter_descriptor<LevelRange>;
		static std::ostringstream& describe(std::ostringstream& os, const SkillLevelRange& filter)
		{
			return Base::describeLevel(os, "SKL", filter);
		}
	};

	template <>
	struct filter_descriptor<SkillWeightRange>
	{
		using Base = filter_descriptor<LevelRange>;
		static std::ostringstream& describe(std::ostringstream& os, const SkillWeightRange& filter)
		{
			return Base::describeLevel(os, "SWE", filter);
		}
	};

	template <>
	struct filter_descriptor<SexTrait>
	{
		static std::ostringstream& describe(std::ostringstream& os, const SexTrait& filter)
		{
			os << (filter.sex == RE::SEX::kMale ? "M" : "F");
			return os;
		}
	};

	template <>
	struct filter_descriptor<UniqueTrait>
	{
		static std::ostringstream& describe(std::ostringstream& os, [[maybe_unused]] const UniqueTrait& filter)
		{
			os << "U";
			return os;
		}
	};

	template <>
	struct filter_descriptor<SummonableTrait>
	{
		static std::ostringstream& describe(std::ostringstream& os, [[maybe_unused]] const SummonableTrait& filter)
		{
			os << "S";
			return os;
		}
	};

	template <>
	struct filter_descriptor<ChildTrait>
	{
		static std::ostringstream& describe(std::ostringstream& os, [[maybe_unused]] const ChildTrait& filter)
		{
			os << "C";
			return os;
		}
	};

	template <>
	struct filter_descriptor<Chance>
	{
		static std::ostringstream& describe(std::ostringstream& os, const Chance& filter)
		{
			os << "RND " << filter.maximum << "%";
			return os;
		}
	};
}

// ---------------- Expressions ----------------
namespace Filter
{
	/// An abstract filter component that can be evaluated for specified NPC.
	struct Evaluatable
	{
		virtual ~Evaluatable() = default;

		/// Evaluates whether specified NPC matches conditions defined by this Evaluatable.
		[[nodiscard]] virtual Result evaluate([[maybe_unused]] const NPCData& a_npcData) const = 0;

	    virtual std::ostringstream& describe(std::ostringstream& os) const = 0;

        [[nodiscard]] virtual std::size_t GetSize() const
		{
			return 1;
		}
	};

	template <typename T>
	struct evaluatable
	{
		static constexpr bool value = std::is_base_of_v<Evaluatable, T>;
	};

	/// FilterEntry evaluates whether or not given NPC matches entry's value.
	///	It uses filter_eval specialized to FilterType.
	template <typename FilterType>
	struct FilterEntry : Evaluatable
	{
		const FilterType value;

		FilterEntry(const FilterType& value) :
			value(value) {}

		FilterEntry(const FilterEntry& other) :
			value(other.value) {}

		[[nodiscard]] Result evaluate(const NPCData& a_npcData) const override
		{
			return filter_eval<FilterType>::evaluate(value, a_npcData);
		}

		std::ostringstream& describe(std::ostringstream& ss) const override
		{
			return filter_descriptor<FilterType>::describe(ss, value);
		}
	};

	// TODO: Make this a general NegatedEvaluatable, so that it supports negating all kinds of evaluatables.
	/// NegatedFilter is a filter that always inverts the result of evaluation.
	/// It corresponds to `-` prefix in a config file (e.g. "-ActorTypeNPC")
	template <typename FilterType>
	struct NegatedFilter : FilterEntry<FilterType>
	{
		using Base = FilterEntry<FilterType>;

		NegatedFilter(const FilterType& value) :
			Base(value) {}

		[[nodiscard]] Result evaluate(const NPCData& a_npcData) const override
		{
			switch (Base::evaluate(a_npcData)) {
			case Result::kFail:
			case Result::kFailRNG:
				return Result::kPass;
			default:
			case Result::kPass:
				return Result::kFail;
			}
		}

		std::ostringstream& describe(std::ostringstream& ss) const override
		{
			ss << "NOT ";
			return Base::describe(ss);
		}
	};

	/// An expression is a combination of filters and/or nested expressions.
	///
	/// To determine how entries in the expression are combined use either AndExpression or OrExpression.
	struct Expression : Evaluatable
	{
		// Expression are not filters in itself and serve as a shallow containers of actual filters.
		// Thus, expression's size is a number of actual FilterEntry objects nested within.
		// Any number of nested empty expressions will amount in a size of zero.
        [[nodiscard]] std::size_t GetSize() const override
		{
			return std::accumulate(entries.begin(), entries.end(), static_cast<std::size_t>(0), [&](std::size_t res, const std::shared_ptr<Evaluatable>& ptr) {
				return res + ptr->GetSize();
			});
		}

		/// Removes nested empty expressions.
        void flatten()
        {
			for (auto it = entries.begin(); it != entries.end(); it++) {
                const auto eval = it->get();
				if (const auto expression = dynamic_cast<Expression*>(eval)) {
					if (expression->GetSize() == 0) {
						entries.erase(it--);    
					} else {
						expression->flatten();
					}
				}
			}
        }

	    template <typename T, typename = std::enable_if_t<evaluatable<T>::value>>
		void emplace_back(T* ptr)
		{
			static_assert(std::is_constructible_v<std::shared_ptr<Evaluatable>, T*>, "T* must be constructible into shared_ptr<Evaluatable>.");
			entries.emplace_back(std::make_shared<T>(*ptr));
		}

		template <typename T, typename = std::enable_if_t<evaluatable<T>::value>>
		void emplace_back(T&& obj)
		{
			// Not sure why, but this assertion fails, even though the method works. (this is ChatGPT stuff, so I'm not sure what exactly is wrong :))
			//static_assert(std::is_constructible_v<std::shared_ptr<Evaluatable>, T&&>, "T must be constructible into shared_ptr<Evaluatable>.");
			entries.emplace_back(std::make_shared<std::decay_t<T>>(std::forward<T>(obj)));
		}

		template <class FilterType>
		void for_each_filter(std::function<void(const FilterType&)> a_callback) const
		{
			for (const auto& eval : entries) {
				if (auto expression = dynamic_cast<Expression*>(eval.get())) {
					expression->for_each_filter<FilterType>(a_callback);
				} else if (auto entry = dynamic_cast<FilterEntry<FilterType>*>(eval.get())) {
					a_callback(entry->value);
				}
			}
		}

		template <class FilterType>
		bool contains(std::function<bool(const FilterType&)> comparator) const
		{
			return std::ranges::any_of(entries, [&](const auto& eval) {
				if (auto filter = dynamic_cast<Expression*>(eval.get())) {
					return filter->contains<FilterType>(comparator);
				}
				if (auto entry = dynamic_cast<FilterEntry<FilterType>*>(eval.get())) {
					return comparator(entry->value);
				}
				return false;
			});
		}

		std::ostringstream& describe(std::ostringstream& ss) const override
	    {
			return join(", ", ss);
	    }

	protected:
		std::vector<std::shared_ptr<Evaluatable>> entries{};

		std::ostringstream& join(const std::string separator, std::ostringstream& os) const
		{
			auto       begin = entries.begin();
            const auto end = entries.end();
			const bool isComposite = GetSize() > 1;

			if (isComposite)
				os << "(";
			if (begin != end) {
				(*begin++)->describe(os);
				for (; begin != end; ++begin) {
					(*begin)->describe(os);
				}
			}
			if (isComposite)
				os << ")";
			return os;
		}
	};

	/// Entries are combined using AND logic.
	struct AndExpression : Expression
	{
		[[nodiscard]] Result evaluate(const NPCData& a_npcData) const override
		{
			for (auto& entry : entries) {
				const auto res = entry.get()->evaluate(a_npcData);
				if (res != Result::kPass) {
					return res;
				}
			}
			return Result::kPass;
		}

		std::ostringstream& describe(std::ostringstream& os) const override
		{
			return join(" AND ", os);
		}
	};

	/// Entries are combined using OR logic.
	struct OrExpression : Expression
	{
		[[nodiscard]] Result evaluate(const NPCData& a_npcData) const override
		{
			Result failure = Result::kFail;

			for (auto& entry : entries) {
				const auto res = entry.get()->evaluate(a_npcData);
				if (res == Result::kPass) {
					return Result::kPass;
				}
				if (res > failure) {
					// we rely on Result cases being ordered in order of priorities.
					// Hence if there was a kFailRNG it will always be the result of this evaluation.
					failure = res;
				}
			}
			return entries.empty() ? Result::kPass : failure;
		}

		std::ostringstream& describe(std::ostringstream& ss) const override
		{
			return join(" OR ", ss);
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
