#pragma once

#include "LookupNPC.h"

namespace filters
{
	enum class Result
	{
		kFail = 0,
		kFailRNG,
		kPass
	};
}

// ---------------- Expressions ----------------
namespace filters
{
	/// An abstract filter component that can be evaluated for specified NPC.
	struct Evaluatable
	{
		virtual ~Evaluatable() = default;
      
        /// Evaluates whether specified NPC matches conditions defined by this Evaluatable.
		[[nodiscard]] virtual Result evaluate([[maybe_unused]] const NPCData& a_npcData) const = 0;

	    virtual std::ostringstream& describe(std::ostringstream& os) const = 0;

        [[nodiscard]] virtual std::size_t GetSize() const = 0;
        
	};

    struct Filter: Evaluatable
	{
		[[nodiscard]] std::size_t GetSize() const override { return 1; }

	protected:
		Filter() = default;
	};

	/// Filter evaluates whether or not given NPC matches specific value provided to the filter.
	///	Inherit it to define your own filters.
	template <typename Value>
    struct ValueFilter : Filter
	{
		Value value;

	    ValueFilter(const Value& value) : Filter(),
			value(value) {}
		ValueFilter() = default;
	};

    template <typename T>
	concept filtertype = std::derived_from<T, Filter>;

	/// An expression is a combination of filters and/or nested expressions.
	///
	/// To determine how entries in the expression are combined use either AndExpression or OrExpression.
	struct Expression : Evaluatable
	{
		// Expression are not filters in itself and serve as a shallow containers of actual filters.
		// Thus, expression's size is a number of actual Filter objects nested within.
		// Any number of nested empty expressions will amount in a size of zero.
        [[nodiscard]] std::size_t GetSize() const override
		{
			return std::accumulate(entries.begin(), entries.end(), static_cast<std::size_t>(0), [&](std::size_t res, const auto& ptr) {
				return res + ptr.get()->GetSize();
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

		void emplace_back(Evaluatable* ptr)
		{
			entries.emplace_back(ptr);
		}

		template <filtertype FilterType>
		void for_each_filter(std::function<void(const FilterType*)> a_callback) const
		{
			for (const auto& eval : entries) {
				if (const auto expression = dynamic_cast<Expression*>(eval.get())) {
					expression->for_each_filter<FilterType>(a_callback);
				} else if (const auto entry = dynamic_cast<FilterType*>(eval.get())) {
					a_callback(entry);
				}
			}
		}

		template <filtertype FilterType, filtertype NewFilterType>
		void map(std::function<NewFilterType* (FilterType*)> mapper)
		{
			// TODO: Figure out the mapping..
			// Could try to merge UnknownFormIDFilter and FormFilter into one and use a for_each_filter instead to map correct Form.
		/*	std::vector<std::unique_ptr<Evaluatable>> newEntries{};
			std::transform(entries.begin(), entries.end(), std::back_inserter(newEntries), [&mapper] (std::unique_ptr<Evaluatable> eval) {
				if (const auto expression = dynamic_cast<Expression*>(eval.get())) {
					expression->map<FilterType, NewFilterType>(mapper);
				} else if (const auto entry = dynamic_cast<FilterType*>(eval.get())) {
					if (const auto newFilter = mapper(entry); newFilter) {
                        return std::unique_ptr<Evaluatable>(newFilter);
                    }
                }
				return std::unique_ptr(std::move(eval));
            });*/
			//entries = newEntries;
		}

		template <filtertype FilterType>
		bool contains(std::function<bool(const FilterType*)> comparator) const
		{
			return std::ranges::any_of(entries, [&](const auto& eval) {
				if (const auto filter = dynamic_cast<Expression*>(eval.get())) {
					return filter->contains<FilterType>(comparator);
				}
				if (const auto entry = dynamic_cast<const FilterType*>(eval.get())) {
					return comparator(entry);
				}
				return false;
			});
		}

		std::ostringstream& describe(std::ostringstream& ss) const override
	    {
			return join(", ", ss);
	    }

	protected:
		std::vector<std::unique_ptr<Evaluatable>> entries{};

        std::ostringstream& join(const std::string& separator, std::ostringstream& os) const
		{
			auto       begin = entries.begin();
            const auto end = entries.end();
			const bool isComposite = GetSize() > 1;

			if (isComposite)
				os << "(";
			if (begin != end) {
				(*begin++)->describe(os);
				for (; begin != end; ++begin) {
					os << separator;
					(*begin)->describe(os);
				}
			}
			if (isComposite)
				os << ")";
			return os;
		}
	};

	/// Negated is an evaluatable wrapper that always inverts the result of the wrapped evaluation.
	/// It corresponds to `-` prefix in a config file (e.g. "-ActorTypeNPC")
	struct Negated : Expression
	{
	    Negated(Evaluatable* const eval) :
			Expression()
	    {
			entries.emplace_back(eval);
	    }

		[[nodiscard]] Result evaluate(const NPCData& npcData) const override
		{
			if (const auto eval = wrapped_value()) {
				switch (eval->evaluate(npcData)) {
				case Result::kFail:
				case Result::kFailRNG:
					return Result::kPass;
				default:
				case Result::kPass:
					return Result::kFail;
				}
			}
            return Result::kFail;
		}

		std::ostringstream& describe(std::ostringstream& os) const override
		{
			if (const auto eval = wrapped_value()) {
				os << "NOT ";
				return eval->describe(os);
			}
			return os;
		}

        [[nodiscard]] std::size_t GetSize() const override
		{
			if (const auto eval = wrapped_value()) {
				return eval->GetSize();
			}
			return 0;
		}

	private:

		Evaluatable* wrapped_value() const
        {
			return entries.front().get();
		}
	};

	/// Entries are combined using AND logic.
	struct AndExpression : Expression
	{
		[[nodiscard]] Result evaluate(const NPCData& npcData) const override
		{
			for (const auto& entry : entries) {
				const auto res = entry.get()->evaluate(npcData);
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
		[[nodiscard]] Result evaluate(const NPCData& npcData) const override
		{
            auto failure = Result::kFail;

			for (const auto& entry : entries) {
				const auto res = entry.get()->evaluate(npcData);
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
namespace filters
{
	struct Data
	{
		Data(AndExpression* filters);

		std::shared_ptr<AndExpression> filters;
		bool                           hasLeveledFilters;

		[[nodiscard]] bool   HasLevelFilters() const;
		[[nodiscard]] Result PassedFilters(const NPC::Data& a_npcData) const;

	private:
		[[nodiscard]] bool HasLevelFiltersImpl() const;
	};
}

using FilterData = filters::Data;

// ------------- Filterable types --------------
namespace filters
{
	namespace SPID
	{
	    /// Form filter that wasn't bound to an actual RE::TESForm.
	    ///	This filter will always fail and will log a warning when described.
		struct UnknownFormIDFilter : ValueFilter<FormOrEditorID>
		{
			using ValueFilter::ValueFilter;

			[[nodiscard]] Result evaluate([[maybe_unused]] const NPCData& a_npcData) const override
			{
				return Result::kFail;
			}

			std::ostringstream& describe(std::ostringstream& os) const override
			{
				os << "Unknown ";
				if (std::holds_alternative<FormModPair>(value)) {
					const auto& formModPair = std::get<FormModPair>(value);
					auto& [formID, modName] = formModPair;
					if (formID) {
						os << "[FORM:"
						   << std::setfill('0')
						   << std::setw(sizeof(RE::FormID) * 2)
						   << std::uppercase
						   << std::hex
						   << *formID
						   << "]";    
					}

					if (modName) {
						if (formID) {
							os << "@";
						}
						os << *modName;
					}
				} else if (std::holds_alternative<std::string>(value)) {
					const auto& edid = std::get<std::string>(value);
					os << edid;
				}
				return os;
			}
		};

		struct FormFilter : ValueFilter<FormOrMod>
		{
			using ValueFilter::ValueFilter;

            [[nodiscard]] Result evaluate(const NPCData& a_npcData) const override
			{
				if (std::holds_alternative<RE::TESForm*>(value)) {
					if (const auto form = std::get<RE::TESForm*>(value); form && has_form(form, a_npcData)) {
						return Result::kPass;
					}
				} else if (std::holds_alternative<const RE::TESFile*>(value)) {
					if (const auto file = std::get<const RE::TESFile*>(value); file && (file->IsFormInMod(a_npcData.GetOriginalFormID()) || file->IsFormInMod(a_npcData.GetTemplateFormID()))) {
						return Result::kPass;
					}
				}
				return Result::kFail;
			}

			std::ostringstream& describe(std::ostringstream& os) const override
			{
				if (std::holds_alternative<RE::TESForm*>(value)) {
					if (const auto form = std::get<RE::TESForm*>(value)) {
						if (const std::string& edid = form->GetFormEditorID(); !edid.empty()) {
							os << edid;
						}
						os << "["
						   << std::to_string(form->GetFormType())
						   << ":"
						   << std::setfill('0')
						   << std::setw(sizeof(RE::FormID) * 2)
						   << std::uppercase
						   << std::hex
						   << form->GetFormID()
						   << "]";
					}
				} else if (std::holds_alternative<const RE::TESFile*>(value)) {
					if (const auto file = std::get<const RE::TESFile*>(value); file) {
						os << file->fileName;
					}
				}
				return os;
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

		/// String value for a filter that represents a wildcard.
		/// The value must be without asterisks (e.g. filter "*Vampire" should be trimmed to "Vampire")
		struct WildcardFilter : ValueFilter<std::string>
		{
			using ValueFilter::ValueFilter;

			[[nodiscard]] Result evaluate(const NPCData& npcData) const override
			{
				// Utilize regex here. (will also support one-sided wildcards (e.g. *Name and Name*)
				// std::regex regex(filter.value, std::regex_constants::icase);
				// std::regex_match(keyword, regex);
				if (std::ranges::any_of(npcData.GetKeywords(), [&](auto keyword) { return string::icontains(keyword, value); }) ||
					string::icontains(npcData.GetName(), value) ||
					string::icontains(npcData.GetOriginalEDID(), value) ||
					string::icontains(npcData.GetTemplateEDID(), value)) {
					return Result::kPass;
				}
				return Result::kFail;
			}

			std::ostringstream& describe(std::ostringstream& os) const override
            {
				os << "~'" << value << "'";
				return os;
			}
        };

		/// String value for a filter that represents an exact match.
		/// The value is stored as-is.
		struct MatchFilter : ValueFilter<std::string>
		{
			using ValueFilter::ValueFilter;

            [[nodiscard]] Result evaluate(const NPCData& a_npcData) const override
			{
				if (a_npcData.GetKeywords().contains(value) ||
					string::iequals(a_npcData.GetName(), value) ||
					string::iequals(a_npcData.GetOriginalEDID(), value) ||
					string::iequals(a_npcData.GetTemplateEDID(), value)) {
					return Result::kPass;
				}
				return Result::kFail;
			}

			std::ostringstream& describe(std::ostringstream& os) const override
            {
				os << "='" << value << "'";
				return os;
			}
		};

		using Level = std::uint8_t;
		using LevelPair = std::pair<Level, Level>;
		/// Value that represents a range of acceptable Actor levels.
		struct LevelFilter : ValueFilter<LevelPair>
		{
			inline constexpr static Level MinLevel = 0;
			inline constexpr static Level MaxLevel = UINT8_MAX;

			LevelFilter(const Level min, const Level max): ValueFilter()
			{
				const auto bound_min = min == MaxLevel ? MinLevel : min;
				this->value = { std::min(bound_min, max), std::max(bound_min, max) };
			}

			LevelFilter(LevelPair value) = delete;
			LevelFilter(const LevelPair& value) = delete;

			Result evaluate(const NPCData& a_npcData) const override
			{
				const auto actorLevel = a_npcData.GetLevel();
				if (actorLevel >= value.first && actorLevel <= value.second) {
					return Result::kPass;
				}
				return Result::kFail;
			}

			std::ostringstream& describe(std::ostringstream& os) const override
			{
				return describeLevel(os, "LVL");
			}

		protected:
			std::ostringstream& describeLevel(std::ostringstream& os, const std::string& name) const
            {
				const int min = value.first;
				const int max = value.second;
				if (value.first == MinLevel && value.second == MaxLevel) {
					os << name << "=ANY";
				} else if (value.first == value.second) {
					os << name << "=" << value.first;
				} else if (value.first == MinLevel) {
					os << name << "=" << max << "-";
				} else if (value.second == MaxLevel) {
					os << name << "=" << min << "+";
				} else {
					os << name << "=" << min << "-" << max;
				}
				return os;
			}
		};

		/// Value that represents a range of acceptable Skill Level for particular skill.
		struct SkillFilter : LevelFilter
		{
			using Skills = RE::TESNPC::Skills;
			using Skill = std::uint32_t;

			Skill skill;

			SkillFilter(const Skill skill, const Level min, const Level max) :
				LevelFilter(min, max)
			{
				this->skill = skill;
			}

			SkillFilter(Level min, Level max) = delete;

			Result evaluate(const NPCData& a_npcData) const override
			{
				const auto npc = a_npcData.GetNPC();
				const auto skillLevel = npc->playerSkills.values[skill];
				if (skillLevel >= value.first && skillLevel <= value.second) {
					return Result::kPass;
				}
				return Result::kFail;
			}

			std::ostringstream& describe(std::ostringstream& os) const override
			{
				return describeLevel(os, "SKL");
			}
		};

		/// Similar to SkillFilter, but operates with Skill Weights of NPC's Class instead.
		struct SkillWeightFilter : SkillFilter
		{
			using SkillFilter::SkillFilter;

            [[nodiscard]] Result evaluate(const NPCData& a_npcData) const override
			{
				if (const auto skillWeight = weight(a_npcData); *skillWeight >= value.first && *skillWeight <= value.second) {
					return Result::kPass;
				}
				return Result::kFail;
			}

			std::ostringstream& describe(std::ostringstream& os) const override
			{
				return describeLevel(os, "SWE");
			}

		private:
			std::optional<std::uint8_t> weight(const NPCData& a_npcData) const
            {
				if (const auto npcClass = a_npcData.GetNPC()->npcClass) {
					const auto& skillWeights = npcClass->data.skillWeights;
					using Skills = SkillFilter::Skills;
					switch (skill) {
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

		struct SexFilter : ValueFilter<RE::SEX>
		{

			using ValueFilter::ValueFilter;

			Result evaluate(const NPCData& a_npcData) const override
			{
				return a_npcData.GetSex() == value ? Result::kPass : Result::kFail;
			}

		    std::ostringstream& describe(std::ostringstream& os) const override
		    {
				os << (value == RE::SEX::kMale ? "M" : "F");
				return os;
		    }
		};

		struct UniqueFilter : Filter
		{

			Result evaluate(const NPCData& a_npcData) const override
			{
				return a_npcData.IsUnique() ? Result::kPass : Result::kFail;
			}

		    std::ostringstream& describe(std::ostringstream& os) const override
		    {
				os << "U";
				return os;
		    }
		};

		struct SummonableFilter : Filter
		{

			Result evaluate(const NPCData& a_npcData) const override
			{
				return a_npcData.IsSummonable() ? Result::kPass : Result::kFail;
			}

		    std::ostringstream& describe(std::ostringstream& os) const override
		    {
				os << "S";
				return os;
		    }
		};

		struct ChildFilter : Filter
		{

			Result evaluate(const NPCData& a_npcData) const override
			{
				return a_npcData.IsChild() ? Result::kPass : Result::kFail;
			}

			std::ostringstream& describe(std::ostringstream& os) const override
			{
				os << "C";
				return os;
			}

		};

		using chance = std::uint32_t;
		struct ChanceFilter : ValueFilter<chance>
		{
			inline static RNG staticRNG{};

			using ValueFilter::ValueFilter;

			Result evaluate([[maybe_unused]] const NPCData& a_npcData) const override
            {
				if (value >= 100) {
					return Result::kPass;
				}

				const auto rng = staticRNG.Generate<chance>(0, 100);
				return rng > value ? Result::kPass : Result::kFailRNG;
			}

			std::ostringstream& describe(std::ostringstream& os) const override
			{
				if (value >= 100) {
					os << "ALWAYS";
				} else {
					os << "WITH " << value << "% CHANCE";
				}
				return os;
			}
		};
	}
}
