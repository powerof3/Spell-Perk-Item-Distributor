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

	bool Data::ShouldProcessNPC() const
	{
		if (keywords.contains(processedKeywordEDID)) {
			return false;
		}

		if (!processedKeyword) {
			const auto factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSKeyword>();
			if (const auto keyword = factory ? factory->Create() : nullptr) {
				keyword->formEditorID = processedKeywordEDID;
				processedKeyword = keyword;
			}
		}

		if (processedKeyword) {
			npc->AddKeyword(processedKeyword);
		}

		return true;
	}

	bool Data::InsertKeyword(const char* a_keyword)
	{
		return keywords.emplace(a_keyword).second;
	}

	RE::TESNPC* Data::GetNPC() const
	{
		return npc;
	}

	StringSet Data::GetKeywords() const
	{
		return keywords;
	}

	std::string Data::GetName() const
	{
		return name;
	}
	std::string Data::GetOriginalEDID() const
	{
		return originalEDID;
	}
	std::string Data::GetTemplateEDID() const
	{
		return templateEDID;
	}

	RE::FormID Data::GetOriginalFormID() const
	{
		return originalFormID;
	}
	RE::FormID Data::GetTemplateFormID() const
	{
		return templateFormID;
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
