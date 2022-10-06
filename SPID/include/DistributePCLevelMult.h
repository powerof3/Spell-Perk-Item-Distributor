#pragma once

namespace Distribute
{
	inline std::uint64_t currentPlayerID{ 0 };
	inline std::string currentPlayerIDStr{ "0" };

	inline bool newGameStarted{ false };

	// Skip re-distribution of failed entries for PC Level Mult NPCs, when leveling up
	struct PCLevelMult
	{
		struct Input
		{
			Input(RE::FormID a_npcFormID, std::uint16_t a_npcLevel, bool a_onlyPlayerLevelEntries, bool a_noPlayerLevelDistribution) :
				playerID(currentPlayerID),
				npcFormID(a_npcFormID),
				npcLevel(a_npcLevel),
				onlyPlayerLevelEntries(a_onlyPlayerLevelEntries),
				noPlayerLevelDistribution(a_noPlayerLevelDistribution)
			{
			}

			std::uint64_t playerID;
			RE::FormID npcFormID;
			std::uint16_t npcLevel;
			bool onlyPlayerLevelEntries;
			bool noPlayerLevelDistribution;
		};

		struct Manager
		{
			bool find_rejected_entry(const Input& a_input, RE::FormID a_distributedFormID, std::uint32_t a_formDataIndex) const
			{
				if (a_input.noPlayerLevelDistribution) {
					return false;
				}

				if (const auto idIt = cache.find(a_input.playerID); idIt != cache.end()) {
					auto& npcFormIDMap = idIt->second;
					if (const auto npcIt = npcFormIDMap.find(a_input.npcFormID); npcIt != npcFormIDMap.end()) {
						auto& levelMap = npcIt->second;
						for (const auto& [level, cachedData] : levelMap) {
							if (a_input.npcLevel >= level) {
								if (const auto formIt = cachedData.rejectedEntries.find(a_distributedFormID); formIt != cachedData.rejectedEntries.end()) {
									return formIt->second.contains(a_formDataIndex);
								}
							}
						}
					}
				}

				return false;
			}
			bool insert_rejected_entry(const Input& a_input, RE::FormID a_distributedFormID, std::uint32_t a_formDataIndex)
			{
				if (a_input.noPlayerLevelDistribution) {
					return false;
				}

				return cache[a_input.playerID][a_input.npcFormID][a_input.npcLevel].rejectedEntries[a_distributedFormID].insert(a_formDataIndex).second;
			}
			void dump_rejected_entries()
			{
				for (auto& [playerID, npcFormIDs] : cache) {
					logger::info("PlayerID : {:X}", playerID);
					for (auto& [npcFormID, levelMap] : npcFormIDs) {
						logger::info("	NPC : {} [{:X}]", Cache::EditorID::GetEditorID(npcFormID), npcFormID);
						for (auto& [level, distFormMap] : levelMap) {
							logger::info("		Level : {}", level);
							for (auto& [distFormID, idxSet] : distFormMap.rejectedEntries) {
								logger::info("			Dist FormID : {} [{:X}]", Cache::EditorID::GetEditorID(distFormID), distFormID);
								for (auto& idx : idxSet) {
									logger::info("				IDX : {}", idx);
								}
							}
						}
					}
				}
			}

			bool has_distributed_entry(const Input& a_input)
			{
				if (const auto it = cache.find(a_input.playerID); it != cache.end()) {
					if (const auto npcIt = it->second.find(a_input.npcFormID); npcIt != it->second.end()) {
						return !npcIt->second.empty();
					}
				}
				return false;
			}
			bool insert_distributed_entry(const Input& a_input, RE::FormID a_distributedFormID, IdxOrCount a_idx)
			{
				if (a_input.noPlayerLevelDistribution) {
					return false;
				}

				return cache[a_input.playerID][a_input.npcFormID][a_input.npcLevel].distributedEntries.insert(DistributedEntry(a_distributedFormID, a_idx)).second;
			}
			void for_each_distributed_entry(const Input& a_input, std::function<void(RE::TESForm&, IdxOrCount a_idx, bool)> a_fn) const
			{
				if (a_input.noPlayerLevelDistribution) {
					return;
				}

				if (const auto idIt = cache.find(a_input.playerID); idIt != cache.end()) {
					auto& npcFormIDMap = idIt->second;
					if (const auto npcIt = npcFormIDMap.find(a_input.npcFormID); npcIt != npcFormIDMap.end()) {
						auto& levelMap = npcIt->second;
						for (const auto& [level, cachedData] : levelMap) {
							bool is_below_level = a_input.npcLevel < level;
							for (auto& [formid, idx] : cachedData.distributedEntries) {
								if (auto form = RE::TESForm::LookupByID(formid)) {
									a_fn(*form, idx, is_below_level);
								}
							}
						}
					}
				}
			}
			void dump_distributed_entries()
			{
				for (auto& [playerID, npcFormIDs] : cache) {
					logger::info("PlayerID : {:X}", playerID);
					for (auto& [npcFormID, levelMap] : npcFormIDs) {
						logger::info("	NPC : {} [{:X}]", Cache::EditorID::GetEditorID(npcFormID), npcFormID);
						for (auto& [level, distFormMap] : levelMap) {
							logger::info("		Level : {}", level);
							for (auto& [distFormID, idxSet] : distFormMap.distributedEntries) {
								logger::info("			Dist FormID : {} [{:X}] : {}", Cache::EditorID::GetEditorID(distFormID), distFormID, idxSet);
							}
						}
					}
				}
			}

		private:
			struct DistributedEntry
			{
				RE::FormID form;
				IdxOrCount idxOrCount;

				bool operator<(const DistributedEntry& a_rhs) const
				{
					return this->form < a_rhs.form;
				}
			};

			struct Data
			{
				std::map<RE::FormID, std::set<std::uint32_t>> rejectedEntries;  // Distributed formID, FormData vector index
				std::set<DistributedEntry> distributedEntries;                  // Distributed formID
			};

			std::map<std::uint64_t,          // PlayerID
				std::map<RE::FormID,         // NPC formID
					std::map<std::uint16_t,  // Actor Level
						Data>>>              // Data
				cache{};
		};
	};

	inline PCLevelMult::Manager pcLevelMultManager;

	namespace PlayerLeveledActor
	{
		class Manager : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			static Manager* GetSingleton()
			{
				static Manager singleton;
				return &singleton;
			}

			static void Register();

		protected:
			EventResult ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override;

		private:
			Manager() = default;
			Manager(const Manager&) = delete;
			Manager(Manager&&) = delete;

			~Manager() override = default;

			Manager& operator=(const Manager&) = delete;
			Manager& operator=(Manager&&) = delete;
		};

		void Install();
	}

	void ApplyToPCLevelMultNPCs(RE::TESDataHandler* a_dataHandler);
}
