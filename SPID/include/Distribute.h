#pragma once

#include "LookupFilters.h"
#include "LookupForms.h"
#include "PCLevelMultManager.h"

namespace Distribute
{
	template <class Form>
	void for_each_form(
		RE::TESNPC& a_actorbase,
		Forms::FormMap<Form>& a_formDataMap,
		const PCLevelMult::Input& a_input,
		std::function<bool(const FormCount<Form>&)> a_fn)
	{
		auto& map = a_input.onlyPlayerLevelEntries ? a_formDataMap.formsWithLevels : a_formDataMap.forms;

	    const auto pcLevelMultManager = PCLevelMult::Manager::GetSingleton();

		for (auto& [form, data] : map) {
			if (form != nullptr) {
				auto formID = form->GetFormID();

				auto& [npcCount, formDataVec] = data;
				for (std::uint32_t idx = 0; auto& formData : formDataVec) {
					++idx;
					if (pcLevelMultManager->FindRejectedEntry(a_input, formID, idx)) {
						continue;
					}
					if (!Filter::strings(a_actorbase, formData) || !Filter::forms(a_actorbase, formData)) {
						continue;
					}
					auto result = Filter::secondary(a_actorbase, formData, a_input.noPlayerLevelDistribution);
				    if (result != Filter::SECONDARY_RESULT::kPass) {
						if (result == Filter::SECONDARY_RESULT::kFailDueToRNG) {
							pcLevelMultManager->InsertRejectedEntry(a_input, formID, idx);
						}
						continue;
					}
					auto idxOrCount = std::get<DATA::TYPE::kIdxOrCount>(formData);
					if (a_fn({ form, idxOrCount })) {
						pcLevelMultManager->InsertDistributedEntry(a_input, formID, idxOrCount);
					    ++npcCount;
					}
				}
			}
		}
	}

	template <class Form>
	void list_npc_count(std::string_view a_recordType, Forms::FormMap<Form>& a_formDataMap, const size_t a_totalNPCCount)
	{
		logger::info("	{}s", a_recordType);

		for (auto& [form, formData] : a_formDataMap.forms) {
			if (form != nullptr) {
				auto& [npcCount, data] = formData;
				std::string name;
				if constexpr (std::is_same_v<Form, RE::BGSKeyword>) {
					name = form->GetFormEditorID();
				} else {
					name = Cache::EditorID::GetEditorID(form->GetFormID());
				}
				if (auto file = form->GetFile(0)) {
					logger::info("		{} [0x{:X}~{}] added to {}/{} NPCs", name, form->GetLocalFormID(), file->GetFilename(), npcCount, a_totalNPCCount);
				} else {
					logger::info("		{} [0x{:X}] added to {}/{} NPCs", name, form->GetFormID(), npcCount, a_totalNPCCount);
				}
			}
		}
	}

	namespace DeathItem
	{
		class Manager : public RE::BSTEventSink<RE::TESDeathEvent>
		{
		public:
			static Manager* GetSingleton()
			{
				static Manager singleton;
				return &singleton;
			}

			static void Register();

		protected:
			EventResult ProcessEvent(const RE::TESDeathEvent* a_event, RE::BSTEventSource<RE::TESDeathEvent>*) override;

		private:
			Manager() = default;
			Manager(const Manager&) = delete;
			Manager(Manager&&) = delete;

			~Manager() override = default;

			Manager& operator=(const Manager&) = delete;
			Manager& operator=(Manager&&) = delete;
		};
	}

	namespace LeveledActor
	{
		void Install();
	}

	void Distribute(RE::TESNPC* a_actorbase, bool a_onlyPlayerLevelEntries, bool a_noPlayerLevelDistribution);

	void ApplyToNPCs();
}
