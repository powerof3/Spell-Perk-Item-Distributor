#include "OutfitManager.h"

namespace Outfits
{
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

	void Manager::InitializeSerialization()
	{
		const auto serializationInterface = SKSE::GetSerializationInterface();
		serializationInterface->SetUniqueID(serializationKey);
		serializationInterface->SetSaveCallback(Save);
		serializationInterface->SetLoadCallback(Load);
	}

	bool Manager::LoadReplacementV1(SKSE::SerializationInterface* interface, RE::FormID& loadedActorFormID, Manager::OutfitReplacement& loadedReplacement)
	{
		RE::FormID     id = 0;
		RE::FormID     failedDistributedOutfitFormID = 0;
		RE::BGSOutfit* distributed;

		if (!details::Load(interface, loadedActorFormID)) {
			//#ifndef NDEBUG
			logger::warn("[🧥][LOAD] Failed to load Outfit Replacement record: Unknown actor [{:08X}].", loadedActorFormID);
			//#endif
			return false;
		}

		RE::BGSOutfit* original;
		if (!details::Load(interface, original, id)) {
			//#ifndef NDEBUG
			logger::warn("[🧥][LOAD] Failed to load Outfit Replacement record: Unknown original outfit [{:08X}].", id);
			//#endif
			return false;
		}

		if (!details::Load(interface, distributed, id)) {
			//#ifndef NDEBUG
			logger::warn("[🧥][LOAD] Failed to load Outfit Replacement record: Unknown distributed outfit [{:08X}].", id);
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
			logger::warn("[🧥][LOAD] Failed to load Outfit Replacement record: Unknown actor [{:08X}].", loadedActorFormID);
			//#endif
			return false;
		}

		if (!details::Load(interface, distributed, id)) {
			//#ifndef NDEBUG
			logger::warn("[🧥][LOAD] Failed to load Outfit Replacement record: Unknown distributed outfit [{:08X}].", id);
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
				logger::warn("[🧥][LOAD] Failed to load attributes for Outfit Replacement record. Actor: {}", *loadedActor);
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
			logger::warn("[🧥][LOAD] Failed to load Outfit Replacement record: Unknown actor [{:08X}].", loadedActorFormID);
			//#endif
			return false;
		}

		if (!details::Load(interface, distributed, id)) {
			//#ifndef NDEBUG
			logger::warn("[🧥][LOAD] Failed to load Outfit Replacement record: Unknown distributed outfit [{:08X}].", id);
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
				logger::warn("[🧥][LOAD] Failed to load attributes for Outfit Replacement record. Actor: {}", *loadedActor);
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
		std::unordered_map<RE::FormID, OutfitReplacement> loadedReplacements;
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
					if (loadedReplacement.distributed) {
						manager->wornReplacements[actorFormID] = loadedReplacement;
						//#ifndef NDEBUG
						loadedReplacements[actorFormID] = loadedReplacement;
						//#endif
					} else if (const auto actor = RE::TESForm::LookupByID<RE::Actor>(actorFormID); actor) {
						logger::warn("[🧥][LOAD] Loaded replacement doesn't have an outfit, reverting actor {}", *actor);
						manager->RevertOutfit(actor, loadedReplacement);
					}
				} else {
					logger::error("[🧥][LOAD] Failed to load replacement");
				}
			}
		}

		auto& pendingReplacements = manager->pendingReplacements;

		//#ifndef NDEBUG
		logger::info("[🧥][LOAD] Loaded {}/{} Outfit Replacements", loadedReplacements.size(), total);
		for (const auto& pair : loadedReplacements) {
			if (const auto actor = RE::TESForm::LookupByID<RE::Actor>(pair.first); actor) {
				logger::info("[🧥][LOAD] \t{}", *actor);
			} else {
				logger::info("[🧥][LOAD] \t[ACHR:{:08X}]", pair.first);
			}
			logger::info("[🧥][LOAD] \t\t{}", pair.second);
		}

		logger::info("[🧥][LOAD] Pending {} Outfit Replacements", pendingReplacements.size());
		for (const auto& pair : pendingReplacements) {
			if (const auto actor = RE::TESForm::LookupByID<RE::Actor>(pair.first); actor) {
				logger::info("[🧥][LOAD] \t{}", *actor);
			}
			logger::info("[🧥][LOAD] \t\t{}", pair.second);
		}

		logger::info("[🧥][LOAD] Applying resolved outfits...");
		//#endif

		// We don't increment iterator here, since ResolveWornOutfit will be erasing each pending entry
		for (auto it = pendingReplacements.begin(); it != pendingReplacements.end();) {
			if (auto actor = RE::TESForm::LookupByID<RE::Actor>(it->first); actor) {
				if (auto resolved = manager->ResolveWornOutfit(actor, it, false); resolved) {
					//#ifndef NDEBUG
					logger::info("[🧥][LOAD] \tActor: {}", *actor);
					logger::info("[🧥][LOAD] \t\tResolved: {}", *resolved);
					logger::info("[🧥][LOAD] \t\tDefault: {}", *(actor->GetActorBase()->defaultOutfit));
					//#endif
					manager->ApplyOutfit(actor, resolved->distributed);
				}
			}
		}
		LOG_HEADER("");
	}

	void Manager::Save(SKSE::SerializationInterface* interface)
	{
		//#ifndef NDEBUG
		LOG_HEADER("SAVING");
		//#endif
		auto       manager = Manager::GetSingleton();
		const auto replacements = manager->GetWornOutfits();
		//#ifndef NDEBUG
		logger::info("[🧥][SAVE] Saving {} distributed outfits...", replacements.size());
		std::uint32_t savedCount = 0;
		//#endif

		for (const auto& pair : replacements) {
			if (!SaveReplacement(interface, pair.first, pair.second)) {
				//#ifndef NDEBUG
				if (const auto actor = RE::TESForm::LookupByID<RE::Actor>(pair.first); actor) {
					logger::error("[🧥][SAVE] Failed to save Outfit Replacement ({}) for {}", pair.second, *actor);
				}
				//#endif
				continue;
			}
			//#ifndef NDEBUG
			if (const auto actor = RE::TESForm::LookupByID<RE::Actor>(pair.first); actor) {
				logger::info("[🧥][SAVE] \tSaved Outfit Replacement ({}) for actor {}", pair.second, *actor);
			}
			++savedCount;
			//#endif
		}

		//#ifndef NDEBUG
		logger::info("[🧥][SAVE] Saved {} replacements", savedCount);
		//#endif
	}
}
