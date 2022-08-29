#pragma once

#include "LookupFilters.h"
#include "LookupForms.h"

namespace Distribute
{
	inline std::set<RE::FormID> pcMultNPCs;

    template <class Form>
	void for_each_form(
		RE::TESNPC& a_actorbase,
		Forms::FormMap<Form>& a_formDataMap,
		std::function<bool(const FormCount<Form>&)> a_fn)
	{
		for (auto& [form, data] : a_formDataMap.forms) {
			if (form != nullptr) {
				auto& [npcCount, formDataVec] = data;
				for (auto& formData : formDataVec) {
					if (!Filter::strings(a_actorbase, formData) || !Filter::forms(a_actorbase, formData) || !Filter::secondary(a_actorbase, formData)) {
						continue;
					}
					auto idxOrCount = std::get<DATA::TYPE::kIdxOrCount>(formData);
					if (a_fn({ form, idxOrCount })) {
						++npcCount;
					}
				}
			}
		}
	}

	template <class Form>
	void list_npc_count(const std::string& a_type, Forms::FormMap<Form>& a_formDataMap, const size_t a_totalNPCCount)
	{
		logger::info("	{}", a_type);

		for (auto& [form, formData] : a_formDataMap.forms) {
			if (form != nullptr) {
				auto& [npcCount, data] = formData;
				std::string name;
				if constexpr (std::is_same_v<Form, RE::BGSKeyword>) {
					name = form->GetFormEditorID();
				} else {
					name = Cache::EditorID::GetEditorID(form->GetFormID());
				}
				logger::info("		{} [0x{:X}] added to {}/{} NPCs", name, form->GetFormID(), npcCount, a_totalNPCCount);
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
			using EventResult = RE::BSEventNotifyControl;

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

	namespace PlayerLeveledActor
	{
		void Install();
	}

	void Distribute(RE::TESNPC* a_actorbase);

	void ApplyToNPCs();
}
