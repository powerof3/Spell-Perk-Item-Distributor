#pragma once

#include "LookupFilters.h"
#include "LookupForms.h"
#include "PCLevelMultManager.h"

namespace Distribute
{
	namespace detail
	{
		bool uses_template(RE::TESNPC* a_actorbase)
		{
			return a_actorbase->UsesTemplate() || a_actorbase->baseTemplateForm || a_actorbase->templateForms;
		};
	}

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
			auto& [form, idxOrCount, stringFilters, formFilters, levelFilters, traits, chance, npcCount] = formData;
			auto distributedFormID = form->GetFormID();

			if (pcLevelMultManager->FindRejectedEntry(a_input, distributedFormID, idx)) {
				continue;
			}
			if (!Filter::strings(a_actorbase, stringFilters) || !Filter::forms(a_actorbase, formFilters)) {
				continue;
			}
			auto result = Filter::secondary(a_actorbase, levelFilters, traits, chance, a_input.noPlayerLevelDistribution);
			if (result != SECONDARY_RESULT::kPass) {
				if (result == SECONDARY_RESULT::kFailRNG) {
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

			for (auto& formData : a_distributables.forms) {
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
			EventResult ProcessEvent(const RE::TESDeathEvent* a_event, RE::BSTEventSource<RE::TESDeathEvent>*) override;
			EventResult ProcessEvent(const RE::TESFormDeleteEvent* a_event, RE::BSTEventSource<RE::TESFormDeleteEvent>*) override;

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
