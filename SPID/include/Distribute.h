#pragma once

#include "LookupForms.h"
#include "PCLevelMultManager.h"

namespace Distribute
{
	template <class Form>
	void for_each_form(
		RE::TESNPC& a_actorbase,
		Forms::Distributables<Form>& a_distributables,
		const PCLevelMult::Input& a_input,
		std::function<bool(Form*, IdxOrCount&)> a_fn)
	{
		auto& vec = a_input.onlyPlayerLevelEntries ? a_distributables.formsWithLevels : a_distributables.forms;

		const auto pcLevelMultManager = PCLevelMult::Manager::GetSingleton();

		for (std::uint32_t idx = 0; auto& formData : vec) {
			++idx;
			auto& [form, idxOrCount, filters, npcCount] = formData;
			auto distributedFormID = form->GetFormID();

			if (pcLevelMultManager->FindRejectedEntry(a_input, distributedFormID, idx)) {
				continue;
			}

		    auto result = filters.PassedFilters(a_actorbase, a_input.noPlayerLevelDistribution);
			if (result != Filter::Result::kPass) {
				if (result == Filter::Result::kFailRNG) {
					pcLevelMultManager->InsertRejectedEntry(a_input, distributedFormID, idx);
				}
				continue;
			}

		    if (a_fn(form, idxOrCount)) {
				pcLevelMultManager->InsertDistributedEntry(a_input, distributedFormID, idxOrCount);
				++npcCount;
			}
		}
	}

	template <class Form>
	void list_npc_count(std::string_view a_recordType, Forms::Distributables<Form>& a_distributables, const size_t a_totalNPCCount)
	{
		if (a_distributables) {
			logger::info("\t{}", a_recordType);

			// Group the same entries together to show total number of distributed records in the log.
			std::map<RE::FormID, Forms::FormData<Form>> sums{};
			for (auto& formData : a_distributables.forms) {
				if (const auto& form = formData.form) {
					auto it = sums.find(form->GetFormID());
					if (it != sums.end()) {
						it->second.npcCount += formData.npcCount;
					} else {
						sums.insert({ form->GetFormID(), formData });
					}
				}
			}

			for (auto& entry : sums) {
				auto& formData = entry.second;
				if (const auto& form = formData.form) {
					std::string name{};
					if constexpr (std::is_same_v<Form, RE::BGSKeyword>) {
						name = form->GetFormEditorID();
					} else {
						name = Cache::EditorID::GetEditorID(form->GetFormID());
					}
					if (auto file = form->GetFile(0)) {
						logger::info("\t\t{} [0x{:X}~{}] added to {}/{} NPCs", name, form->GetLocalFormID(), file->GetFilename(), formData.npcCount, a_totalNPCCount);
					} else {
						logger::info("\t\t{} [0x{:X}] added to {}/{} NPCs", name, form->GetFormID(), formData.npcCount, a_totalNPCCount);
					}
				}
			}
		}
	}

	namespace Event
	{
		class Manager :
			public RE::BSTEventSink<RE::TESDeathEvent>,
			public RE::BSTEventSink<RE::TESFormDeleteEvent>
		{
		public:
			static Manager* GetSingleton()
			{
				static Manager singleton;
				return &singleton;
			}

			static void Register();

		protected:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESDeathEvent* a_event, RE::BSTEventSource<RE::TESDeathEvent>*) override;
			RE::BSEventNotifyControl ProcessEvent(const RE::TESFormDeleteEvent* a_event, RE::BSTEventSource<RE::TESFormDeleteEvent>*) override;

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

	void Distribute(RE::TESNPC* a_actorbase, const PCLevelMult::Input& a_input);

	void ApplyToNPCs();
}
