#pragma once

#include "LookupForms.h"

namespace Distribute
{
	namespace detail
	{
		bool uses_template(const RE::TESNPC* a_npc);
	}

	template <class Form>
	void list_npc_count(std::string_view a_recordType, Forms::Distributables<Form>& a_distributables, const size_t a_totalNPCCount)
	{
		if (a_distributables) {
			logger::info("\t{}", a_recordType);

			// Group the same entries together to show total number of distributed records in the log.
			std::map<RE::FormID, Forms::Data<Form>> sums{};
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
						name = Cache::EditorID::GetEditorID(form);
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

	namespace Actor
	{
		inline std::once_flag lookupForms;

	    void Install();
	}

	namespace LeveledActor
	{
		void Install();
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

	// Distribute to all unique and static NPCs, after data load
	void OnInit();
}
