#include "LookupNPC.h"

namespace NPC
{
	void Data::cache_keywords(RE::TESNPC* a_npc)
	{
		a_npc->ForEachKeyword([&](RE::BGSKeyword& a_keyword) {
			if (const auto keywordEDID = a_keyword.GetFormEditorID(); !string::is_empty(keywordEDID)) {
				keywords.insert(keywordEDID);
			}
			return RE::BSContainer::ForEachResult::kContinue;
		});
		if (const auto race = a_npc->GetRace()) {
			race->ForEachKeyword([&](RE::BGSKeyword& a_keyword) {
				if (const auto keywordEDID = a_keyword.GetFormEditorID(); !string::is_empty(keywordEDID)) {
					keywords.insert(keywordEDID);
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});
		}
	}

	Data::Data(RE::TESNPC* a_npc) :
		npc(a_npc),
		formID(a_npc->GetFormID()),
		name(a_npc->GetName()),
		originalEDID(Cache::EditorID::GetEditorID(npc)),
		level(a_npc->GetLevel()),
		child(a_npc->GetRace() ? a_npc->GetRace()->IsChildRace() : false)
	{
		cache_keywords(a_npc);
	}

	Data::Data(RE::Actor* a_actor, RE::TESNPC* a_npc) :
		npc(a_npc),
		name(a_npc->GetName()),
		level(a_npc->GetLevel()),
		child(a_npc->GetRace() ? a_npc->GetRace()->IsChildRace() : false)
	{
		if (const auto extraLvlCreature = a_actor->extraList.GetByType<RE::ExtraLeveledCreature>()) {
			if (const auto originalBase = extraLvlCreature->originalBase) {
				originalEDID = Cache::EditorID::GetEditorID(originalBase);
			}
			if (const auto templateBase = extraLvlCreature->templateBase) {
				formID = templateBase->GetFormID();
				templateEDID = Cache::EditorID::GetEditorID(templateBase);
			}
		} else {
			formID = a_npc->GetFormID();
			originalEDID = Cache::EditorID::GetEditorID(npc);
		}
		cache_keywords(a_npc);
	}

	RE::TESNPC* Data::GetNPC() const
	{
		return npc;
	}

	RE::FormID Data::GetFormID() const
	{
		return formID;
	}

	std::string Data::GetName() const
	{
		return name;
	}

	std::pair<std::string, std::string> Data::GetEditorID() const
	{
		return { originalEDID, templateEDID };
	}

	std::uint16_t Data::GetLevel() const
	{
		return level;
	}

	bool Data::IsChild() const
	{
		return child;
	}

	bool Data::has_keyword(const std::string& a_string) const
	{
		return std::ranges::any_of(keywords, [&](const auto& keyword) {
			return string::iequals(keyword, a_string);
		});
	}

	bool Data::contains_keyword(const std::string& a_string) const
	{
		return std::ranges::any_of(keywords, [&](const auto& keyword) {
			return string::icontains(keyword, a_string);
		});
	}

	bool Data::HasStringFilter(const StringVec& a_strings) const
	{
		return std::ranges::any_of(a_strings, [&](const auto& str) {
			return has_keyword(str) || string::iequals(name, str) || string::iequals(originalEDID, str) || string::iequals(templateEDID, str);
		});
	}

	bool Data::ContainsStringFilter(const StringVec& a_strings) const
	{
		return std::ranges::any_of(a_strings, [&](const auto& str) {
			return contains_keyword(str) || string::icontains(name, str) || string::icontains(originalEDID, str) || string::icontains(templateEDID, str);
		});
	}

	bool Data::HasAllKeywords(const StringVec& a_strings) const
	{
		return std::ranges::all_of(a_strings, [&](const auto& str) {
			return has_keyword(str);
		});
	}
}
