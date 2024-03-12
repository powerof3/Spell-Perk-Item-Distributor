#pragma once

#include "FormData.h"
#include "LookupNPC.h"
#include "PCLevelMultManager.h"

namespace Distribute
{
	namespace detail
	{
		template <class Form>
		bool passed_filters(
			const NPCData&            a_npcData,
			const PCLevelMult::Input& a_input,
			const Forms::Data<Form>&  a_formData)
		{
			const auto pcLevelMultManager = PCLevelMult::Manager::GetSingleton();

			const auto hasLevelFilters = a_formData.filters.HasLevelFilters();
			const auto distributedFormID = a_formData.form->GetFormID();
			const auto index = a_formData.index;

			if (hasLevelFilters && pcLevelMultManager->FindRejectedEntry(a_input, distributedFormID, index)) {
				return false;
			}

			auto result = a_formData.filters.PassedFilters(a_npcData);

			if (result != Filter::Result::kPass) {
				if (result == Filter::Result::kFailRNG && hasLevelFilters) {
					pcLevelMultManager->InsertRejectedEntry(a_input, distributedFormID, index);
				}
				return false;
			}

			return true;
		}

		template <class Form>
		bool passed_filters(
			const NPCData&           a_npcData,
			const Forms::Data<Form>& a_formData)
		{
			return a_formData.filters.PassedFilters(a_npcData) == Filter::Result::kPass;
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
					auto  package = a_form->As<RE::TESPackage>();
					auto& packageList = a_npc->aiPackages.packages;
					return std::ranges::find(packageList, package) != packageList.end();
				} else {
					return false;
				}
			} else {
				return false;
			}
		}

		void equip_worn_outfit(RE::Actor* actor, const RE::BGSOutfit* a_outfit);
		void add_item(RE::Actor* a_actor, RE::TESBoundObject* a_item, std::uint32_t a_itemCount);
		void init_leveled_items(RE::Actor* a_actor);
		bool can_equip_outfit(const RE::TESNPC* a_npc, RE::BGSOutfit* a_outfit);
	}

	// old method (distributing one by one)
	// for now, only packages/death items use this
	template <class Form>
	void for_each_form(
		const NPCData&                         a_npcData,
		Forms::Distributables<Form>&           a_distributables,
		const PCLevelMult::Input&              a_input,
		std::function<bool(Form*, IdxOrCount)> a_callback)
	{
		auto& vec = a_distributables.GetForms(a_input.onlyPlayerLevelEntries);

		for (auto& formData : vec) {
			if (!a_npcData.HasMutuallyExclusiveForm(formData.form) && detail::passed_filters(a_npcData, a_input, formData)) {
				a_callback(formData.form, formData.idxOrCount);
				++formData.npcCount;
			}
		}
	}

	// outfits/sleep outfits
	// skins
	template <class Form>
	void for_each_form(
		const NPCData&               a_npcData,
		Forms::Distributables<Form>& a_distributables,
		const PCLevelMult::Input&    a_input,
		std::function<bool(Form*)>   a_callback)
	{
		auto& vec = a_distributables.GetForms(a_input.onlyPlayerLevelEntries);

		for (auto& formData : vec) {  // Vector is reversed in FinishLookupForms
			if (!a_npcData.HasMutuallyExclusiveForm(formData.form) && detail::passed_filters(a_npcData, a_input, formData) && a_callback(formData.form)) {
				++formData.npcCount;
				break;
			}
		}
	}

	// outfits/sleep outfits
	template <class Form>
	void for_each_form(
		const NPCData&               a_npcData,
		Forms::Distributables<Form>& a_distributables,
		std::function<bool(Form*)>   a_callback)
	{
		auto& vec = a_distributables.GetForms(false);

		for (auto& formData : vec) {  // Vector is reversed in FinishLookupForms
			if (!a_npcData.HasMutuallyExclusiveForm(formData.form) && detail::passed_filters(a_npcData, formData) && a_callback(formData.form)) {
				++formData.npcCount;
				break;
			}
		}
	}

	// items
	template <class Form>
	void for_each_form(
		const NPCData&                                          a_npcData,
		Forms::Distributables<Form>&                            a_distributables,
		const PCLevelMult::Input&                               a_input,
		std::function<bool(std::map<Form*, IdxOrCount>&, bool)> a_callback)
	{
		auto& vec = a_distributables.GetForms(a_input.onlyPlayerLevelEntries);

		if (vec.empty()) {
			return;
		}

		std::map<Form*, IdxOrCount> collectedForms{};
		bool                        hasLeveledItems = false;

		for (auto& formData : vec) {
			if (!a_npcData.HasMutuallyExclusiveForm(formData.form) && detail::passed_filters(a_npcData, a_input, formData)) {
				if (formData.form->Is(RE::FormType::LeveledItem)) {
					hasLeveledItems = true;
				}
				collectedForms[formData.form] += formData.idxOrCount;
				++formData.npcCount;
			}
		}

		if (!collectedForms.empty()) {
			a_callback(collectedForms, hasLeveledItems);
		}
	}

	// spells, perks, shouts, keywords
	// forms that can be added to
	template <class Form>
	void for_each_form(
		NPCData&                                       a_npcData,
		Forms::Distributables<Form>&                   a_distributables,
		const PCLevelMult::Input&                      a_input,
		std::function<void(const std::vector<Form*>&)> a_callback)
	{
		auto& vec = a_distributables.GetForms(a_input.onlyPlayerLevelEntries);

		if (vec.empty()) {
			return;
		}

		const auto npc = a_npcData.GetNPC();

		std::vector<Form*> collectedForms{};
		Set<RE::FormID>    collectedFormIDs{};
		Set<RE::FormID>    collectedLeveledFormIDs{};

		collectedForms.reserve(vec.size());
		collectedFormIDs.reserve(vec.size());
		collectedLeveledFormIDs.reserve(vec.size());

		for (auto& formData : vec) {
			auto form = formData.form;
			auto formID = form->GetFormID();
			if (collectedFormIDs.contains(formID)) {
				continue;
			}
			if constexpr (std::is_same_v<RE::BGSKeyword, Form>) {
				if (!a_npcData.HasMutuallyExclusiveForm(form) && detail::passed_filters(a_npcData, a_input, formData) && a_npcData.InsertKeyword(form->GetFormEditorID())) {
					collectedForms.emplace_back(form);
					collectedFormIDs.emplace(formID);
					if (formData.filters.HasLevelFilters()) {
						collectedLeveledFormIDs.emplace(formID);
					}
					++formData.npcCount;
				}
			} else {
				if (!a_npcData.HasMutuallyExclusiveForm(form) && detail::passed_filters(a_npcData, a_input, formData) && !detail::has_form(npc, form) && collectedFormIDs.emplace(formID).second) {
					collectedForms.emplace_back(form);
					if (formData.filters.HasLevelFilters()) {
						collectedLeveledFormIDs.emplace(formID);
					}
					++formData.npcCount;
				}
			}
		}

		if (!collectedForms.empty()) {
			a_callback(collectedForms);
			if (!collectedLeveledFormIDs.empty()) {
				PCLevelMult::Manager::GetSingleton()->InsertDistributedEntry(a_input, Form::FORMTYPE, collectedLeveledFormIDs);
			}
		}
	}

	template <class Form>
	void for_each_form(
		NPCData&                                       a_npcData,
		Forms::Distributables<Form>&                   a_distributables,
		std::function<void(const std::vector<Form*>&)> a_callback)
	{
		const auto& vec = a_distributables.GetForms(false);

		if (vec.empty()) {
			return;
		}

		const auto npc = a_npcData.GetNPC();

		std::vector<Form*> collectedForms{};
		Set<RE::FormID>    collectedFormIDs{};

		collectedForms.reserve(vec.size());
		collectedFormIDs.reserve(vec.size());

		for (auto& formData : vec) {
			auto form = formData.form;
			auto formID = form->GetFormID();
			if (collectedFormIDs.contains(formID)) {
				continue;
			}
			if constexpr (std::is_same_v<RE::BGSKeyword, Form>) {
				if (!a_npcData.HasMutuallyExclusiveForm(form) && detail::passed_filters(a_npcData, formData) && a_npcData.InsertKeyword(form->GetFormEditorID())) {
					collectedForms.emplace_back(form);
					collectedFormIDs.emplace(formID);
					++formData.npcCount;
				}
			} else {
				if (!a_npcData.HasMutuallyExclusiveForm(form) && detail::passed_filters(a_npcData, formData) && !detail::has_form(npc, form) && collectedFormIDs.emplace(formID).second) {
					collectedForms.emplace_back(form);
					++formData.npcCount;
				}
			}
		}

		if (!collectedForms.empty()) {
			a_callback(collectedForms);
		}
	}

	void Distribute(NPCData& a_npcData, const PCLevelMult::Input& a_input);
	void DistributeItemOutfits(NPCData& a_npcData, const PCLevelMult::Input& a_input);

	void Distribute(NPCData& a_npcData, bool a_onlyLeveledEntries, bool a_noItemOutfits = false);
}
