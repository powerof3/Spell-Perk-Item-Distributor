#include "OutfitManager.h"
#include "Hooking.h"
#include "LookupNPC.h"
#include "DistributeManager.h"

namespace Outfits
{
#pragma region Serialization

	constexpr std::uint32_t serializationKey = 'SPID';
	constexpr std::uint32_t serializationVersion = 3;

	constexpr std::uint32_t recordType = 'OTFT';

	namespace details
	{
		template <typename T>
		bool Write(SKSE::SerializationInterface* interface, const T& data)
		{
			return interface->WriteRecordData(&data, sizeof(T));
		}

		template <>
		bool Write(SKSE::SerializationInterface* interface, const std::string& data)
		{
			const std::size_t size = data.length();
			return interface->WriteRecordData(size) && interface->WriteRecordData(data.data(), static_cast<std::uint32_t>(size));
		}

		template <typename T>
		bool Read(SKSE::SerializationInterface* a_interface, T& result)
		{
			return a_interface->ReadRecordData(&result, sizeof(T));
		}

		template <>
		bool Read(SKSE::SerializationInterface* interface, std::string& result)
		{
			std::size_t size = 0;
			if (!interface->ReadRecordData(size)) {
				return false;
			}
			if (size > 0) {
				result.resize(size);
				if (!interface->ReadRecordData(result.data(), static_cast<std::uint32_t>(size))) {
					return false;
				}
			} else {
				result = "";
			}
			return true;
		}

		bool Load(SKSE::SerializationInterface* interface, RE::FormID& formID)
		{
			RE::FormID tempID = 0;
			formID = 0;

			if (!details::Read(interface, tempID)) {
				return false;
			}

			if (!tempID) {
				return false;
			}

			formID = tempID;  // save the originally read formID

			if (!interface->ResolveFormID(tempID, tempID)) {
				return false;
			}

			formID = tempID;  // save the resolved formID
			return true;
		}

		template <typename T>
		bool Load(SKSE::SerializationInterface* interface, T*& output, RE::FormID& formID)
		{
			output = nullptr;

			if (!Load(interface, formID)) {
				return false;
			}

			if (const auto form = RE::TESForm::LookupByID<T>(formID); form) {
				output = form;
				return true;
			}

			return false;
		}

	}

	bool Manager::LoadReplacementV1(SKSE::SerializationInterface* interface, RE::FormID& loadedActorFormID, Manager::OutfitReplacement& loadedReplacement)
	{
		RE::FormID     id = 0;
		RE::FormID     failedDistributedOutfitFormID = 0;
		RE::BGSOutfit* distributed;

		if (!details::Load(interface, loadedActorFormID)) {
			//#ifndef NDEBUG
			logger::warn("Failed to load Outfit Replacement record: Unknown actor [{:08X}].", loadedActorFormID);
			//#endif
			return false;
		}

		RE::BGSOutfit* original;
		if (!details::Load(interface, original, id)) {
			//#ifndef NDEBUG
			logger::warn("Failed to load Outfit Replacement record: Unknown original outfit [{:08X}].", id);
			//#endif
			return false;
		}

		if (!details::Load(interface, distributed, id)) {
			//#ifndef NDEBUG
			logger::warn("Failed to load Outfit Replacement record: Unknown distributed outfit [{:08X}].", id);
			//#endif
			if (!id) {
				return false;
			}
			failedDistributedOutfitFormID = id;
		}

		bool isDeathOutfit = false;
		// For earlier version we assume that all distributed outfits on currently dead actors are death outfits.
		if (auto loadedActor = RE::TESForm::LookupByID<RE::Actor>(loadedActorFormID); loadedActor) {
			auto npc = loadedActor->GetActorBase();
			if (npc) {
				// Revert NPC's outfit back to original since we no longer need to track original defaultOutfit since we won't change it.
				// Note this also handles the case when distributed outfit got corrupted (which will make defaultOutfit to be NULL).
				if (!distributed || npc->defaultOutfit == distributed) {
					npc->defaultOutfit = original;
				}
			}
			isDeathOutfit = NPCData::IsDead(loadedActor);
		}

		if (distributed) {
			loadedReplacement = OutfitReplacement{ distributed, isDeathOutfit, false };
		} else {
			loadedReplacement = OutfitReplacement{ failedDistributedOutfitFormID };
		}

		return true;
	}

	bool Manager::LoadReplacementV2(SKSE::SerializationInterface* interface, RE::FormID& loadedActorFormID, Manager::OutfitReplacement& loadedReplacement)
	{
		bool isDeathOutfit = false;
		bool isFinalOutfit = false;
		bool isSuspended = false;

		RE::FormID     id = 0;
		RE::FormID     failedDistributedOutfitFormID = 0;
		RE::BGSOutfit* distributed;

		if (!details::Load(interface, loadedActorFormID)) {
			//#ifndef NDEBUG
			logger::warn("Failed to load Outfit Replacement record: Unknown actor [{:08X}].", loadedActorFormID);
			//#endif
			return false;
		}

		if (!details::Load(interface, distributed, id)) {
			//#ifndef NDEBUG
			logger::warn("Failed to load Outfit Replacement record: Unknown distributed outfit [{:08X}].", id);
			//#endif
			if (!id) {
				return false;
			}
			failedDistributedOutfitFormID = id;
		}

		if (!details::Read(interface, isDeathOutfit) ||
			!details::Read(interface, isFinalOutfit) ||
			!details::Read(interface, isSuspended)) {
			//#ifndef NDEBUG
			if (auto loadedActor = RE::TESForm::LookupByID<RE::Actor>(loadedActorFormID); loadedActor) {
				logger::warn("Failed to load attributes for Outfit Replacement record. Actor: {}", *loadedActor);
			}
			//#endif
		}

		if (distributed) {
			loadedReplacement = OutfitReplacement{ distributed, isDeathOutfit, isFinalOutfit };
		} else {
			loadedReplacement = OutfitReplacement{ failedDistributedOutfitFormID };
		}
		return true;
	}

