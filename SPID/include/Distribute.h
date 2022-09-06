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
			[[nodiscard]] bool find(const Input& a_input, RE::FormID a_distributedFormID, std::uint32_t a_formDataIndex) const
			{
				if (a_input.noPlayerLevelDistribution) {
					return false;
				}

				if (const auto idIt = map.find(a_input.playerID); idIt != map.end()) {
					auto& npcFormIDMap = idIt->second;
					if (const auto npcIt = npcFormIDMap.find(a_input.npcFormID); npcIt != npcFormIDMap.end()) {
						auto& levelMap = npcIt->second; 
					    for (const auto& [level, distFormIDMap] : levelMap) {
							if (a_input.npcLevel >= level) {
								if (const auto formIt = distFormIDMap.find(a_distributedFormID); formIt != distFormIDMap.end()) {
									return formIt->second.contains(a_formDataIndex);
								}
							}
						}
					}
				}

				return false;
			}

			bool insert(const Input& a_input, RE::FormID a_distributedFormID, std::uint32_t a_formDataIndex)
			{
				if (a_input.noPlayerLevelDistribution) {
					return false;
				}

				return map[a_input.playerID][a_input.npcFormID][a_input.npcLevel][a_distributedFormID].insert(a_formDataIndex).second;
			}

			void dump()
			{
				for (auto& [playerID, npcFormIDs] : map) {
					logger::info("PlayerID : {}", playerID);
					for (auto& [npcFormID, levelMap] : npcFormIDs) {
						logger::info("	NPC : {} [{:X}]", Cache::EditorID::GetEditorID(npcFormID), npcFormID);
						for (auto& [level, distFormMap] : levelMap) {
							logger::info("		Level : {}", level);
							for (auto& [distFormID, idxSet] : distFormMap) {
								logger::info("			Dist FormID : {} [{:X}]", Cache::EditorID::GetEditorID(distFormID), distFormID);
								for (auto& idx : idxSet) {
									logger::info("				IDX : {}", idx);
								}
							}
						}
					}
				}
			}

		private:
			std::map<std::uint64_t,                      // PlayerID
				std::map<RE::FormID,                     // NPC formID
					std::map<std::uint16_t,              // Actor Level
						std::map<RE::FormID,             // Distributed formID
							std::set<std::uint32_t>>>>>  // FormData vector index
				map{};
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
				for (std::uint32_t idx = 0; auto& formData : formDataVec) {
					++idx;
					if (pcLevelExclusionMap.find(a_input, formID, idx)) {
						continue;
					}
					if (!Filter::strings(a_actorbase, formData) || !Filter::forms(a_actorbase, formData)) {
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
