#include "Filters.h"

namespace Filter
{
	// ---------------- Strings ----------------

	template <>
	struct filter_eval<std::string>
	{
		static bool evaluate(const std::string filter, const NPCData& a_npcData)
		{
			return a_npcData.GetKeywords().contains(filter) ||
			       string::iequals(a_npcData.GetName(), filter) ||
			       string::iequals(a_npcData.GetOriginalEDID(), filter) ||
			       string::iequals(a_npcData.GetTemplateEDID(), filter);
		}
	};

	template <>
	struct filter_eval<Wildcard>
	{
		static bool evaluate(const Wildcard filter, const NPCData& a_npcData)
		{
			// Utilize regex here. (will also support one-sided wildcards (e.g. *Name and Name*)
			// std::regex regex(filter.value, std::regex_constants::icase);
			// std::regex_match(keyword, regex);
			return std::ranges::any_of(a_npcData.GetKeywords(), [&](auto keyword) { return string::icontains(keyword, filter.value); }) ||
			       string::icontains(a_npcData.GetName(), filter.value) ||
			       string::icontains(a_npcData.GetOriginalEDID(), filter.value) ||
			       string::icontains(a_npcData.GetTemplateEDID(), filter.value);
		}
	};

	// ---------------- Forms ----------------

	template <>
	struct filter_eval<FormOrMod>
	{
		static bool evaluate(const FormOrMod filter, const NPCData& a_npcData)
		{
			if (std::holds_alternative<RE::TESForm*>(filter)) {
				const auto form = std::get<RE::TESForm*>(filter);
				return form && has_form(form, a_npcData);
			}
			if (std::holds_alternative<const RE::TESFile*>(filter)) {
				const auto file = std::get<const RE::TESFile*>(filter);
				return file && (file->IsFormInMod(a_npcData.GetOriginalFormID()) || file->IsFormInMod(a_npcData.GetTemplateFormID()));
			}
			return false;
		}

	private:
		/// Determines whether given form is associated with an NPC.
		static bool has_form(RE::TESForm* a_form, const NPCData& a_npcData)
		{
			auto npc = a_npcData.GetNPC();
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
				return npc->GetRace() == a_form;
			case RE::FormType::Outfit:
				return npc->defaultOutfit == a_form;
			case RE::FormType::NPC:
				return npc == a_form;
			case RE::FormType::VoiceType:
				return npc->voiceType == a_form;
			case RE::FormType::Spell:
				{
					const auto spell = a_form->As<RE::SpellItem>();
					return npc->GetSpellList()->GetIndex(spell).has_value();
				}
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
		static bool evaluate(const LevelRange filter, const NPCData& a_npcData)
		{
			const auto actorLevel = a_npcData.GetLevel();
			return actorLevel >= filter.min && actorLevel <= filter.max;
		}
	};

	template <>
	struct filter_eval<SkillLevelRange>
	{
		static bool evaluate(const SkillLevelRange filter, const NPCData& a_npcData)
		{
			const auto npc = a_npcData.GetNPC();
			const auto skillLevel = npc->playerSkills.values[filter.skill];
			return skillLevel >= filter.min && skillLevel <= filter.max;
		}
	};

	template <>
	struct filter_eval<SkillWeightRange>
	{
		static bool evaluate(const SkillLevelRange filter, const NPCData& a_npcData)
		{
			if (auto skillWeight = weight(filter, a_npcData)) {
				return skillWeight >= filter.min && skillWeight <= filter.max;
			}
			return false;
		}

	private:
		static std::optional<std::uint8_t> weight(const SkillLevelRange filter, const NPCData& a_npcData)
		{
			const auto& skillWeights = a_npcData.GetNPC()->npcClass->data.skillWeights;

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
	};

	// ---------------- Traits ----------------

	template <>
	struct filter_eval<SexTrait>
	{
		static bool evaluate([[maybe_unused]] const SexTrait filter, const NPCData& a_npcData)
		{
			return a_npcData.GetSex() == filter.sex;
		}
	};

	template <>
	struct filter_eval<UniqueTrait>
	{
		static bool evaluate([[maybe_unused]] const UniqueTrait filter, const NPCData& a_npcData)
		{
			return a_npcData.IsUnique();
		}
	};

	template <>
	struct filter_eval<SummonableTrait>
	{
		static bool evaluate([[maybe_unused]] const SummonableTrait filter, const NPCData& a_npcData)
		{
			return a_npcData.IsSummonable();
		}
	};

	template <>
	struct filter_eval<ChildTrait>
	{
		static bool evaluate([[maybe_unused]] const ChildTrait filter, const NPCData& a_npcData)
		{
			return a_npcData.IsChild();
		}
	};

	template <>
	struct filter_eval<Chance>
	{
		inline static RNG staticRNG;

		static bool evaluate(const Chance filter, [[maybe_unused]] const NPCData& a_npcData)
		{
			if (filter.maximum >= 100) {
				return true;
			}

			const auto rng = staticRNG.Generate<Chance::chance>(0, 100);
			return rng > filter.maximum;
		}
	};
}
