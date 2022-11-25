#pragma once

#include "LookupForms.h"
#include "LookupNPC.h"
#include "PCLevelMultManager.h"

namespace Distribute
{
	namespace detail
	{
		template <class Form>
		bool passed_filters(
			const NPCData& a_npcData,
			const PCLevelMult::Input& a_input,
			Forms::Data<Form>& a_formData,
			std::uint32_t idx)
		{
			const auto pcLevelMultManager = PCLevelMult::Manager::GetSingleton();

			auto distributedFormID = a_formData.form->GetFormID();
			if (pcLevelMultManager->FindRejectedEntry(a_input, distributedFormID, idx)) {
				return false;
			}

			auto result = a_formData.filters.PassedFilters(a_npcData, a_input.noPlayerLevelDistribution);
			if (result != Filter::Result::kPass) {
				if (result == Filter::Result::kFailRNG) {
					pcLevelMultManager->InsertRejectedEntry(a_input, distributedFormID, idx);
				}
				return false;
			}
			return true;
		}

		template <class Form>
		bool has_form(RE::TESNPC* a_npc, Form* a_form)
		{
			if constexpr (std::is_same_v<RE::TESFaction, Form>) {
				return a_npc->IsInFaction(a_form);
			} else if constexpr (std::is_same_v<RE::BGSPerk, Form>) {
				return a_npc->GetPerkIndex(a_form).has_value();
			} else if constexpr (std::is_same_v<RE::SpellItem, Form> || std::is_same_v<RE::TESShout, Form> || std::is_same_v<RE::TESLevSpell, Form>) {
				const auto actorEffects = a_npc->GetSpellList();
				return actorEffects && actorEffects->GetIndex(a_form).has_value();
			} else if constexpr (std::is_same_v<RE::TESForm, Form>) {
				if (a_form->Is(RE::TESPackage::FORMTYPE)) {
					auto package = a_form->As<RE::TESPackage>();
					auto& packageList = a_npc->aiPackages.packages;
					return std::ranges::find(packageList, package) != packageList.end();
				} else {
					return false;
				}
			} else {
				return false;
			}
		}
	}

	// items, death items
	// wip (packages need to have both add and overwrite behaviors)
	template <class Form>
	void for_each_form(
		const NPCData& a_npcData,
		Forms::Distributables<Form>& a_distributables,
		const PCLevelMult::Input& a_input,
		std::function<bool(Form*, IdxOrCount&)> a_callback)
	{
		auto& vec = a_input.onlyPlayerLevelEntries ? a_distributables.formsWithLevels : a_distributables.forms;

		std::uint32_t vecIdx = 0;
		for (auto& formData : vec) {
			++vecIdx;
			if (detail::passed_filters(a_npcData, a_input, formData, vecIdx)) {
				auto form = formData.form;
				auto idxOrCount = formData.idxOrCount;
				if (a_callback(form, idxOrCount)) {
					++formData.npcCount;
				}
			}
		}
	}

	// outfits/sleep outfits
	// overridable forms
	template <class Form>
	void for_each_form(
		const NPCData& a_npcData,
		Forms::Distributables<Form>& a_distributables,
		const PCLevelMult::Input& a_input,
		std::function<bool(Form*)> a_callback)
	{
		auto& vec = a_input.onlyPlayerLevelEntries ? a_distributables.formsWithLevels : a_distributables.forms;

		std::uint32_t vecIdx = 0;
		for (auto& formData : vec | std::views::reverse) {  //iterate from last inserted config (Zzz -> Aaaa)
			++vecIdx;
			if (detail::passed_filters(a_npcData, a_input, formData, vecIdx)) {
				auto form = formData.form;
				if (a_callback(form)) {
					++formData.npcCount;
					break;
				}
			}
		}
	}

	// spells, perks, shouts, keywords
	// forms that can be added to
	template <class Form>
	void for_each_form(
		NPCData& a_npcData,
		Forms::Distributables<Form>& a_distributables,
		const PCLevelMult::Input& a_input,
		std::function<void(const std::vector<Form*>&)> a_callback)
	{
		auto& vec = a_input.onlyPlayerLevelEntries ? a_distributables.formsWithLevels : a_distributables.forms;

		if (vec.empty()) {
			return;
		}

    	const auto npc = a_npcData.GetNPC();

		std::vector<Form*> collectedForms{};
		collectedForms.reserve(vec.size());

		Set<RE::FormID> collectedFormIDs{};

		std::uint32_t vecIdx = 0;
		for (auto& formData : vec) {
			++vecIdx;
			auto form = formData.form;
			auto formID = form->GetFormID();
			if (collectedFormIDs.contains(formID)) {
				continue;
			}
			if constexpr (std::is_same_v<RE::BGSKeyword, Form>) {
				if (detail::passed_filters(a_npcData, a_input, formData, vecIdx) && a_npcData.InsertKeyword(form->GetFormEditorID())) {
					collectedForms.emplace_back(form);
					collectedFormIDs.emplace(formID);
					++formData.npcCount;
				}
			} else {
				if (detail::passed_filters(a_npcData, a_input, formData, vecIdx) && !detail::has_form(npc, form) && collectedFormIDs.emplace(formID).second) {
					collectedForms.emplace_back(form);
					++formData.npcCount;
				}
			}
		}

		if (!collectedForms.empty()) {
			a_callback(collectedForms);

		    PCLevelMult::Manager::GetSingleton()->InsertDistributedEntry(a_input, Form::FORMTYPE, collectedFormIDs);
		}
	}

	void Distribute(NPCData& a_npcData, const PCLevelMult::Input& a_input);
}