	bool Manager::LoadReplacementV3(SKSE::SerializationInterface* interface, RE::FormID& loadedActorFormID, Manager::OutfitReplacement& loadedReplacement)
	{
		bool isDeathOutfit = false;
		bool isFinalOutfit = false;

		RE::FormID     id = 0;
		RE::FormID     failedDistributedOutfitFormID = 0;
		RE::BGSOutfit* distributed;

		if (!details::Load(interface, loadedActorFormID)) {
			//#ifndef NDEBUG
			logger::warn("Failed to load Outfit Replacement record: Unknown actor [{:08X}].", loadedActorFormID);
			//#endif
			return false;
		}

		if (!details::Load(interface, distributed, id)) {
			//#ifndef NDEBUG
			logger::warn("Failed to load Outfit Replacement record: Unknown distributed outfit [{:08X}].", id);
			//#endif
			if (!id) {
				return false;
			}
			failedDistributedOutfitFormID = id;
		}

		if (!details::Read(interface, isDeathOutfit) ||
			!details::Read(interface, isFinalOutfit)) {
			//#ifndef NDEBUG
			if (auto loadedActor = RE::TESForm::LookupByID<RE::Actor>(loadedActorFormID); loadedActor) {
				logger::warn("Failed to load attributes for Outfit Replacement record. Actor: {}", *loadedActor);
			}
			//#endif
		}

		if (distributed) {
			loadedReplacement = OutfitReplacement{ distributed, isDeathOutfit, isFinalOutfit };
		} else {
			loadedReplacement = OutfitReplacement{ failedDistributedOutfitFormID };
		}
		return true;
	}

	bool Manager::SaveReplacement(SKSE::SerializationInterface* interface, const RE::FormID& actorFormID, const OutfitReplacement& replacement)
	{
		if (!interface->OpenRecord(recordType, serializationVersion)) {
			return false;
		}

		return details::Write(interface, actorFormID) &&
		       details::Write(interface, replacement.distributed ? replacement.distributed->formID : 0) &&
		       details::Write(interface, replacement.isDeathOutfit) &&
		       details::Write(interface, replacement.isFinalOutfit);
	}

	void Manager::Load(SKSE::SerializationInterface* interface)
	{
		//#ifndef NDEBUG
		LOG_HEADER("LOADING");
		std::unordered_map<RE::Actor*, OutfitReplacement> loadedReplacements;
		//#endif

		auto manager = Manager::GetSingleton();

		WriteLocker wornLock(manager->_wornLock);
		WriteLocker pendingLock(manager->_pendingLock);

		std::uint32_t type, version, length;
		int           total = 0;

		while (interface->GetNextRecordInfo(type, version, length)) {
			if (type == recordType) {
				RE::FormID        actorFormID;
				OutfitReplacement loadedReplacement;
				total++;
				bool loaded = false;
				switch (version) {
				case 1:
					loaded = LoadReplacementV1(interface, actorFormID, loadedReplacement);
					break;
				case 2:
					loaded = LoadReplacementV2(interface, actorFormID, loadedReplacement);
					break;
				default:
					loaded = LoadReplacementV3(interface, actorFormID, loadedReplacement);
					break;
				}
				if (loaded) {
					// Only carry over replacements for actors that still exist.
					if (const auto actor = RE::TESForm::LookupByID<RE::Actor>(actorFormID); actor) {
						if (loadedReplacement.distributed) {
							manager->wornReplacements[actorFormID] = loadedReplacement;
							//#ifndef NDEBUG
							loadedReplacements[actor] = loadedReplacement;
							//#endif
						} else {
							manager->RevertOutfit(actor, loadedReplacement);
						}
					}
				}
			}
		}

		auto& pendingReplacements = manager->pendingReplacements;

		//#ifndef NDEBUG
		logger::info("Loaded {}/{} Outfit Replacements", loadedReplacements.size(), total);
		for (const auto& pair : loadedReplacements) {
			logger::info("\t{}", *pair.first);
			logger::info("\t\t{}", pair.second);
		}

		logger::info("Pending {} Outfit Replacements", pendingReplacements.size());
		for (const auto& pair : pendingReplacements) {
			if (const auto actor = RE::TESForm::LookupByID<RE::Actor>(pair.first); actor) {
				logger::info("\t{}", *actor);
			}
			logger::info("\t\t{}", pair.second);
		}

		logger::info("Applying resolved outfits...");
		//#endif

		// We don't increment iterator here, since ResolveWornOutfit will be erasing each pending entry
		for (auto it = pendingReplacements.begin(); it != pendingReplacements.end();) {
			if (auto actor = RE::TESForm::LookupByID<RE::Actor>(it->first); actor) {
				if (auto resolved = manager->ResolveWornOutfit(actor, it, false); resolved) {
					//#ifndef NDEBUG
					logger::info("\tActor: {}", *actor);
					logger::info("\t\tResolved: {}", *resolved);
					logger::info("\t\tDefault: {}", *(actor->GetActorBase()->defaultOutfit));
					//#endif
					manager->ApplyOutfit(actor, resolved->distributed);
				}
			}
		}
	}

