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
				if (hasLevelFilters && result == Filter::Result::kFailRNG) {
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

		void add_item(RE::Actor* a_actor, RE::TESBoundObject* a_item, std::uint32_t a_itemCount);
	}

#pragma region Packages, Death Items
	// old method (distributing one by one)
	// for now, only packages/death items use this
	template <class Form>
	void for_each_form(
		const NPCData&                           a_npcData,
		Forms::DataVec<Form>&                    forms,
		const PCLevelMult::Input&                a_input,
		std::function<bool(Form*, IndexOrCount)> a_callback,
		std::set<RE::TESForm*>*                  accumulatedForms = nullptr)
	{
		for (auto& formData : forms) {
			if (!a_npcData.HasMutuallyExclusiveForm(formData.form) && detail::passed_filters(a_npcData, a_input, formData)) {
				if (accumulatedForms) {
					accumulatedForms->insert(formData.form);
				}
				a_callback(formData.form, formData.idxOrCount);
				++formData.npcCount;
			}
		}
	}
#pragma endregion

#pragma region Outfits, Sleep Outfits, Skins
	template <class Form>
	void for_each_form(
		const NPCData&             a_npcData,
		Forms::DataVec<Form>&      forms,
		const PCLevelMult::Input&  a_input,
		std::function<bool(Form*)> a_callback,
		std::set<RE::TESForm*>*    accumulatedForms = nullptr)
	{
		for (auto& formData : forms) {  // Vector is reversed in FinishLookupForms
			if (!a_npcData.HasMutuallyExclusiveForm(formData.form) && detail::passed_filters(a_npcData, a_input, formData) && a_callback(formData.form)) {
				if (accumulatedForms) {
					accumulatedForms->insert(formData.form);
				}
				++formData.npcCount;
				break;
			}
		}
	}
#pragma endregion

	// TODO: Is this unused?
	// outfits/sleep outfits
	template <class Form>
	void for_each_form(
		const NPCData&               a_npcData,
		Forms::Distributables<Form>& a_distributables,
		std::function<bool(Form*)>   a_callback,
		std::set<RE::TESForm*>*      accumulatedForms = nullptr)
	{
		auto& vec = a_distributables.GetForms(false);

		for (auto& formData : vec) {  // Vector is reversed in FinishLookupForms
			if (!a_npcData.HasMutuallyExclusiveForm(formData.form) && detail::passed_filters(a_npcData, formData) && a_callback(formData.form)) {
				if (accumulatedForms) {
					accumulatedForms->insert(formData.form);
				}
				++formData.npcCount;
				break;
			}
		}
	}

#pragma region Items
	// countable items
	template <class Form>
	void for_each_form(
		const NPCData&                               a_npcData,
		Forms::DataVec<Form>&                        forms,
		const PCLevelMult::Input&                    a_input,
		std::function<bool(std::map<Form*, Count>&)> a_callback,
		std::set<RE::TESForm*>*                      accumulatedForms = nullptr)
	{
		std::map<Form*, Count> collectedForms{};

		for (auto& formData : forms) {
			if (!a_npcData.HasMutuallyExclusiveForm(formData.form) && detail::passed_filters(a_npcData, a_input, formData)) {
				// TODO: Safe guard getting RandomCount and if for any reason there is a PackageIndex, default it to count = 1
				auto count = std::get<RandomCount>(formData.idxOrCount).GetRandom(); 
				if (auto leveledItem = formData.form->As<RE::TESLevItem>())
				{
					auto                                level = a_npcData.GetLevel();
					RE::BSScrapArray<RE::CALCED_OBJECT> calcedObjects{};

					leveledItem->CalculateCurrentFormList(level, count, calcedObjects, 0, true);
					for (auto& calcObj : calcedObjects) {
						collectedForms[static_cast<RE::TESBoundObject*>(calcObj.form)] += calcObj.count;
					}
				}
				else
				{
					collectedForms[formData.form] += count;
				}
				++formData.npcCount;
			}
		}

		if (!collectedForms.empty()) {
			if (accumulatedForms) {
				std::ranges::copy(collectedForms | std::views::keys, std::inserter(*accumulatedForms, accumulatedForms->end()));
			}
			a_callback(collectedForms);
		}
	}
#pragma endregion

#pragma region Spells, Perks, Shouts, Keywords
	// spells, perks, shouts, keywords
	// forms that can be added to
	template <class Form>
	void for_each_form(
		NPCData&                                       a_npcData,
		Forms::DataVec<Form>&                          forms,
		const PCLevelMult::Input&                      a_input,
		std::function<void(const std::vector<Form*>&)> a_callback,
		std::set<RE::TESForm*>*                        accumulatedForms = nullptr)
	{
		const auto npc = a_npcData.GetNPC();

		std::vector<Form*> collectedForms{};
		Set<RE::FormID>    collectedFormIDs{};
		Set<RE::FormID>    collectedLeveledFormIDs{};

		collectedForms.reserve(forms.size());
		collectedFormIDs.reserve(forms.size());
		collectedLeveledFormIDs.reserve(forms.size());

		for (auto& formData : forms) {
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
			if (accumulatedForms) {
				accumulatedForms->insert(collectedForms.begin(), collectedForms.end());
			}
			a_callback(collectedForms);
			if (!collectedLeveledFormIDs.empty()) {
				PCLevelMult::Manager::GetSingleton()->InsertDistributedEntry(a_input, Form::FORMTYPE, collectedLeveledFormIDs);
			}
		}
	}
#pragma endregion

	void Distribute(NPCData& a_npcData, const PCLevelMult::Input& a_input);
	void Distribute(NPCData& a_npcData, bool a_onlyLeveledEntries);
}
