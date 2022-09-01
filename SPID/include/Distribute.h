#pragma once

#include "LookupFilters.h"
#include "LookupForms.h"

namespace Distribute
{
	// Skip re-distribution of failed entries for PC Level Mult NPCs, when leveling up
    struct PCLevelExclusion
	{
		struct Input
		{
			Input(std::uint64_t a_playerID, RE::FormID a_npcFormID, std::uint16_t a_npcLevel, bool a_onlyPlayerLevelEntries, bool a_noPlayerLevelDistribution) :
				playerID(a_playerID),
				npcFormID(a_npcFormID),
				npcLevel(a_npcLevel),
				onlyPlayerLevelEntries(a_onlyPlayerLevelEntries),
				noPlayerLevelDistribution(a_noPlayerLevelDistribution)
			{}

			std::uint64_t playerID;
			RE::FormID npcFormID;
			std::uint16_t npcLevel;
			bool onlyPlayerLevelEntries;
			bool noPlayerLevelDistribution;
		};

		struct Map
		{
			std::map<std::uint64_t, std::map<RE::FormID,                                       // PlayerID, NPC formID
										std::map<std::uint16_t, std::map<RE::FormID,           // Actor Level, Form formID
																	std::set<std::size_t>>>>>  // FormData vector index
				map;

			bool find(const Input& a_data, RE::FormID a_itemFormID, std::size_t a_formDataIndex)
			{
				if (a_data.noPlayerLevelDistribution) {
					return false;
				}

			    if (const auto npcIt = map[a_data.playerID].find(a_data.npcFormID); npcIt != map[a_data.playerID].end()) {
					if (const auto levelIt = std::ranges::find_if(npcIt->second, [&](const auto& a_levelMap) {
							return a_data.npcLevel >= a_levelMap.first;  // npc matches level or higher
						});
						levelIt != npcIt->second.end()) {
						if (const auto formIt = levelIt->second.find(a_itemFormID); formIt != levelIt->second.end()) {
							return formIt->second.contains(a_formDataIndex);
						}
					}
				}

			    return false;
			}

			bool insert(const Input& a_data, RE::FormID a_itemFormID, std::size_t a_formDataIndex)
			{
				if (a_data.noPlayerLevelDistribution) {
					return false;
				}

			    return map[a_data.playerID][a_data.npcFormID][a_data.npcLevel][a_itemFormID].insert(a_formDataIndex).second;
			}
		};
	};

	inline PCLevelExclusion::Map pcLevelExclusionMap;

	template <class Form>
	void for_each_form(
		RE::TESNPC& a_actorbase,
		Forms::FormMap<Form>& a_formDataMap,
		const PCLevelExclusion::Input& a_input,
		std::function<bool(const FormCount<Form>&)> a_fn)
	{
		auto& map = a_input.onlyPlayerLevelEntries ? a_formDataMap.formsWithLevels : a_formDataMap.forms;

		for (auto& [form, data] : map) {
			if (form != nullptr) {
				auto formID = form->GetFormID();

				auto& [npcCount, formDataVec] = data;
				for (size_t idx = 0; auto& formData : formDataVec) {
					++idx;
					if (pcLevelExclusionMap.find(a_input, formID, idx) || !Filter::strings(a_actorbase, formData) || !Filter::forms(a_actorbase, formData)) {
						continue;
					}
					if (auto result = Filter::secondary(a_actorbase, formData, a_input.noPlayerLevelDistribution); result != Filter::SECONDARY_RESULT::kPass) {
						if (result == Filter::SECONDARY_RESULT::kFailDueToRNG) {
							pcLevelExclusionMap.insert(a_input, formID, idx);
						}
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

	void Distribute(RE::TESNPC* a_actorbase, bool a_onlyPlayerLevelEntries, bool a_noPlayerLevelDistribution);

	void ApplyToNPCs();
}
