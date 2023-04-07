#pragma once

#include "Expressions.h"
#include "LookupNPC.h"

// Data --------------------
namespace filters
{
	using namespace Expressions;

	using Result = Expressions::Result;
	using NPCExpression = Expression<NPCData>;
	using NPCAndExpression = AndExpression<NPCData>;
	using NPCOrExpression = OrExpression<NPCData>;
	using NPCFilter = Filter<NPCData>;
	template <typename Value>
	using NPCValueFilter = ValueFilter<NPCData, Value>;

	struct Data
	{
		Data(NPCExpression* filters);

		std::shared_ptr<NPCExpression> filters;
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
	/// Form filter that wasn't bound to an actual RE::TESForm.
	///	This filter will always fail and will log a warning when described.
	struct UnknownFormIDFilter : NPCValueFilter<FormOrEditorID>
	{
		using ValueFilter::ValueFilter;

		// UnknownFormIDFilter is never valid and renders the whole entry containing it invalid, thus causing it to being skipped.
		[[nodiscard]] bool isValid() const override { return false; }

		[[nodiscard]] Result evaluate([[maybe_unused]] const NPCData& a_npcData) const override
		{
			return Result::kFail;
		}

		std::ostringstream& describe(std::ostringstream& os) const override
		{
			os << "<UNKNOWN> ";
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

	using chance = std::uint32_t;
	struct ChanceFilter : NPCValueFilter<chance>
	{
		inline static RNG staticRNG{};

		using ValueFilter::ValueFilter;

		[[nodiscard]] bool isSuperfluous() const override
		{
			return value >= MAX;
		}

		[[nodiscard]] Result evaluate([[maybe_unused]] const NPCData& a_npcData) const override
		{
			if (value >= MAX) {
				return Result::kPass;
			}
			if (value <= 0) {
				return Result::kDiscard;
			}

			const auto rng = staticRNG.Generate<chance>(0, MAX);
			return rng > value ? Result::kPass : Result::kDiscard;
		}

		std::ostringstream& describe(std::ostringstream& os) const override
		{
			os << "WITH " << value << "% CHANCE";
			return os;
		}

	private:
		static constexpr chance MAX = 100;
	};
}

// Forms -----------------------------------
namespace filters
{
	struct FormFilter : NPCValueFilter<RE::TESForm*>
	{
		using ValueFilter::ValueFilter;

		[[nodiscard]] Result evaluate(const NPCData& a_npcData) const override
		{
			if (value && has_form(value, a_npcData)) {
				return Result::kPass;
			}
			return Result::kFail;
		}

