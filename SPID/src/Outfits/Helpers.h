#pragma once

namespace Outfits
{
	/// This re-creates game's function that performs a similar code, but crashes for unknown reasons :)
	/// <param name="shouldInitOutfit">Whether this call should auto-init outfit items if needed. If you manually control outfits items init, pass false.</param>
	inline void AddWornOutfit(RE::Actor* actor, RE::BGSOutfit* outfit, bool shouldUpdate3D, bool shouldInitOutfit = true)
	{
		bool equipped = false;
		if (const auto invChanges = actor->GetInventoryChanges()) {
			if (shouldInitOutfit && !actor->HasOutfitItems(outfit)) {
				invChanges->InitOutfitItems(outfit, actor->GetLevel());
			}
			if (const auto entryList = invChanges->entryList) {
				const auto formID = outfit->GetFormID();
				for (const auto& entryData : *entryList) {
					if (entryData && entryData->object && entryData->extraLists) {
						for (const auto& extraList : *entryData->extraLists) {
							auto outfitItem = extraList ? extraList->GetByType<RE::ExtraOutfitItem>() : nullptr;
							if (outfitItem && outfitItem->id == formID) {
								// forceEquip - actually it corresponds to the "PreventRemoval" flag in the game's function,
								//				which determines whether NPC/EquipItem call can unequip the item. See EquipItem Papyrus function.
								// What it does in practice is blocks that equip slot from being re-evaluated when UpdateWornGear is called.
								RE::ActorEquipManager::GetSingleton()->EquipObject(actor, entryData->object, extraList, 1, nullptr, shouldUpdate3D, false, false, true);
								equipped = true;
							}
						}
					}
				}
			}
		}

		if (shouldUpdate3D && equipped) {
			if (const auto cell = actor->GetParentCell(); cell && cell->cellState.underlying() == 4) {
				actor->Update3DModel();
			}
		}
	}

	/// Groups all items in actor's inventory by the outfits associated with them.
	inline std::map<RE::BGSOutfit*, std::vector<std::pair<RE::TESForm*, bool>>> GetAllOutfitItems(RE::Actor* actor)
	{
		std::map<RE::BGSOutfit*, std::vector<std::pair<RE::TESForm*, bool>>> items;
		if (const auto invChanges = actor->GetInventoryChanges()) {
			if (const auto entryList = invChanges->entryList) {
				for (const auto& entryData : *entryList) {
					if (entryData && entryData->object && entryData->extraLists) {
						for (const auto& extraList : *entryData->extraLists) {
							auto outfitItem = extraList ? extraList->GetByType<RE::ExtraOutfitItem>() : nullptr;
							if (outfitItem) {
								auto isWorn = extraList ? extraList->GetByType<RE::ExtraWorn>() : nullptr;
								auto item = entryData->object;

								if (auto outfit = RE::TESForm::LookupByID<RE::BGSOutfit>(outfitItem->id); outfit) {
									items[outfit].emplace_back(item, isWorn != nullptr);
								}
							}
						}
					}
				}
			}
		}

		return items;
	}

	template <typename T>
	std::string JoinVector(const std::vector<T>& vec)
	{
		std::ostringstream oss;
		for (size_t i = 0; i < vec.size(); ++i) {
			oss << std::hex << std::uppercase << vec[i];
			if (i + 1 < vec.size()) {
				oss << ", ";
			}
		}
		return oss.str();
	}

	inline void LogInventory(RE::Actor* actor, const char* prefix = "\t")
	{
		if (const auto invChanges = actor->GetInventoryChanges()) {
			if (const auto entryList = invChanges->entryList) {
				for (const auto& entryData : *entryList) {
					if (entryData && entryData->object) {
						bool             isWorn = false;
						std::vector<int> extraTypes{};
						RE::BGSOutfit*   outfit = nullptr;
						if (const auto extraLists = entryData->extraLists) {
							for (const auto& extraList : *entryData->extraLists) {
								if (!extraList) {
									continue;
								}

								for (int i = 0; i < static_cast<int>(RE::ExtraDataType::kUnkBF); i++) {
									if (auto extraData = extraList->GetByType(static_cast<RE::ExtraDataType>(i)); extraData) {
										extraTypes.push_back(i);
									}
								}
								if (std::find(extraTypes.begin(), extraTypes.end(), 0x16) != extraTypes.end()) {
									isWorn = true;
								}

								if (auto outfitItem = extraList->GetByType<RE::ExtraOutfitItem>(); outfitItem) {
									if (auto o = RE::TESForm::LookupByID<RE::BGSOutfit>(outfitItem->id); o) {
										outfit = o;
									}
								}
							}
						}
						if (outfit) {
							logger::info("{}{} [{:+}]{} (Part of {}) (extras: {})", prefix, *entryData->object, entryData->countDelta, isWorn ? " (Worn)" : "", *outfit, JoinVector(extraTypes));
						} else {
							logger::info("{}{} [{:+}]{} (extras: {})", prefix, *entryData->object, entryData->countDelta, isWorn ? " (Worn)" : "", JoinVector(extraTypes));
						}
					}
				}
			}
		}
	}

	inline bool IsWearingOutfit(RE::Actor* actor, RE::BGSOutfit* targetOutfit)
	{
		std::set<RE::FormID> targetOutfitItemIDs{};
		for (const auto& obj : targetOutfit->outfitItems) {
			targetOutfitItemIDs.insert(obj->formID);
		}

		std::set<RE::FormID> wornOutfitItemIDs{};
		const auto           itemsMap = GetAllOutfitItems(actor);
		if (const auto target = itemsMap.find(targetOutfit); target != itemsMap.end()) {
			for (const auto& [item, isWorn] : target->second) {
				if (isWorn) {
					wornOutfitItemIDs.insert(item->formID);
				}
			}
		}

		return targetOutfitItemIDs == wornOutfitItemIDs;
	}

	inline void LogWornOutfitItems(RE::Actor* actor, const char* prefix = "\t\t")
	{
		auto items = GetAllOutfitItems(actor);

		for (const auto& [outfit, itemVec] : items) {
			logger::info("{}{}", prefix, *outfit);
			const auto lastItemIndex = itemVec.size() - 1;
			for (int i = 0; i < lastItemIndex; ++i) {
				const auto& item = itemVec[i];
				logger::info("{}├─── {}{}", prefix, *item.first, item.second ? " (Worn)" : "");
			}
			const auto& lastItem = itemVec[lastItemIndex];
			logger::info("{}└─── {}{}", prefix, *lastItem.first, lastItem.second ? " (Worn)" : "");
		}
	}
}
