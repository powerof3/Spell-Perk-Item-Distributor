#include "LookupNPC.h"

namespace NPC
{
	void Data::cache_keywords()
	{
		npc->ForEachKeyword([&](const RE::BGSKeyword& a_keyword) {
			keywords.emplace(a_keyword.GetFormEditorID());
			return RE::BSContainer::ForEachResult::kContinue;
		});
		if (const auto race = npc->GetRace()) {
			race->ForEachKeyword([&](const RE::BGSKeyword& a_keyword) {
				keywords.emplace(a_keyword.GetFormEditorID());
				return RE::BSContainer::ForEachResult::kContinue;
			});
		}
	}

    bool Data::is_child(RE::TESNPC* a_npc)
    {
		if (const auto race = a_npc->GetRace()) {
			if (race->IsChildRace()) {
				return true;
			}
			if (race->formEditorID.contains("RaceChild")) {
				return true;
			}
		}
        return false;
    }

    Data::Data(RE::TESNPC* a_npc) :
		npc(a_npc),
		originalFormID(a_npc->GetFormID()),
		name(a_npc->GetName()),
		originalEDID(Cache::EditorID::GetEditorID(npc)),
		level(a_npc->GetLevel()),
		sex(a_npc->GetSex()),
		unique(a_npc->IsUnique()),
		summonable(a_npc->IsSummonable()),
		child(is_child(a_npc))
	{
		cache_keywords();
	}

	Data::Data(RE::Actor* a_actor, RE::TESNPC* a_npc) :
		npc(a_npc),
		name(a_npc->GetName()),
		level(a_npc->GetLevel()),
		sex(a_npc->GetSex()),
		unique(a_npc->IsUnique()),
		summonable(a_npc->IsSummonable()),
		child(is_child(a_npc))
	{
		if (const auto extraLvlCreature = a_actor->extraList.GetByType<RE::ExtraLeveledCreature>()) {
			if (const auto originalBase = extraLvlCreature->originalBase) {
				originalFormID = originalBase->GetFormID();
				originalEDID = Cache::EditorID::GetEditorID(originalBase);
			}
			if (const auto templateBase = extraLvlCreature->templateBase) {
				templateFormID = templateBase->GetFormID();
				templateEDID = Cache::EditorID::GetEditorID(templateBase);
			}
		} else {
			originalFormID = a_npc->GetFormID();
			originalEDID = Cache::EditorID::GetEditorID(npc);
		}
		cache_keywords();
	}

	RE::TESNPC* Data::GetNPC() const
	{
		return npc;
	}

	bool Data::has_keyword_string(const std::string& a_string) const
	{
		return std::ranges::any_of(keywords, [&](const auto& keyword) {
			return string::iequals(keyword, a_string);
		});
	}

	bool Data::contains_keyword_string(const std::string& a_string) const
	{
		return std::ranges::any_of(keywords, [&](const auto& keyword) {
			return string::icontains(keyword, a_string);
		});
	}

	bool Data::HasStringFilter(const StringVec& a_strings, bool a_all) const
	{
		if (a_all) {
			return std::ranges::all_of(a_strings, [&](const auto& str) {
				return has_keyword_string(str);
			});
		} else {
			return std::ranges::any_of(a_strings, [&](const auto& str) {
				return has_keyword_string(str) || string::iequals(name, str) || string::iequals(originalEDID, str) || string::iequals(templateEDID, str);
			});
		}
	}

	bool Data::ContainsStringFilter(const StringVec& a_strings) const
	{
		return std::ranges::any_of(a_strings, [&](const auto& str) {
			return contains_keyword_string(str) || string::icontains(name, str) || string::icontains(originalEDID, str) || string::icontains(templateEDID, str);
		});
	}

	bool Data::InsertKeyword(const char* a_keyword)
	{
		return keywords.emplace(a_keyword).second;
	}

	bool Data::has_form(RE::TESForm* a_form) const
	{
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
			{
				const auto filterFormID = a_form->GetFormID();
				return npc == a_form || originalFormID == filterFormID || templateFormID == filterFormID;
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
					if (result = has_form(&a_formInList); result) {
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

	bool Data::HasFormFilter(const FormVec& a_forms, bool all) const
	{
		const auto has_form_or_file = [&](const std::variant<RE::TESForm*, const RE::TESFile*>& a_formFile) {
			if (std::holds_alternative<RE::TESForm*>(a_formFile)) {
				const auto form = std::get<RE::TESForm*>(a_formFile);
				return form && has_form(form);
			}
			if (std::holds_alternative<const RE::TESFile*>(a_formFile)) {
				const auto file = std::get<const RE::TESFile*>(a_formFile);
				return file && (file->IsFormInMod(originalFormID) || file->IsFormInMod(templateFormID));
			}
			return false;
		};

		if (all) {
			return std::ranges::all_of(a_forms, has_form_or_file);
		} else {
			return std::ranges::any_of(a_forms, has_form_or_file);
		}
	}

	std::uint16_t Data::GetLevel() const
	{
		return level;
	}

	RE::SEX Data::GetSex() const
	{
		return sex;
	}

	bool Data::IsUnique() const
	{
		return unique;
	}

	bool Data::IsSummonable() const
	{
		return summonable;
	}

	bool Data::IsChild() const
	{
		return child;
	}
}