	void Manager::Save(SKSE::SerializationInterface* interface)
	{
		//#ifndef NDEBUG
		LOG_HEADER("SAVING");
		//#endif
		auto       manager = Manager::GetSingleton();
		const auto replacements = manager->GetWornOutfits();
		//#ifndef NDEBUG
		logger::info("Saving {} distributed outfits...", replacements.size());
		std::uint32_t savedCount = 0;
		//#endif

		for (const auto& pair : replacements) {
			if (!SaveReplacement(interface, pair.first, pair.second)) {
				//#ifndef NDEBUG
				if (const auto actor = RE::TESForm::LookupByID<RE::Actor>(pair.first); actor) {
					logger::error("Failed to save Outfit Replacement ({}) for {}", pair.second, *actor);
				}
				//#endif
				continue;
			}
			//#ifndef NDEBUG
			if (const auto actor = RE::TESForm::LookupByID<RE::Actor>(pair.first); actor) {
				logger::info("\tSaved Outfit Replacement ({}) for actor {}", pair.second, *actor);
			}
			++savedCount;
			//#endif
		}

		//#ifndef NDEBUG
		logger::info("Saved {} replacements", savedCount);
		//#endif
	}
#pragma endregion

#pragma region Helpers
	/// This re-creates game's function that performs a similar code, but crashes for unknown reasons :)
	void AddWornOutfit(RE::Actor* actor, RE::BGSOutfit* outfit, bool shouldUpdate3D)
	{
		bool equipped = false;
		if (const auto invChanges = actor->GetInventoryChanges()) {
			if (!actor->HasOutfitItems(outfit)) {
				invChanges->InitOutfitItems(outfit, actor->GetLevel());
			}
			if (const auto entryLists = invChanges->entryList) {
				const auto formID = outfit->GetFormID();
				for (const auto& entryList : *entryLists) {
					if (entryList && entryList->object && entryList->extraLists) {
						for (const auto& xList : *entryList->extraLists) {
							auto outfitItem = xList ? xList->GetByType<RE::ExtraOutfitItem>() : nullptr;
							if (outfitItem && outfitItem->id == formID) {
								// forceEquip - actually it corresponds to the "PreventRemoval" flag in the game's function,
								//				which determines whether NPC/EquipItem call can unequip the item. See EquipItem Papyrus function.
								RE::ActorEquipManager::GetSingleton()->EquipObject(actor, entryList->object, xList, 1, nullptr, shouldUpdate3D, true, false, false);
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
	std::map<RE::BGSOutfit*, std::vector<std::pair<RE::TESForm*, bool>>> GetAllOutfitItems(RE::Actor* actor)
	{
		std::map<RE::BGSOutfit*, std::vector<std::pair<RE::TESForm*, bool>>> items;
		if (const auto invChanges = actor->GetInventoryChanges()) {
			if (const auto entryLists = invChanges->entryList) {
				for (const auto& entryList : *entryLists) {
					if (entryList && entryList->object && entryList->extraLists) {
						for (const auto& xList : *entryList->extraLists) {
							auto outfitItem = xList ? xList->GetByType<RE::ExtraOutfitItem>() : nullptr;
							if (outfitItem) {
								auto isWorn = xList ? xList->GetByType<RE::ExtraWorn>() : nullptr;
								auto item = entryList->object;
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

	void LogWornOutfitItems(RE::Actor* actor)
	{
		auto items = GetAllOutfitItems(actor);

		for (const auto& [outfit, itemVec] : items) {
			logger::info("\t{}", *outfit);
			const auto lastItemIndex = itemVec.size() - 1;
			for (int i = 0; i < lastItemIndex; ++i) {
				const auto& item = itemVec[i];
				logger::info("\t├─── {}{}", *item.first, item.second ? " (Worn)" : "");
			}
			const auto& lastItem = itemVec[lastItemIndex];
			logger::info("\t└─── {}{}", *lastItem.first, lastItem.second ? " (Worn)" : "");
		}
	}

#pragma endregion

#pragma region Hooks
	struct ShouldBackgroundClone
	{
		using Target = RE::Character;
		static inline constexpr std::size_t index{ 0x6D };

		static bool thunk(RE::Character* actor)
		{
#ifndef NDEBUG
			//	logger::info("Outfits: ShouldBackgroundClone({})", *(actor->As<RE::Actor>()));
#endif
			return Manager::GetSingleton()->ProcessShouldBackgroundClone(actor, [&] { return func(actor); });
		}

		static inline void post_hook()
		{
			logger::info("\t\t🪝Installed ShouldBackgroundClone hook.");
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	/// This hook applies pending outfit replacements before loading 3D model. Outfit Replacements are created by SetDefaultOutfit.
	struct Load3D
	{
		using Target = RE::Character;
		static inline constexpr std::size_t index{ 0x6A };

		static RE::NiAVObject* thunk(RE::Character* actor, bool a_backgroundLoading)
		{
			//logger::info("Load3D ({}); Background: {}", *(actor->As<RE::Actor>()), a_backgroundLoading);
			return Manager::GetSingleton()->ProcessLoad3D(actor, [&] { return func(actor, a_backgroundLoading); });
		}

		static inline void post_hook()
		{
			logger::info("\t\t🪝Installed Load3D hook.");
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	/// This hook builds a map of initialOutfits for all NPCs that can have it.
	/// The map is later used to determine when manual calls to SetOutfit should suspend/resume SPID-managed outfits.
	struct InitItemImpl
	{
		using Target = RE::TESNPC;
		static inline constexpr std::size_t index{ 0x13 };

		static void thunk(RE::TESNPC* npc)
		{
			Manager::GetSingleton()->ProcessInitItemImpl(npc, [&] { func(npc); });
		}

		static inline void post_hook()
		{
			logger::info("\t\t🪝Installed InitItemImpl hook.");
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	/// This hook is used to track when NPCs come back to life to clear the isDeathOutfit flag and restore general outfit behavior,
	/// as alive NPCs are allowed to change their outfits (even if looted).
	///
	/// Note: recycleactor console command also triggers resurrect when needed.
	/// Note: NPCs who Start Dead cannot be resurrected.
	struct Resurrect
	{
		using Target = RE::Character;
		static inline constexpr std::size_t index{ 0xAB };

		static void thunk(RE::Character* actor, bool resetInventory, bool attach3D)
		{
			//#ifndef NDEBUG
			logger::info("Resurrect({}); IsDead: {}, ResetInventory: {}, Attach3D: {}", *(actor->As<RE::Actor>()), actor->IsDead(), resetInventory, attach3D);
			//#endif
			return Manager::GetSingleton()->ProcessResurrect(actor, [&] { return func(actor, resetInventory, attach3D); });
		}

		static inline void post_hook()
		{
			logger::info("\t\t🪝Installed Resurrect hook.");
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	/// This hook is used to clear outfit replacements info since the actor will be completely reset,
	/// thus eligible for a fresh distribution.
	struct ResetReference
	{
		static inline constexpr REL::ID     relocation = RELOCATION_ID(21556, 22038);
		static inline constexpr std::size_t offset = OFFSET(0x4B, 0x4B);

		static bool thunk(std::int64_t a1, std::int64_t a2, RE::TESObjectREFR* refr, std::int64_t a4, std::int64_t a5, std::int64_t a6, int a7, int* a8)
		{
			if (refr) {
				if (const auto actor = refr->As<RE::Actor>(); actor) {
					//#ifndef NDEBUG
					logger::info("RecycleActor({})", *(actor->As<RE::Actor>()));
					//#endif
					return Manager::GetSingleton()->ProcessResetReference(actor, [&] { return func(a1, a2, refr, a4, a5, a6, a7, a8); });
				}
			}
			return func(a1, a2, refr, a4, a5, a6, a7, a8);
		}

		static inline void post_hook()
		{
			logger::info("\t\t🪝Installed ResetReference hook.");
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	/// This hook is used to detect manual changes of Actor's outfit and suspend SPID-managed outfits.
	/// Suspending allows SPID-managed outfits to behave like default outfits.
	struct SetOutfitActor
	{
		static inline constexpr REL::ID     relocation = RELOCATION_ID(53960, 54784);
		static inline constexpr std::size_t offset = OFFSET(0x3E60, 0x47D5);

		// This re-creates original papyrus::Actor::SetOutfit function.
		static void thunk(RE::BSScript::Internal::VirtualMachine* vm, RE::VMStackID stackID, RE::Actor* actor, RE::BGSOutfit* outfit, bool isSleepOutfit)
		{
			if (!outfit) {
				using PapyrusLog = void(RE::TESObjectREFR*, const char*, RE::BSScript::Internal::VirtualMachine*, RE::VMStackID, RE::BSScript::ErrorLogger::Severity);
				REL::Relocation<PapyrusLog> writeLog{ RELOCATION_ID(53024, 53824) };
				writeLog(actor, "Cannot set sleep or default outfit to None", vm, stackID, RE::BSScript::ErrorLogger::Severity::kError);
				return;
			}

			auto npc = actor->GetActorBase();

			// Sleep outfits retain original logic... for now :)
			if (isSleepOutfit) {
				if (npc->sleepOutfit == outfit) {
					return;
				}
				actor->RemoveOutfitItems(npc->sleepOutfit);
				npc->SetSleepOutfit(outfit);

				actor->InitInventoryIfRequired();
				if (!actor->IsDisabled()) {
					actor->AddWornOutfit(outfit, true);
				}
			} else {
				Manager::GetSingleton()->ProcessSetOutfitActor(actor, outfit, [&] { func(vm, stackID, actor, outfit, isSleepOutfit); });
			}
		}

		static inline void post_hook()
		{
			logger::info("\t\t🪝Installed SetOutfit hook.");
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct EquipObject
	{
		static inline constexpr REL::ID     relocation = RELOCATION_ID(37938, 38894);
		static inline constexpr std::size_t offset = OFFSET(0xE5, 0x170);

		static void thunk(RE::ActorEquipManager* manager, RE::Actor* actor, RE::TESBoundObject* object, RE::ExtraDataList* list)
		{
			if (actor && object) {
				logger::info("[EQUIP] {} equips {}", *actor, *object);
			}
			func(manager, actor, object, list);
		}

		static inline void post_hook()
		{
			logger::info("\t\t🪝Installed EquipObject hook.");
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct UnequipObject
	{
		static inline constexpr REL::ID     relocation = RELOCATION_ID(37945, 38901);
		static inline constexpr std::size_t offset = OFFSET(0x138, 0x1B9);

		static void thunk(RE::ActorEquipManager* manager, RE::Actor* actor, RE::TESBoundObject* object, RE::ExtraDataList* list)
		{
			if (actor && object) {
				logger::info("[UNEQUIP] {} unequips {}", *actor, *object);
			}
			func(manager, actor, object, list);
		}

		static inline void post_hook()
		{
			logger::info("\t\t🪝Installed UnequipObject hook.");
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};
#pragma endregion

	void Manager::HandleMessage(SKSE::MessagingInterface::Message* message)
	{
		switch (message->type) {
		case SKSE::MessagingInterface::kPostLoad:
			{
				logger::info("🧥Outfit Manager");

				const auto serializationInterface = SKSE::GetSerializationInterface();
				serializationInterface->SetUniqueID(serializationKey);
				serializationInterface->SetSaveCallback(Save);
				serializationInterface->SetLoadCallback(Load);

				if (const auto scripts = RE::ScriptEventSourceHolder::GetSingleton()) {
					scripts->AddEventSink<RE::TESFormDeleteEvent>(this);
					logger::info("\t\t📝Registered for {}.", typeid(RE::TESFormDeleteEvent).name());
					scripts->AddEventSink<RE::TESDeathEvent>(this);
					logger::info("\t\t📝Registered for {}.", typeid(RE::TESDeathEvent).name());
					//#ifndef NDEBUG
					scripts->AddEventSink<RE::TESContainerChangedEvent>(this);
					logger::info("\t\t📝Registered for {}.", typeid(RE::TESContainerChangedEvent).name());
					//#endif
				}

				stl::install_hook<InitItemImpl>();
				stl::install_hook<ShouldBackgroundClone>();
				stl::install_hook<Load3D>();
				stl::install_hook<Resurrect>();
				stl::install_hook<ResetReference>();
				stl::install_hook<SetOutfitActor>();
				//#ifndef NDEBUG
				stl::install_hook<EquipObject>();
				stl::install_hook<UnequipObject>();
				//#endif
			}
			break;
#ifndef NDEBUG
		case SKSE::MessagingInterface::kPostPostLoad:
			LOG_HEADER("OUTFITS");  // This is where TESNPCs start initializing, so we give it a nice header.
			break;
#endif
		case SKSE::MessagingInterface::kPreLoadGame:
			isLoadingGame = true;
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
			isLoadingGame = false;
			break;
		default:
			break;
		}
	}

#pragma region Outfit Management
	bool Manager::HasDefaultOutfit(const RE::TESNPC* npc, const RE::BGSOutfit* outfit) const
	{
		if (!outfit) {
			return false;
		}

		if (auto existing = initialOutfits.find(npc->formID); existing != initialOutfits.end()) {
			return existing->second == outfit;
		}

		return npc->defaultOutfit == outfit;
	}

	bool Manager::CanEquipOutfit(const RE::Actor* actor, const RE::BGSOutfit* outfit) const
	{
		// Actors that don't have default outfit can't wear any outfit.
		if (!actor->GetActorBase()->defaultOutfit) {
			return false;
		}

		const auto race = actor->GetRace();
		for (const auto& item : outfit->outfitItems) {
			if (const auto armor = item->As<RE::TESObjectARMO>()) {
				if (!std::any_of(armor->armorAddons.begin(), armor->armorAddons.end(), [&](const auto& arma) {
						return arma && arma->IsValidRace(race);
					})) {
					return false;
				}
			}
		}

		return true;
	}

	bool Manager::RevertOutfit(RE::Actor* actor)
	{
		if (!actor) {
			return false;
		}
		if (const auto replacement = PopWornOutfit(actor); replacement) {
			return RevertOutfit(actor, *replacement);
		}
		return false;
	}

	std::optional<Manager::OutfitReplacement> Manager::ResolveWornOutfit(RE::Actor* actor, bool isDying)
	{
		WriteLocker pendingLock(_pendingLock);
		WriteLocker wornLock(_wornLock);
		if (auto pending = pendingReplacements.find(actor->formID); pending != pendingReplacements.end()) {
			return ResolveWornOutfit(actor, pending, isDying);
		}

		return std::nullopt;
	}

	// TODO: Do not lock this whole function OR move out RevertOutfit() call.
	std::optional<Manager::OutfitReplacement> Manager::ResolveWornOutfit(RE::Actor* actor, OutfitReplacementMap::iterator& pending, bool isDying)
	{
		// W and G are named according to the Resolution Table.
		const bool isDead = NPCData::IsDead(actor);
		const auto G = pending->second;
		pendingReplacements.erase(pending++);  // we also increment iterator so that it will remain valid after the erase.

		assert(!isDying || G.isDeathOutfit);            // If actor is dying then outfit must be from On Death Distribution, otherwise there is a mistake in the code.
		assert(!G.isDeathOutfit || isDying || isDead);  // If outfit is death outfit then actor must be dying or dead, otherwise there is a mistake in the code.

		if (G.distributed) {  // If there is a Distributed outfit, then we need to apply it
			if (const auto existing = wornReplacements.find(actor->formID); existing != wornReplacements.end()) {
				auto& W = existing->second;
				if (isDying) {  // On Death Dying Distribution
					W.distributed = G.distributed;
					W.isDeathOutfit = G.isDeathOutfit;
					W.isFinalOutfit = G.isFinalOutfit;
					return W;
				} else if (isDead) {        // Regular/Death Dead Distribution
					if (G.isDeathOutfit) {  // On Death Dead Distribution
						if (!W.isDeathOutfit && actor->HasOutfitItems(existing->second.distributed)) {
							W.distributed = G.distributed;
							W.isDeathOutfit = G.isDeathOutfit;
							W.isFinalOutfit = G.isFinalOutfit;
							return W;
						}                      // If Worn outfit is either death outfit or it was already looted, we don't allow changing it.
					}                          // Regular Dead Distribution (not allowed to overwrite anything)
				} else {                       // Regular Alive Distribution
					assert(!G.isDeathOutfit);  // When alive only regular outfits can be worn.
					if (!W.isFinalOutfit) {
						W.distributed = G.distributed;
						W.isDeathOutfit = G.isDeathOutfit;
						W.isFinalOutfit = G.isFinalOutfit;
						return W;
					}
				}
			} else {
				if (isDying) {  // On Death Dying Distribution
					return wornReplacements.try_emplace(actor->formID, G).first->second;
				} else if (isDead) {  // Regular/Death Dead Distribution
					if (const auto npc = actor->GetActorBase(); npc && npc->defaultOutfit && actor->HasOutfitItems(npc->defaultOutfit)) {
						return wornReplacements.try_emplace(actor->formID, G).first->second;
					}     // In both On Death and Regular Distributions if Worn outfit was already looted, we don't allow changing it.
				} else {  // Regular Alive Distribution
					return wornReplacements.try_emplace(actor->formID, G).first->second;
				}
			}
		} else {  // If there is no distributed outfit, we treat it as a marker that Distribution didn't pick any outfit for this actor.
			// In this case we want to lock in on the current outfit of the actor if they are dead.
			if (const auto existing = wornReplacements.find(actor->formID); existing != wornReplacements.end()) {
				auto& W = existing->second;
				if (isDying) {               // On Death Dying Distribution
					W.isDeathOutfit = true;  // Persist current outfit as Death Outfit
				} else if (NPCData::IsDead(actor)) {
					if (G.isDeathOutfit) {  // On Death Dead Distribution
						W.isDeathOutfit = true;
					}     // Regular Dead Distribution (just forwards the outfit)
				} else {  // Regular Alive Distribution
					if (!W.isFinalOutfit) {
						RevertOutfit(actor, W);
						wornReplacements.erase(existing);
					}
				}
			} else {
				if (isDying || NPCData::IsDead(actor)) {  // Persist default outfit
					if (const auto npc = actor->GetActorBase(); npc && npc->defaultOutfit) {
						wornReplacements.try_emplace(actor->formID, npc->defaultOutfit, G.isDeathOutfit, false).first->second;
					}
				}
			}
		}
		return std::nullopt;
	}

	std::optional<Manager::OutfitReplacement> Manager::ResolvePendingOutfit(const NPCData& data, RE::BGSOutfit* outfit, bool isDeathOutfit, bool isFinalOutfit)
	{
		const auto actor = data.GetActor();
		const bool isDead = data.IsDead();
		const bool isDying = data.IsDying();

		assert(!isDeathOutfit || isDying || isDead);  // If outfit is death outfit then actor must be dying or dead, otherwise there is a mistake in the code.
		assert(!isDying || isDeathOutfit);            // If actor is dying then outfit must be from On Death Distribution, otherwise there is a mistake in the code.

		WriteLocker lock(_pendingLock);
		if (const auto pending = pendingReplacements.find(actor->formID); pending != pendingReplacements.end()) {
			auto& G = pending->second;  // Named according to the Resolution Table, outfit corresponds to L from that table.
			// Null outfit in this case means that additional distribution didn't pick new outfit.
			// When there is no pending replacement and outfit is null we store it to indicate that Outfit distribution failed to this actor,
			// During ResolveWornOutfit, such empty replacement will indicate whether or not we want to lock in on a dead NPC's current outfit.
			if (!outfit) {
				// We also allow final flag to be set (but not reset)
				if (isFinalOutfit && !G.isFinalOutfit) {
					G.isFinalOutfit = isFinalOutfit;
				}
				// And same for death outfit flag: regular forward can become death outfit, but not the other way around.
				if (isDeathOutfit && !G.isDeathOutfit) {
					G.isDeathOutfit = isDeathOutfit;
				}

				return G;
			}
			if (isDeathOutfit) {  // On Death Distribution
				if (isDying) {    // On Death Dying Distribution
					if (!G.isFinalOutfit) {
						G.distributed = outfit;
						G.isDeathOutfit = isDeathOutfit;
						G.isFinalOutfit = isFinalOutfit;
					}
				} else {  // On Death Dead Distribution
					// pending can be a regular outfit here (from regular distribution that occurred just before On Death Distribution)
					if (!G.isDeathOutfit || !G.isFinalOutfit) {  // overwrite any regular outfits or non-final death outfits
						G.distributed = outfit;
						G.isDeathOutfit = isDeathOutfit;
						G.isFinalOutfit = isFinalOutfit;
					}
				}
			} else {  // Regular Distribution
				// In both Dead and Alive state pending replacements are allowed to overwrite non-final outfits.
				if (!G.isFinalOutfit && !G.isDeathOutfit) {
					G.distributed = outfit;
					G.isDeathOutfit = isDeathOutfit;
					G.isFinalOutfit = isFinalOutfit;
				}
			}
			return G;
		} else {  // If this is the first outfit in the pending queue, then we just add it.
			return pendingReplacements.try_emplace(actor->formID, outfit, isDeathOutfit, isFinalOutfit).first->second;
		}
	}

	bool Manager::SetOutfit(const NPCData& data, RE::BGSOutfit* outfit, bool isDeathOutfit, bool isFinalOutfit)
	{
		const auto actor = data.GetActor();
		const auto npc = data.GetNPC();

		if (!actor || !npc) {  // invalid call
			return false;
		}

		const auto defaultOutfit = npc->defaultOutfit;

		if (!defaultOutfit) {
			return false;
		}

		// If outfit is nullptr, we just track that distribution didn't provide any outfit for this actor.
		if (outfit) {
			//#ifndef NDEBUG
			logger::info("Evaluating outfit for {}", *actor);
			logger::info("\tDefault Outfit: {}", *defaultOutfit);
			if (auto worn = wornReplacements.find(actor->formID); worn != wornReplacements.end()) {
				logger::info("\tWorn Outfit: {}", *worn->second.distributed);
			} else {
				logger::info("\tWorn Outfit: None");
			}
			logger::info("\tNew Outfit: {}", *outfit);
			//#endif
			if (!CanEquipOutfit(actor, outfit)) {
				//#ifndef NDEBUG
				logger::warn("\tAttempted to set Outfit {} that can't be worn by given actor.", *outfit);
				//#endif
				return false;
			}
		}

		if (auto replacement = ResolvePendingOutfit(data, outfit, isDeathOutfit, isFinalOutfit); replacement) {
			//#ifndef NDEBUG
			if (replacement->distributed) {
				logger::info("\tResolved Pending Outfit: {}", *replacement->distributed);
			}
			//#endif
		}

		return true;
	}

	bool Manager::ApplyOutfit(RE::Actor* actor, RE::BGSOutfit* outfit, bool shouldUpdate3D) const
	{
		if (!actor) {
			return false;
		}

		const auto npc = actor->GetActorBase();
		if (!npc) {
			return false;
		}

		// If default outfit is nullptr, then actor can't wear any outfit at all.
		if (!npc->defaultOutfit) {
			return false;
		}

		//#ifndef NDEBUG
		logger::info("[BEFORE EQUIP] Outfit items present in {} inventory", *actor);
		LogWornOutfitItems(actor);
		//#endif

		if (IsSuspendedReplacement(actor)) {
			//#ifndef NDEBUG
			logger::info("\t\tSkipping outfit equip because distribution is suspended for {}", *actor);
			//#endif
			return false;
		}

		auto itemsMap = GetAllOutfitItems(actor);

		if (auto items = itemsMap.find(outfit); items != itemsMap.end()) {
			std::set<RE::FormID> newOutfitItemIDs{};
			for (const auto& obj : outfit->outfitItems) {
				newOutfitItemIDs.insert(obj->formID);
			}
			std::set<RE::FormID> wornOutfitItemIDs{};
			for (const auto& [item, isWorn] : items->second) {
				if (isWorn) {
					wornOutfitItemIDs.insert(item->formID);
				}
			}

			if (newOutfitItemIDs == wornOutfitItemIDs) {
				//#ifndef NDEBUG
				logger::info("\t\tOutfit {} is already equipped on {}", *outfit, *actor);
				//#endif
				return true;
			}
		}

		//#ifndef NDEBUG
		logger::info("\t\tEquipping Outfit {}", *outfit);
		//#endif
		actor->InitInventoryIfRequired();
		actor->RemoveOutfitItems(nullptr);
		if (!actor->IsDisabled()) {
			AddWornOutfit(actor, outfit, shouldUpdate3D);
			actor->WornArmorChanged();  // This should make sure game updates perks and whatnot that might be dependent on the worn outfit.
		}
		//#ifndef NDEBUG
		logger::info("[AFTER EQUIP] Outfit items present in {} inventory", *actor);
		LogWornOutfitItems(actor);
		//#endif
		return true;
	}

	void Manager::RestoreOutfit(RE::Actor* actor)
	{
		WriteLocker lock(_wornLock);
		if (auto replacement = wornReplacements.find(actor->formID); replacement != wornReplacements.end()) {
			auto& W = replacement->second;
			if (W.distributed == actor->GetActorBase()->defaultOutfit) {
				wornReplacements.erase(replacement);
			} else {
				W.isDeathOutfit = false;
			}
		}
	}

	bool Manager::RevertOutfit(RE::Actor* actor, const OutfitReplacement& replacement) const
	{
		//#ifndef NDEBUG
		logger::info("\tReverting Outfit Replacement for {}", *actor);
		logger::info("\t\t{:R}", replacement);
		//#endif
		return ApplyOutfit(actor, actor->GetActorBase()->defaultOutfit);
	}

	RE::BGSOutfit* Manager::GetInitialOutfit(const RE::Actor* actor) const
	{
		if (const auto npc = actor->GetActorBase(); npc) {
			ReadLocker lock(_initialLock);
			if (const auto it = initialOutfits.find(npc->formID); it != initialOutfits.end()) {
				return it->second;
			}
		}
		return nullptr;
	}

	std::optional<Manager::OutfitReplacement> Manager::GetWornOutfit(const RE::Actor* actor) const
	{
		ReadLocker lock(_wornLock);
		if (const auto it = wornReplacements.find(actor->formID); it != wornReplacements.end()) {
			return it->second;
		}
		return std::nullopt;
	}

	std::optional<Manager::OutfitReplacement> Manager::PopWornOutfit(const RE::Actor* actor)
	{
		WriteLocker lock(_wornLock);
		if (const auto it = wornReplacements.find(actor->formID); it != wornReplacements.end()) {
			auto replacement = it->second;
			wornReplacements.erase(it);
			return replacement;
		}
		return std::nullopt;
	}

	Manager::OutfitReplacementMap Manager::GetWornOutfits() const
	{
		ReadLocker lock(_wornLock);
		return wornReplacements;
	}

	std::optional<Manager::OutfitReplacement> Manager::GetPendingOutfit(const RE::Actor* actor) const
	{
		ReadLocker lock(_pendingLock);
		if (const auto it = pendingReplacements.find(actor->formID); it != pendingReplacements.end()) {
			return it->second;
		}
		return std::nullopt;
	}

	bool Manager::HasPendingOutfit(const RE::Actor* actor) const
	{
		ReadLocker lock(_pendingLock);
		return pendingReplacements.find(actor->formID) != pendingReplacements.end();
	}

	bool Manager::IsSuspendedReplacement(const RE::Actor* actor) const
	{
		if (const auto npc = actor->GetActorBase(); npc && npc->defaultOutfit) {
			if (const auto initialOutfit = GetInitialOutfit(actor)) {
				return initialOutfit != npc->defaultOutfit;
			}
		}

		return false;
	}

#pragma endregion

#pragma region Hooks Handling
	RE::BSEventNotifyControl Manager::ProcessEvent(const RE::TESFormDeleteEvent* event, RE::BSTEventSource<RE::TESFormDeleteEvent>*)
	{
		if (event && event->formID != 0) {
			{
				WriteLocker lock(_wornLock);
				wornReplacements.erase(event->formID);
			}
			{
				WriteLocker lock(_pendingLock);
				pendingReplacements.erase(event->formID);
			}
			{
				WriteLocker lock(_initialLock);
				initialOutfits.erase(event->formID);
			}
		}
		return RE::BSEventNotifyControl::kContinue;
	}

	RE::BSEventNotifyControl Manager::ProcessEvent(const RE::TESDeathEvent* event, RE::BSTEventSource<RE::TESDeathEvent>*)
	{
		if (!event || event->dead) {
			return RE::BSEventNotifyControl::kContinue;
		}

		if (const auto actor = event->actorDying->As<RE::Actor>(); actor && !actor->IsPlayerRef()) {
			if (const auto outfit = ResolveWornOutfit(actor, true); outfit) {
				ApplyOutfit(actor, outfit->distributed);
			}
		}

		return RE::BSEventNotifyControl::kContinue;
	}

	RE::BSEventNotifyControl Manager::ProcessEvent(const RE::TESContainerChangedEvent* event, RE::BSTEventSource<RE::TESContainerChangedEvent>*)
	{
		if (event) {
			auto fromID = event->oldContainer;
			auto toID = event->newContainer;
			auto itemID = event->baseObj;
			auto count = event->itemCount;

			if (const auto item = RE::TESForm::LookupByID<RE::TESBoundObject>(itemID)) {
				if (const auto from = RE::TESForm::LookupByID<RE::TESObjectREFR>(fromID); from) {
					if (const auto fromActor = from->As<RE::Actor>()) {
						if (const auto to = RE::TESForm::LookupByID<RE::TESObjectREFR>(toID); to) {
							if (const auto toActor = to->As<RE::Actor>()) {
								logger::info("[ADDITEM] {} took {} {} from {}", *toActor, count, *item, *fromActor);
							} else {
								logger::info("[ADDITEM] {} put {} {} to {}", *fromActor, count, *item, *to);
							}
						} else {
							logger::info("[ADDITEM] {} dropped {} {}", *fromActor, count, *item);
						}
					} else {  // from is inanimate container
						if (const auto to = RE::TESForm::LookupByID<RE::TESObjectREFR>(toID); to) {
							if (const auto toActor = to->As<RE::Actor>()) {
								logger::info("[ADDITEM] {} took {} {} from {}", *toActor, count, *item, *from);
							} else {
								//logger::info("[ADDITEM] {} {} transfered from {} to {}", count, *item, *from, *to);
							}
						} else {
							//logger::info("[ADDITEM] {} {} removed from {}", count, *item, *from);
						}
					}
				} else {  // From is none
					if (const auto to = RE::TESForm::LookupByID<RE::TESObjectREFR>(toID); to) {
						if (const auto toActor = to->As<RE::Actor>()) {
							logger::info("[ADDITEM] {} picked up {} {}", *toActor, count, *item);
						} else {
							//logger::info("[ADDITEM] {} {} transfered to {}", count, *item, *to);
						}
					} else {
						//logger::info("[ADDITEM] {} {} materialized out of nowhere and vanished without a trace.", count, *item);
					}
				}
			}
		}

		return RE::BSEventNotifyControl::kContinue;
	}

	bool Manager::ProcessShouldBackgroundClone(RE::Actor* actor, std::function<bool()> funcCall)
	{
		// For now, we only support a single distribution per game session.
		// As such if SPID_Processed is detected, we assumed that the actor was already given an outfit if needed.
		// Previous setup relied on the fact that each ShouldBackgroundClone call would also refresh distribution of the outfit,
		// allowing OutfitManager to then swap it, thus perform rotation.
		if (!HasPendingOutfit(actor) && !actor->HasKeyword(Distribute::processed)) {
			SetOutfit(actor, nullptr, NPCData::IsDead(actor), false);
		}

		return funcCall();
	}

	RE::NiAVObject* Manager::ProcessLoad3D(RE::Actor* actor, std::function<RE::NiAVObject*()> funcCall)
	{
		if (!isLoadingGame) {
			if (const auto outfit = ResolveWornOutfit(actor, false); outfit) {
				ApplyOutfit(actor, outfit->distributed);
			}
		}

		return funcCall();
	}

	void Manager::ProcessInitItemImpl(RE::TESNPC* npc, std::function<void()> funcCall)
	{
		funcCall();

		if (npc->defaultOutfit) {
			//#ifndef NDEBUG
			//			logger::info("{}: {}", *npc, *npc->defaultOutfit);
			//#endif
			WriteLocker lock(_initialLock);
			initialOutfits.try_emplace(npc->formID, npc->defaultOutfit);
		}
	}

	void Manager::ProcessResurrect(RE::Actor* actor, std::function<void()> funcCall)
	{
		RestoreOutfit(actor);
		funcCall();
		if (const auto wornOutfit = GetWornOutfit(actor); wornOutfit) {
			ApplyOutfit(actor, wornOutfit->distributed);
		}
	}

	bool Manager::ProcessResetReference(RE::Actor* actor, std::function<bool()> funcCall)
	{
		RevertOutfit(actor);
		return funcCall();
	}

	void Manager::ProcessSetOutfitActor(RE::Actor* actor, RE::BGSOutfit* outfit, std::function<void()> funcCall)
	{
		logger::info("[PAPYRUS] SetOutfit({}) was called for actor {}.", *outfit, *actor);

		// Empty outfit might be used to undress the actor.
		if (outfit->outfitItems.empty()) {
			logger::info("\[PAPYRUS] t⚠️ Outfit {} is empty - Actor will appear naked unless followed by another call to SetOutfit.", *outfit);
		}

		// If there is no distributed outfit there is nothing to suspend/resume.
		if (const auto wornOutfit = GetWornOutfit(actor); wornOutfit) {
			const auto initialOutfit = GetInitialOutfit(actor);
			if (initialOutfit) {
				if (initialOutfit == outfit) {
					logger::info("[PAPYRUS] \t▶️ Resuming outfit distribution for {} as defaultOutfit has been reverted to its initial state", *actor);
					// In any case SetOutfit should result in outfit being set as NPC's defaultOutfit.
					if (actor->GetActorBase()->defaultOutfit != outfit) {
						actor->GetActorBase()->SetDefaultOutfit(outfit);
					}
					ApplyOutfit(actor, wornOutfit->distributed, true);  // apply our distributed outfit instead of defaultOutfit
					return;
				} else {
					logger::info("[PAPYRUS] \t⏸️ Suspending outfit distribution for {} due to manual change of the outfit", *actor);
					logger::info("[PAPYRUS] \t\tTo resume distribution SetOutfit({}) should be called for this actor", *initialOutfit);
				}
			}
		}

		funcCall();
	}
#pragma endregion
}
