#include "LookupFilters.h"
#include "LookupForms.h"

namespace Filter
{
	namespace detail
	{
		namespace stringF
		{
			bool contains(RE::TESNPC* a_actorbase, const std::string& a_name, const std::pair<std::string, std::string>& a_edid, const StringVec& a_strings)
			{
				return std::ranges::any_of(a_strings, [&](const auto& str) {
					return a_actorbase->ContainsKeyword(str) || string::icontains(a_name, str) || string::icontains(a_edid.first, str) || string::icontains(a_edid.second, str);
				});
			}

			bool matches(RE::TESNPC* a_actorbase, const std::string& a_name, const std::pair<std::string, std::string>& a_edid, const StringVec& a_strings)
			{
				return std::ranges::any_of(a_strings, [&](const auto& str) {
					return a_actorbase->HasApplicableKeywordString(str) || string::iequals(a_name, str) || string::iequals(a_edid.first, str) || string::iequals(a_edid.second, str);
				});
			}

			bool matches(RE::TESNPC* a_actorbase, const StringVec& a_strings)
			{
				const auto matches = [&](const auto& str) {
					return a_actorbase->HasApplicableKeywordString(str);
				};

				return std::ranges::all_of(a_strings, matches);
			}
		}

		namespace formF
		{
			bool get_type(RE::TESNPC* a_actorbase, RE::TESForm* a_filter)
			{
				switch (a_filter->GetFormType()) {
				case RE::FormType::CombatStyle:
					return a_actorbase->GetCombatStyle() == a_filter;
				case RE::FormType::Class:
					return a_actorbase->npcClass == a_filter;
				case RE::FormType::Faction:
					{
						const auto faction = a_filter->As<RE::TESFaction>();
						return a_actorbase->IsInFaction(faction);
					}
				case RE::FormType::Race:
					return a_actorbase->GetRace() == a_filter;
				case RE::FormType::Outfit:
					return a_actorbase->defaultOutfit == a_filter;
				case RE::FormType::NPC:
					return a_actorbase == a_filter;
				case RE::FormType::VoiceType:
					return a_actorbase->voiceType == a_filter;
				case RE::FormType::Spell:
					{
						const auto spell = a_filter->As<RE::SpellItem>();
						const auto spellList = a_actorbase->GetSpellList();
						return spellList && spellList->GetIndex(spell).has_value();
					}
				case RE::FormType::FormList:
					{
						bool result = false;

						auto list = a_filter->As<RE::BGSListForm>();
						list->ForEachForm([&](RE::TESForm& a_form) {
							if (result = get_type(a_actorbase, &a_form); result) {
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

			bool matches(const NPCData& a_npcData, const FormVec& a_forms, bool a_matchesAll = false)
			{
				const auto npc = a_npcData.GetNPC();
				const auto [originalFormID, templateFormID] = a_npcData.GetFormID();
				
				const auto has_form_or_file = [&](const std::variant<RE::TESForm*, const RE::TESFile*>& a_formFile) {
					if (std::holds_alternative<RE::TESForm*>(a_formFile)) {
						auto form = std::get<RE::TESForm*>(a_formFile);
						return form && get_type(npc, form);
					}
					if (std::holds_alternative<const RE::TESFile*>(a_formFile)) {
						auto file = std::get<const RE::TESFile*>(a_formFile);
						return file && (file->IsFormInMod(originalFormID) || file->IsFormInMod(templateFormID));
					}
					return false;
				};

				if (a_matchesAll) {
					return std::ranges::all_of(a_forms, has_form_or_file);
				} else {
					return std::ranges::any_of(a_forms, has_form_or_file);
				}
			}
		}
	}

	bool strings(const NPCData& a_npcData, const StringFilters& a_stringFilters)
	{
		auto& [strings_ALL, strings_NOT, strings_MATCH, strings_ANY] = a_stringFilters;

		const auto npc = a_npcData.GetNPC();
		
		if (!strings_ALL.empty() && !detail::stringF::matches(npc, strings_ALL)) {
			return false;
		}

		const std::string name = npc->GetName();
		const auto edids = a_npcData.GetEditorID();

		if (!strings_NOT.empty() && detail::stringF::matches(npc, name, edids, strings_NOT)) {
			return false;
		}

		if (!strings_MATCH.empty() && !detail::stringF::matches(npc, name, edids, strings_MATCH)) {
			return false;
		}

		if (!strings_ANY.empty() && !detail::stringF::contains(npc, name, edids, strings_ANY)) {
			return false;
		}

		return true;
	}

	bool forms(const NPCData& a_npcData, const FormFilters& a_formFilters)
	{
		auto& [filterForms_ALL, filterForms_NOT, filterForms_MATCH] = a_formFilters;

		if (!filterForms_ALL.empty() && !detail::formF::matches(a_npcData, filterForms_ALL, true)) {
			return false;
		}

		if (!filterForms_NOT.empty() && detail::formF::matches(a_npcData, filterForms_NOT)) {
			return false;
		}

		if (!filterForms_MATCH.empty() && !detail::formF::matches(a_npcData, filterForms_MATCH)) {
			return false;
		}

		return true;
	}

	SECONDARY_RESULT secondary(const NPCData& a_npcData, const LevelFilters& a_levelFilters, const Traits& a_traits, float a_chance, bool a_noPlayerLevelDistribution)
	{
		constexpr auto failed = SECONDARY_RESULT::kFail;

		const auto npc = a_npcData.GetNPC();
		
		if (Lookup::detail::has_level_filters(a_levelFilters)) {
			if (a_noPlayerLevelDistribution && npc->HasPCLevelMult()) {
				return failed;
			}
		}

		// Actor Level
		auto& [actorMin, actorMax] = std::get<0>(a_levelFilters);
		const auto actorLevel = npc->GetLevel();

		if (actorMin < UINT16_MAX && actorMax < UINT16_MAX) {
			if (actorLevel < actorMin || actorLevel > actorMax) {
				return failed;
			}
		} else if (actorMin < UINT16_MAX && actorLevel < actorMin) {
			return failed;
		} else if (actorMax < UINT16_MAX && actorLevel > actorMax) {
			return failed;
		}

		// Skill Level
		for (auto& [skillType, skill] : std::get<1>(a_levelFilters)) {
			auto& [skillMin, skillMax] = skill;

			const auto skillLevel = npc->playerSkills.values[skillType];

			if (skillMin < UINT8_MAX && skillMax < UINT8_MAX) {
				if (skillLevel < skillMin || skillLevel > skillMax) {
					return failed;
				}
			} else if (skillMin < UINT8_MAX && skillLevel < skillMin) {
				return failed;
			} else if (skillMax < UINT8_MAX && skillLevel > skillMax) {
				return failed;
			}
		}

		const auto& skillWeights = npc->npcClass->data.skillWeights;

		// Skill Weight
		for (auto& [skillType, skill] : std::get<2>(a_levelFilters)) {
			auto& [skillMin, skillMax] = skill;

			std::uint8_t skillWeight = skillWeights.oneHanded;
			using Skill = RE::TESNPC::Skills;
			switch (skillType) {
			case Skill::kOneHanded:
				skillWeight = skillWeights.oneHanded;
				break;
			case Skill::kTwoHanded:
				skillWeight = skillWeights.twoHanded;
				break;
			case Skill::kMarksman:
				skillWeight = skillWeights.archery;
				break;
			case Skill::kBlock:
				skillWeight = skillWeights.block;
				break;
			case Skill::kSmithing:
				skillWeight = skillWeights.smithing;
				break;
			case Skill::kHeavyArmor:
				skillWeight = skillWeights.heavyArmor;
				break;
			case Skill::kLightArmor:
				skillWeight = skillWeights.lightArmor;
				break;
			case Skill::kPickpocket:
				skillWeight = skillWeights.pickpocket;
				break;
			case Skill::kLockpicking:
				skillWeight = skillWeights.lockpicking;
				break;
			case Skill::kSneak:
				skillWeight = skillWeights.sneak;
				break;
			case Skill::kAlchemy:
				skillWeight = skillWeights.alchemy;
				break;
			case Skill::kSpecchcraft:
				skillWeight = skillWeights.speech;
				break;
			case Skill::kAlteration:
				skillWeight = skillWeights.alteration;
				break;
			case Skill::kConjuration:
				skillWeight = skillWeights.conjuration;
				break;
			case Skill::kDestruction:
				skillWeight = skillWeights.destruction;
				break;
			case Skill::kIllusion:
				skillWeight = skillWeights.illusion;
				break;
			case Skill::kRestoration:
				skillWeight = skillWeights.restoration;
				break;
			case Skill::kEnchanting:
				skillWeight = skillWeights.enchanting;
				break;
			default:
				continue;
			}

			if (skillMin < UINT8_MAX && skillMax < UINT8_MAX) {
				if (skillWeight < skillMin || skillWeight > skillMax) {
					return failed;
				}
			} else if (skillMin < UINT8_MAX && skillWeight < skillMin) {
				return failed;
			} else if (skillMax < UINT8_MAX && skillWeight > skillMax) {
				return failed;
			}
		}

		const auto& [sex, isUnique, isSummonable, isChild] = a_traits;
		if (sex && npc->GetSex() != *sex) {
			return failed;
		}
		if (isUnique && npc->IsUnique() != *isUnique) {
			return failed;
		}
		if (isSummonable && npc->IsSummonable() != *isSummonable) {
			return failed;
		}
		if (isChild && (npc->race && npc->race->IsChildRace()) != *isChild) {
			return failed;
		}

		if (!numeric::essentially_equal(a_chance, 100.0f)) {
			if (const auto rng = staticRNG.Generate<float>(0.0f, 100.0f); rng > a_chance) {
				return SECONDARY_RESULT::kFailRNG;
			}
		}

		return SECONDARY_RESULT::kPass;
	}
}
