#include "PCLevelMultManager.h"

namespace PCLevelMult
{
	Input::Input(const RE::Actor* a_character, const RE::TESNPC* a_base, bool a_onlyPlayerLevelEntries) :
		playerID(Manager::GetSingleton()->GetCurrentPlayerID()),
		npcLevel(a_base->GetLevel()),
		npcLevelCap(a_base->actorData.calcLevelMax),
		onlyPlayerLevelEntries(a_onlyPlayerLevelEntries)
	{
		npcFormID = a_base->IsDynamicForm() ? a_character->GetFormID() : a_base->GetFormID();  // use character formID for permanent storage
	}

	void Manager::Register()
	{
		if (const auto UI = RE::UI::GetSingleton()) {
			UI->AddEventSink(GetSingleton());
			logger::info("Registered {}", typeid(Manager).name());
		}
	}

	RE::BSEventNotifyControl Manager::ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
	{
		if (a_event && a_event->menuName == RE::RaceSexMenu::MENU_NAME) {
			if (a_event->opening) {
				oldPlayerID = get_game_playerID();
			} else {
				const auto newPlayerID = get_game_playerID();
				if (newGameStarted) {
					newGameStarted = false;
					currentPlayerID = newPlayerID;
				} else if (oldPlayerID != newPlayerID) {
					remap_player_ids(oldPlayerID, newPlayerID);
				}
			}
		}
		return RE::BSEventNotifyControl::kContinue;
	}

	bool Manager::FindRejectedEntry(const Input& a_input, RE::FormID a_distributedFormID, std::uint32_t a_formDataIndex) const
	{
		ReadLocker lock(_lock);
		if (const auto idIt = _cache.find(a_input.playerID); idIt != _cache.end()) {
			auto& npcFormIDMap = idIt->second;
			if (const auto npcIt = npcFormIDMap.find(a_input.npcFormID); npcIt != npcFormIDMap.end()) {
				for (const auto& [level, cachedData] : npcIt->second.entries) {
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
		WriteLocker lock(_lock);
		return _cache[a_input.playerID]
		             [a_input.npcFormID]
		                 .entries[a_input.npcLevel]
		                 .rejectedEntries[a_distributedFormID]
		                 .insert(a_formDataIndex)
		                 .second;
	}

	void Manager::DumpRejectedEntries()
	{
		ReadLocker lock(_lock);
		for (auto& [playerID, npcFormIDs] : _cache) {
			logger::info("PlayerID : {:X}", playerID);
			for (auto& [npcFormID, levelMap] : npcFormIDs) {
				logger::info("\tNPC : {} [{:X}]", editorID::get_editorID(RE::TESForm::LookupByID(npcFormID)), npcFormID);
				for (auto& [level, distFormMap] : levelMap.entries) {
					logger::info("\t\tLevel : {}", level);
					for (auto& [distFormID, idxSet] : distFormMap.rejectedEntries) {
						logger::info("\t\t\tDist FormID : {} [{:X}]", editorID::get_editorID(RE::TESForm::LookupByID(distFormID)), distFormID);
						for (auto& idx : idxSet) {
							logger::info("\t\t\t\tIDX : {}", idx);
						}
					}
				}
			}
		}
	}

	bool Manager::FindDistributedEntry(const Input& a_input)
	{
		ReadLocker lock(_lock);
		if (const auto it = _cache.find(a_input.playerID); it != _cache.end()) {
			if (const auto npcIt = it->second.find(a_input.npcFormID); npcIt != it->second.end()) {
				return !npcIt->second.entries.empty();
			}
		}
		return false;
	}

	void Manager::InsertDistributedEntry(const Input& a_input, RE::FormType a_formType, const Set<RE::FormID>& a_formIDSet)
	{
		WriteLocker lock(_lock);
		_cache[a_input.playerID][a_input.npcFormID].entries[a_input.npcLevel].distributedEntries[a_formType].insert(a_formIDSet.begin(), a_formIDSet.end());
	}

	void Manager::DumpDistributedEntries()
	{
		ReadLocker lock(_lock);
		for (const auto& [playerID, npcFormIDs] : _cache) {
			logger::info("PlayerID : {:X}", playerID);
			for (const auto& [npcFormID, levelMap] : npcFormIDs) {
				logger::info("\tNPC : {} [{:X}]", editorID::get_editorID(RE::TESForm::LookupByID(npcFormID)), npcFormID);
				for (const auto& [level, distFormMap] : levelMap.entries) {
					logger::info("\t\tLevel : {}", level);
					for (const auto& [formType, formIDSet] : distFormMap.distributedEntries) {
						logger::info("\t\t\tDist FormType : {}", formType);
						for (const auto& formID : formIDSet) {
							logger::info("\t\t\t\tDist FormID : {} [{:X}]", editorID::get_editorID(RE::TESForm::LookupByID(formID)), formID);
						}
					}
				}
			}
		}
	}

	void Manager::ForEachDistributedEntry(const Input& a_input, bool a_onlyValidEntries, std::function<void(RE::FormType, const Set<RE::FormID>&)> a_fn) const
	{
		ReadLocker lock(_lock);
		if (const auto pcIt = _cache.find(a_input.playerID); pcIt != _cache.end()) {
			auto& npcFormIDMap = pcIt->second;
			if (const auto npcIt = npcFormIDMap.find(a_input.npcFormID); npcIt != npcFormIDMap.end()) {
				for (const auto& [level, cachedData] : npcIt->second.entries) {
					if (a_onlyValidEntries && a_input.npcLevel < level) {
						continue;
					}
					for (auto& [formType, entries] : cachedData.distributedEntries) {
						a_fn(formType, entries);
					}
				}
			}
		}
	}

	// For spawned actors with FF reference IDs
	void Manager::DeleteNPC(RE::FormID a_characterID)
	{
		WriteLocker lock(_lock);
		auto&       currentCache = _cache[GetSingleton()->GetCurrentPlayerID()];
		if (const auto it = currentCache.find(a_characterID); it != currentCache.end()) {
			currentCache.erase(it);
		}
	}

	bool Manager::HasHitLevelCap(const Input& a_input)
	{
		bool hitCap = (a_input.npcLevel == a_input.npcLevelCap);

		WriteLocker lock(_lock);
		auto&       map = _cache[a_input.playerID];
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

	std::uint64_t Manager::GetOldPlayerID() const
	{
		return oldPlayerID;
	}

	void Manager::GetPlayerIDFromSave(const std::string& a_saveName)
	{
		// Quicksave0_2A73F01A_0_6E656C736F6E_Tamriel_000002_20220918174138_10_1.ess
		// 2A73F01A is player ID

		oldPlayerID = currentPlayerID;
		if (const auto save = string::split(a_saveName, "_"); save.size() == 9) {
			currentPlayerID = string::to_num<std::uint64_t>(save[1], true);
		} else {
			currentPlayerID = 0;  // non standard save name, use game playerID instead
		}
	}

	void Manager::SetNewGameStarted()
	{
		newGameStarted = true;
	}

	std::uint64_t Manager::get_game_playerID()
	{
		return RE::BGSSaveLoadManager::GetSingleton()->currentCharacterID & 0xFFFFFFFF;
	}

	void Manager::remap_player_ids(std::uint64_t a_oldID, std::uint64_t a_newID)
	{
		WriteLocker lock(_lock);
		if (!_cache.contains(a_newID)) {
			if (const auto it = _cache.find(a_oldID); it != _cache.end()) {
				_cache[a_newID] = it->second;
			}
		}
	}
}
