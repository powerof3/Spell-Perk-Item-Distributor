#pragma once

#include "LookupForms.h"
#include "LookupNPC.h"
#include "PCLevelMultManager.h"

namespace Distribute
{
	namespace detail
	{
		template <class Form>
		bool passed_filters(const NPCData& a_npcData, const PCLevelMult::Input& a_input, Forms::Data<Form>& a_formData, std::uint32_t idx)
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
			if constexpr (std::is_same_v<RE::BGSKeyword, Form>) {
				return a_npc->GetKeywordIndex(a_form).has_value();
			} else if constexpr (std::is_same_v<RE::TESFaction, Form>) {
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
					PCLevelMult::Manager::GetSingleton()->InsertDistributedEntry(a_input, form->GetFormID(), idxOrCount);
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
		for (auto& formData : vec | std::views::reverse) { //iterate from last inserted config (Zzz -> Aaaa)
			++vecIdx;
			if (detail::passed_filters(a_npcData, a_input, formData, vecIdx)) {
				auto form = formData.form;
				if (a_callback(form)) {
					PCLevelMult::Manager::GetSingleton()->InsertDistributedEntry(a_input, form->GetFormID(), formData.idxOrCount);
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
		[[maybe_unused]] const NPCData& a_npcData,
		Forms::Distributables<Form>& a_distributables,
		const PCLevelMult::Input& a_input,
		std::function<void(const std::vector<Form*>&)> a_callback)
	{
		auto& vec = a_input.onlyPlayerLevelEntries ? a_distributables.formsWithLevels : a_distributables.forms;

		if (vec.empty()) {
			return;
		}

		std::set<Form*> collectedFormSet{};
		std::vector<Form*> collectedForms{};
		collectedForms.reserve(vec.size());

		std::uint32_t vecIdx = 0;
		for (auto& formData : vec) {
			++vecIdx;
			if (detail::passed_filters(a_npcData, a_input, formData, vecIdx)) {
				auto form = formData.form;
				if (collectedFormSet.insert(form).second && !detail::has_form(a_npcData.GetNPC(), form)) {
					PCLevelMult::Manager::GetSingleton()->InsertDistributedEntry(a_input, form->GetFormID(), formData.idxOrCount);
					++formData.npcCount;
					collectedForms.emplace_back(form);
				}
			}
		}

		if (!collectedForms.empty()) {
			a_callback(collectedForms);
		}
	}

	void Distribute(const NPCData& a_npcData, const PCLevelMult::Input& a_input);
}
