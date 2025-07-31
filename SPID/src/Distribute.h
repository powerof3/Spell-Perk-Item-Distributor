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

		/// <summary>
		/// Check that NPC doesn't already have the form that is about to be distributed.
		/// </summary>
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
	}

	using namespace Forms;

#pragma region Packages
	// old method (distributing one by one)
	// for now, only packages use this
	template <class Form>
	void for_each_form(
		const NPCData&                           a_npcData,
		Forms::DataVec<Form>&                    forms,
		const PCLevelMult::Input&                a_input,
		std::function<void(Form*, IndexOrCount)> a_callback,
		DistributedForms*                        accumulatedForms = nullptr)
	{
		for (auto& formData : forms) {
			if (!a_npcData.HasMutuallyExclusiveForm(formData.form) && detail::passed_filters(a_npcData, a_input, formData)) {
				if (accumulatedForms) {
					accumulatedForms->insert({ formData.form, formData.path });
				}
				a_callback(formData.form, formData.idxOrCount);
				++formData.npcCount;
			}
		}
	}
#pragma endregion

#pragma region Outfits, Sleep Outfits, Skins
	template <class Form>
	bool for_first_form(
		const NPCData&                           a_npcData,
		Forms::DataVec<Form>&                    forms,
		const PCLevelMult::Input&                a_input,
		std::function<bool(Form*, bool isFinal)> a_callback,
		DistributedForms*                        accumulatedForms = nullptr)
	{
		for (auto& formData : forms) {
			if (!a_npcData.HasMutuallyExclusiveForm(formData.form) && detail::passed_filters(a_npcData, a_input, formData) && a_callback(formData.form, formData.isFinal)) {
				if (accumulatedForms) {
					accumulatedForms->insert({ formData.form, formData.path });
				}
				++formData.npcCount;
				return true;
			}
		}

		return false;
	}
#pragma endregion

#pragma region Items
	// countable items
	template <class Form>
	void for_each_form(
		const NPCData&                               a_npcData,
		Forms::DataVec<Form>&                        forms,
		const PCLevelMult::Input&                    a_input,
		std::function<bool(std::map<Form*, Count>&)> a_callback,
		DistributedForms*                            accumulatedForms = nullptr)
	{
		std::map<Form*, Count> collectedForms{};

		for (auto& formData : forms) {
			if (!a_npcData.HasMutuallyExclusiveForm(formData.form) && detail::passed_filters(a_npcData, a_input, formData)) {
				auto count = std::get<RandomCount>(formData.idxOrCount).GetRandom();
				if (auto leveledItem = formData.form->As<RE::TESLevItem>()) {
					auto                                level = a_npcData.GetLevel();
					RE::BSScrapArray<RE::CALCED_OBJECT> calcedObjects{};

					leveledItem->CalculateCurrentFormList(level, count, calcedObjects, 0, true);
					for (auto& calcObj : calcedObjects) {
						collectedForms[static_cast<RE::TESBoundObject*>(calcObj.form)] += calcObj.count;
						if (accumulatedForms) {
							accumulatedForms->insert({ calcObj.form, formData.path });
						}
					}
				} else {
					collectedForms[formData.form] += count;
					if (accumulatedForms) {
						accumulatedForms->insert({ formData.form, formData.path });
					}
				}
				++formData.npcCount;
			}
		}

		if (!collectedForms.empty()) {
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
		DistributedForms*                              accumulatedForms = nullptr)
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
					if (accumulatedForms) {
						accumulatedForms->insert({ form, formData.path });
					}
					++formData.npcCount;
				}
			} else {
				if (!a_npcData.HasMutuallyExclusiveForm(form) && detail::passed_filters(a_npcData, a_input, formData) && !detail::has_form(npc, form) && collectedFormIDs.emplace(formID).second) {
					collectedForms.emplace_back(form);
					if (formData.filters.HasLevelFilters()) {
						collectedLeveledFormIDs.emplace(formID);
					}
					if (accumulatedForms) {
						accumulatedForms->insert({ form, formData.path });
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
#pragma endregion

	using OutfitDistributor = std::function<bool(const NPCData&, RE::BGSOutfit*, bool isFinal)>;

	/// <summary>
	/// Performs distribution of all configured forms to NPC described with npcData and input.
	/// </summary>
	/// <param name="npcData">General information about NPC that is being processed.</param>
	/// <param name="input">Leveling information about NPC that is being processed.</param>
	/// <param name="forms">A set of forms that should be distributed to NPC.</param>
	/// <param name="accumulatedForms">An optional pointer to a set that will accumulate all distributed forms.</param>
	/// <param name="outfitDistributor">A function to be called to distribute outfits.</param>
	void Distribute(NPCData&, const PCLevelMult::Input&, Forms::DistributionSet& forms, DistributedForms* accumulatedForms, OutfitDistributor);

	/// <summary>
	/// Invokes appropriate distribution for given NPC.
	///
	/// When NPC is dead a Death Distribution will be invoked, otherwise a normal distribution takes place.
	/// </summary>
	/// <param name="npcData">General information about NPC that is being processed.</param>
	/// <param name="onlyLeveledEntries"> Flag indicating that distribution is invoked by a leveling event and only entries with LevelFilters needs to be processed.</param>
	void Distribute(NPCData& npcData, bool onlyLeveledEntries);

	/// <summary>
	/// Performs distribution of outfits to NPC described with npcData and input.
	///
	/// Unlike general Distribute function, this one is expected to be called once for each actor (rather than once for each NPC).
	/// </summary>
	/// <param name="npcData"></param>
	/// <param name="onlyLeveledEntries"></param>
	void DistributeOutfits(NPCData& npcData, bool onlyLeveledEntries);

	void LogDistribution(const DistributedForms& forms, NPCData& npcData, bool append, const char* prefix = "");
}