		std::ostringstream& describe(std::ostringstream& os) const override
		{
			if (value) {
				os << "HAS \"" << value << "\"";
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

	struct KeywordFilter : NPCValueFilter<RE::BGSKeyword*>
	{
		using ValueFilter::ValueFilter;

		[[nodiscard]] Result evaluate(const NPCData& a_npcData) const override
		{
			return a_npcData.HasKeyword(value) ? Result::kPass : Result::kFail;
		}

		std::ostringstream& describe(std::ostringstream& os) const override
		{
			if (value) {
				os << "HAS \"" << value << "\"";
			}
			return os;
		}
	};

	struct ModFilter : NPCValueFilter<const RE::TESFile*>
	{
		using ValueFilter::ValueFilter;

		[[nodiscard]] Result evaluate(const NPCData& a_npcData) const override
		{
			if (value && (value->IsFormInMod(a_npcData.GetOriginalFormID()) || value->IsFormInMod(a_npcData.GetTemplateFormID()))) {
				return Result::kPass;
			}
			return Result::kFail;
		}

		std::ostringstream& describe(std::ostringstream& os) const override
		{
			if (value) {
				os << "IS FROM \"" << value->fileName << "\" MOD";
			}
			return os;
		}
	};

}

// Names -----------------------------------
namespace filters
{
	/// String value for a filter that represents a wildcard.
	/// The value must be without asterisks (e.g. filter "*Vampire" should be trimmed to "Vampire")
	struct WildcardFilter : NPCValueFilter<std::string>
	{
		using ValueFilter::ValueFilter;

		[[nodiscard]] Result evaluate(const NPCData& npcData) const override
		{
			// Utilize regex here. (will also support one-sided wildcards (e.g. *Name and Name*)
			// std::regex regex(filter.value, std::regex_constants::icase);
			// std::regex_match(keyword, regex);
			if (string::icontains(npcData.GetName(), value) ||
				string::icontains(npcData.GetOriginalEDID(), value) ||
				string::icontains(npcData.GetTemplateEDID(), value)) {
				return Result::kPass;
			}
			return Result::kFail;
		}

		std::ostringstream& describe(std::ostringstream& os) const override
		{
			os << "NAME CONTAINS \"" << value << "\"";
			return os;
		}
	};

	/// String value for a filter that represents an exact match.
	/// The value is stored as-is.
	struct MatchFilter : NPCValueFilter<std::string>
	{
		using ValueFilter::ValueFilter;

		[[nodiscard]] Result evaluate(const NPCData& a_npcData) const override
		{
			if (string::iequals(a_npcData.GetName(), value) ||
				string::iequals(a_npcData.GetOriginalEDID(), value) ||
				string::iequals(a_npcData.GetTemplateEDID(), value)) {
				return Result::kPass;
			}
			return Result::kFail;
		}

		std::ostringstream& describe(std::ostringstream& os) const override
		{
			os << "IS NAMED \"" << value << "\"";
			return os;
		}
	};
}

// Levels ----------------------------------
namespace filters
{
	using Level = std::uint8_t;
	using LevelPair = std::pair<Level, Level>;
	/// Value that represents a range of acceptable Actor levels.
	struct LevelFilter : NPCValueFilter<LevelPair>
	{
		inline constexpr static Level MinLevel = 0;
		inline constexpr static Level MaxLevel = UINT8_MAX;

		LevelFilter(const Level min, const Level max) :
			ValueFilter()
		{
			const auto bound_min = min == MaxLevel ? MinLevel : min;
			this->value = { std::min(bound_min, max), std::max(bound_min, max) };
		}

		LevelFilter(LevelPair value) = delete;
		LevelFilter(const LevelPair& value) = delete;

		[[nodiscard]] Result evaluate(const NPCData& a_npcData) const override
		{
			const auto actorLevel = a_npcData.GetLevel();
			if (actorLevel >= value.first && actorLevel <= value.second) {
				return Result::kPass;
			}
			return Result::kFail;
		}

		std::ostringstream& describe(std::ostringstream& os) const override
		{
			return describeLevel(os, "LEVEL");
		}

		[[nodiscard]] bool isSuperfluous() const override
		{
			return value.first == MinLevel && value.second == MaxLevel;
		}

	protected:
		std::ostringstream& describeLevel(std::ostringstream& os, const std::string& name) const
		{
			const int min = value.first;
			const int max = value.second;
			os << name << " IS ";
			if (value.first == value.second) {
				os << "EXACTLY " << min;
			} else if (value.first == MinLevel) {
				os << "LESS THAN " << max;
			} else if (value.second == MaxLevel) {
				os << "GREATER THAN " << min;
			} else {
				os << "BETWEEN " << min << " AND " << max;
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

		[[nodiscard]] Result evaluate(const NPCData& a_npcData) const override
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
			return describeLevel(os, skillName(skill) + " SKILL");
		}

	protected:
		static std::string skillName(const Skill& skill)
		{
			switch (skill) {
			case Skills::kOneHanded:
				return "ONE-HANDED";
			case Skills::kTwoHanded:
				return "TWO-HANDED";
			case Skills::kMarksman:
				return "ARCHERY";
			case Skills::kBlock:
				return "BLOCK";
			case Skills::kSmithing:
				return "SMITHING";
			case Skills::kHeavyArmor:
				return "HEAVY ARMOR";
			case Skills::kLightArmor:
				return "LIGHT ARMOR";
			case Skills::kPickpocket:
				return "PICKPOCKET";
			case Skills::kLockpicking:
				return "LOCKPICKING";
			case Skills::kSneak:
				return "SNEAK";
			case Skills::kAlchemy:
				return "ALCHEMY";
			case Skills::kSpecchcraft:
				return "SPEECH";
			case Skills::kAlteration:
				return "ALTERATION";
			case Skills::kConjuration:
				return "CONJURATION";
			case Skills::kDestruction:
				return "DESTRUCTION";
			case Skills::kIllusion:
				return "ILLUSION";
			case Skills::kRestoration:
				return "RESTORATION";
			case Skills::kEnchanting:
				return "ENCHANTING";
			default:
				return "UNKNOWN";
			}
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
			return describeLevel(os, skillName(skill) + " SKILL WEIGHT");
		}

	private:
		[[nodiscard]] std::optional<std::uint8_t> weight(const NPCData& a_npcData) const
		{
			if (const auto npcClass = a_npcData.GetNPC()->npcClass) {
				const auto& skillWeights = npcClass->data.skillWeights;
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
}

// Traits ----------------------------------
namespace filters
{
	struct SexFilter : NPCValueFilter<RE::SEX>
	{
		using ValueFilter::ValueFilter;

		[[nodiscard]] Result evaluate(const NPCData& a_npcData) const override
		{
			return a_npcData.GetSex() == value ? Result::kPass : Result::kFail;
		}

		std::ostringstream& describe(std::ostringstream& os) const override
		{
			os << "IS A " << (value == RE::SEX::kMale ? "MALE" : "FEMALE");
			return os;
		}
	};

	struct UniqueFilter : NPCFilter
	{
		[[nodiscard]] Result evaluate(const NPCData& a_npcData) const override
		{
			return a_npcData.IsUnique() ? Result::kPass : Result::kFail;
		}

		std::ostringstream& describe(std::ostringstream& os) const override
		{
			os << "IS UNIQUE";
			return os;
		}
	};

	struct SummonableFilter : NPCFilter
	{
		[[nodiscard]] Result evaluate(const NPCData& a_npcData) const override
		{
			return a_npcData.IsSummonable() ? Result::kPass : Result::kFail;
		}

		std::ostringstream& describe(std::ostringstream& os) const override
		{
			os << "IS A SUMMON";
			return os;
		}
	};

	struct ChildFilter : NPCFilter
	{
		[[nodiscard]] Result evaluate(const NPCData& a_npcData) const override
		{
			return a_npcData.IsChild() ? Result::kPass : Result::kFail;
		}

		std::ostringstream& describe(std::ostringstream& os) const override
		{
			os << "IS A CHILD";
			return os;
		}
	};
}
