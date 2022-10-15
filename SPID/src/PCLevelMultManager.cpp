#include "PCLevelMultManager.h"
#include "DistributePCLevelMult.h"

namespace PCLevelMult
{
	Input::Input(const RE::TESNPC* a_npc, bool a_onlyPlayerLevelEntries, bool a_noPlayerLevelDistribution) :
		playerID(Manager::GetSingleton()->GetCurrentPlayerID()),
		npcFormID(a_npc->GetFormID()),
		npcLevel(a_npc->GetLevel()),
		npcLevelCap(a_npc->actorData.calcLevelMax),
		onlyPlayerLevelEntries(a_onlyPlayerLevelEntries),
		noPlayerLevelDistribution(a_noPlayerLevelDistribution)
	{}

	void Manager::Register()
	{
		if (auto UI = RE::UI::GetSingleton()) {
			UI->AddEventSink(this);
			logger::info("	Registered {}"sv, typeid(Manager).name());
		}
	}

	EventResult Manager::ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
	{
		if (a_event && a_event->menuName == RE::RaceSexMenu::MENU_NAME) {
			if (a_event->opening) {
				oldPlayerID = get_game_playerID();
			} else {
				const auto newPlayerID = get_game_playerID();
				if (newGameStarted) {
					newGameStarted = false;

					currentPlayerID = newPlayerID;

					if (const auto dataHandler = RE::TESDataHandler::GetSingleton()) {
						Distribute::ApplyToPCLevelMultNPCs(dataHandler);
					}
				} else if (oldPlayerID != newPlayerID) {
					remap_player_ids(oldPlayerID, newPlayerID);
				}
			}
		}
		return EventResult::kContinue;
	}

	bool Manager::FindRejectedEntry(const Input& a_input, RE::FormID a_distributedFormID, std::uint32_t a_formDataIndex) const
	{
		if (a_input.noPlayerLevelDistribution) {
			return false;
		}

		if (const auto idIt = cache.find(a_input.playerID); idIt != cache.end()) {
			auto& npcFormIDMap = idIt->second;
			if (const auto npcIt = npcFormIDMap.find(a_input.npcFormID); npcIt != npcFormIDMap.end()) {
				auto& [levelCapState, levelMap] = npcIt->second;
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

	bool Manager::InsertRejectedEntry(const Input& a_input, RE::FormID a_distributedFormID, std::uint32_t a_formDataIndex)
	{
		if (a_input.noPlayerLevelDistribution) {
			return false;
		}

		return cache[a_input.playerID]
		            [a_input.npcFormID]
		                .entries[a_input.npcLevel]
		                .rejectedEntries[a_distributedFormID]
		                .insert(a_formDataIndex)
		                .second;
	}

	void Manager::DumpRejectedEntries()
	{
		for (auto& [playerID, npcFormIDs] : cache) {
			logger::info("PlayerID : {:X}", playerID);
			for (auto& [npcFormID, levelMap] : npcFormIDs) {
				logger::info("	NPC : {} [{:X}]", Cache::EditorID::GetEditorID(npcFormID), npcFormID);
				for (auto& [level, distFormMap] : levelMap.entries) {
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

	bool Manager::FindDistributedEntry(const Input& a_input)
	{
		if (const auto it = cache.find(a_input.playerID); it != cache.end()) {
			if (const auto npcIt = it->second.find(a_input.npcFormID); npcIt != it->second.end()) {
				return !npcIt->second.entries.empty();
			}
		}
		return false;
	}

	bool Manager::InsertDistributedEntry(const Input& a_input, RE::FormID a_distributedFormID, IdxOrCount a_idx)
	{
		if (a_input.noPlayerLevelDistribution) {
			return false;
		}

		return cache[a_input.playerID][a_input.npcFormID].entries[a_input.npcLevel].distributedEntries.insert({ a_distributedFormID, a_idx }).second;
	}

	void Manager::ForEachDistributedEntry(const Input& a_input, std::function<void(RE::TESForm&, IdxOrCount a_idx, bool)> a_fn) const
	{
		if (a_input.noPlayerLevelDistribution) {
			return;
		}

		if (const auto idIt = cache.find(a_input.playerID); idIt != cache.end()) {
			auto& npcFormIDMap = idIt->second;
			if (const auto npcIt = npcFormIDMap.find(a_input.npcFormID); npcIt != npcFormIDMap.end()) {
				auto& [levelCapState, levelMap] = npcIt->second;
				for (const auto& [level, cachedData] : levelMap) {
					const bool is_below_level = a_input.npcLevel < level;
					for (auto& [formid, idx] : cachedData.distributedEntries) {
						if (auto form = RE::TESForm::LookupByID(formid)) {
							a_fn(*form, idx, is_below_level);
						}
					}
				}
			}
		}
	}

	void Manager::DumpDistributedEntries()
	{
		for (auto& [playerID, npcFormIDs] : cache) {
			logger::info("PlayerID : {:X}", playerID);
			for (auto& [npcFormID, levelMap] : npcFormIDs) {
				logger::info("	NPC : {} [{:X}]", Cache::EditorID::GetEditorID(npcFormID), npcFormID);
				for (auto& [level, distFormMap] : levelMap.entries) {
					logger::info("		Level : {}", level);
					for (auto& [distFormID, idxSet] : distFormMap.distributedEntries) {
						logger::info("			Dist FormID : {} [{:X}] : {}", Cache::EditorID::GetEditorID(distFormID), distFormID, idxSet);
					}
				}
			}
		}
	}

	bool Manager::HasHitLevelCap(const Input& a_input)
	{
		bool hitCap = (a_input.npcLevel == a_input.npcLevelCap);

		auto& map = cache[a_input.playerID];
		if (const auto it = map.find(a_input.npcFormID); it == map.end()) {
			map[a_input.npcFormID].levelCapState = static_cast<LEVEL_CAP_STATE>(hitCap);
		} else {
			auto& levelCapState = it->second.levelCapState;
			if (hitCap) {
				if (levelCapState == LEVEL_CAP_STATE::kHit) {
					return true;
				}
				levelCapState = LEVEL_CAP_STATE::kHit;
			} else {
				levelCapState = LEVEL_CAP_STATE::kNotHit;
			}
		}

		return false;
	}

	std::uint64_t Manager::GetCurrentPlayerID()
	{
		if (currentPlayerID == 0) {
			currentPlayerID = get_game_playerID();
		}

		return currentPlayerID;
	}

	void Manager::GetPlayerIDFromSave(const std::string& a_saveName)
	{
		// Quicksave0_2A73F01A_0_6E656C736F6E_Tamriel_000002_20220918174138_10_1.ess
		// 2A73F01A is player ID

		if (const auto save = string::split(a_saveName, "_"); save.size() > 1 && !string::is_only_letter(save[1])) {
			currentPlayerID = string::lexical_cast<std::uint64_t>(save[1], true);
		} else {
			logger::info("Loaded non-standard save : {}", a_saveName);
		    currentPlayerID = 0;  // non standard save name, use game playerID instead
		}
	}

	void Manager::SetNewGameStarted()
	{
		newGameStarted = true;
	}

	std::uint64_t Manager::get_game_playerID()
	{
		return RE::BGSSaveLoadManager::GetSingleton()->currentPlayerID & 0xFFFFFFFF;
	}

	void Manager::remap_player_ids(std::uint64_t a_oldID, std::uint64_t a_newID)
	{
		if (!cache.contains(a_newID)) {
			if (const auto it = cache.find(a_oldID); it != cache.end()) {
				cache[a_newID] = it->second;
			}
		}
	}
}
