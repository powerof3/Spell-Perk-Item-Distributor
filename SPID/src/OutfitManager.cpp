#include "OutfitManager.h"

namespace Outfits
{
	constexpr std::uint32_t serializationKey = 'SPID';
	constexpr std::uint32_t serializationVersion = 1;

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
	}

	namespace Data
	{
		constexpr std::uint32_t recordType = 'OTFT';

		template <typename T>
		bool Load(SKSE::SerializationInterface* interface, T*& output, RE::FormID& formID)
		{
			RE::FormID id = 0;
			output = nullptr;

			if (!details::Read(interface, id)) {
				return false;
			}

			if (!id) {
				return false;
			}

			formID = id;  // save the originally read formID

			if (!interface->ResolveFormID(id, id)) {
				return false;
			}

			formID = id;  // save the resolved formID

			if (const auto form = RE::TESForm::LookupByID<T>(id); form) {
				output = form;
				return true;
			}

			return false;
		}

		bool Load(SKSE::SerializationInterface* interface, RE::Actor*& loadedActor, RE::BGSOutfit*& loadedOriginalOutfit, RE::BGSOutfit*& loadedDistributedOutfit, RE::FormID& failedDistributedOutfitFormID)
		{
			RE::FormID id = 0;
			failedDistributedOutfitFormID = 0;

			if (!Load(interface, loadedActor, id)) {
				logger::warn("Failed to load Outfit Replacement record: Corrupted actor [{:08X}].", id);
				return false;
			}

			if (!Load(interface, loadedOriginalOutfit, id)) {
				logger::warn("Failed to load Outfit Replacement record: Corrupted original outfit [{:08X}].", id);
				return false;
			}

			if (!Load(interface, loadedDistributedOutfit, id)) {
				failedDistributedOutfitFormID = id;
				logger::warn("Failed to load Outfit Replacement record: Corrupted distributed outfit [{:08X}].", id);
				return false;
			}

			return true;
		}

		bool Save(SKSE::SerializationInterface* a_interface, const RE::FormID actorFormId, const RE::BGSOutfit* original, const RE::BGSOutfit* distributed)
		{
			if (!a_interface->OpenRecord(recordType, serializationVersion)) {
				return false;
			}

			return details::Write(a_interface, actorFormId) &&
			       details::Write(a_interface, original->formID) &&
			       details::Write(a_interface, distributed ? distributed->formID : 0);
		}
	}

	void Manager::Register()
	{
		const auto serializationInterface = SKSE::GetSerializationInterface();
		serializationInterface->SetUniqueID(serializationKey);
		serializationInterface->SetSaveCallback(Save);
		serializationInterface->SetLoadCallback(Load);

		if (const auto scripts = RE::ScriptEventSourceHolder::GetSingleton()) {
			scripts->AddEventSink<RE::TESFormDeleteEvent>(GetSingleton());
			logger::info("Registered Outfit Manager for {}", typeid(RE::TESFormDeleteEvent).name());
		}
	}

	RE::BSEventNotifyControl Manager::ProcessEvent(const RE::TESFormDeleteEvent* a_event, RE::BSTEventSource<RE::TESFormDeleteEvent>*)
	{
		if (a_event && a_event->formID != 0) {
			replacements.erase(a_event->formID);
		}
		return RE::BSEventNotifyControl::kContinue;
	}

	ReplacementResult Manager::SetDefaultOutfit(RE::Actor* actor, RE::BGSOutfit* outfit, bool allowOverwrites)
	{
		if (!actor || !outfit) {  // invalid call
			return ReplacementResult::Skipped;
		}

		auto* npc = actor->GetActorBase();

		if (!npc) {
			return ReplacementResult::Skipped;
		}

		auto defaultOutfit = npc->defaultOutfit;

		if (auto existing = replacements.find(actor->formID); existing != replacements.end()) {  // we already have tracked replacement
			if (outfit == defaultOutfit && outfit == existing->second.distributed) {             // if the outfit we are trying to set is already the default one and we have a replacement for it, then we confirm that it was set.
				return ReplacementResult::Set;
			} else if (!allowOverwrites) {  // if we are trying to set any other outfit and overwrites are not allowed, we skip it, indicating overwriting status.
				return ReplacementResult::NotOverwrittable;
			}
		}

		if (!CanEquipOutfit(actor, outfit)) {
#ifndef NDEBUG
			logger::warn("Attempted to equip Outfit that can't be worn by given actor. Actor: {}; Outfit: {}", *actor, *outfit);
#endif
			return ReplacementResult::Skipped;
		}

		SetDefaultOutfit(actor, outfit);

		if (auto previous = replacements.find(actor->formID); previous != replacements.end()) {
			previous->second.distributed = outfit;
		} else if (defaultOutfit) {
			replacements.try_emplace(actor->formID, defaultOutfit, outfit);
		}

		return ReplacementResult::Set;
	}

	bool Manager::CanEquipOutfit(const RE::Actor* actor, RE::BGSOutfit* outfit)
	{
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

	void Manager::AddWornOutfit(RE::Actor* actor, const RE::BGSOutfit* a_outfit)
	{
		bool equippedItems = false;
		if (const auto invChanges = actor->GetInventoryChanges()) {
			if (const auto entryLists = invChanges->entryList) {
				const auto formID = a_outfit->GetFormID();

				for (const auto& entryList : *entryLists) {
					if (entryList && entryList->object && entryList->extraLists) {
						for (const auto& xList : *entryList->extraLists) {
							const auto outfitItem = xList ? xList->GetByType<RE::ExtraOutfitItem>() : nullptr;
							if (outfitItem && outfitItem->id == formID) {
								RE::ActorEquipManager::GetSingleton()->EquipObject(actor, entryList->object, xList, 1, nullptr, true);
								equippedItems = true;
							}
						}
					}
				}
			}
		}

		if (equippedItems) {
			actor->currentProcess->Update3DModel(actor);
		}
	}

	bool Manager::SetDefaultOutfit(RE::Actor* actor, RE::BGSOutfit* outfit)
	{
		if (!actor || !outfit) {
			return false;
		}

		const auto npc = actor->GetActorBase();
		if (!npc || npc->defaultOutfit == outfit) {
			return false;
		}
		actor->RemoveOutfitItems(npc->defaultOutfit);
		npc->SetDefaultOutfit(outfit);
		actor->InitInventoryIfRequired();
		if (!actor->IsDisabled()) {
			AddWornOutfit(actor, outfit);
		}
		return true;
	}

	void Manager::Load(SKSE::SerializationInterface* a_interface)
	{
#ifndef NDEBUG
		logger::info("{:*^30}", "LOADING");
#endif
		auto* manager = Manager::GetSingleton();

		std::unordered_map<RE::Actor*, OutfitReplacement> loadedReplacements;
		auto&                                             newReplacements = manager->replacements;

		std::uint32_t type, version, length;
		int           total = 0;
		while (a_interface->GetNextRecordInfo(type, version, length)) {
			if (type == Data::recordType) {
				RE::Actor*     actor;
				RE::BGSOutfit* original;
				RE::BGSOutfit* distributed;
				RE::FormID     failedDistributedOutfitFormID;
				total++;
				if (Data::Load(a_interface, actor, original, distributed, failedDistributedOutfitFormID); actor) {
					if (distributed) {
						loadedReplacements[actor] = { original, distributed };
					} else {
						loadedReplacements[actor] = { original, failedDistributedOutfitFormID };
					}
				}
			}
		}
#ifndef NDEBUG
		logger::info("Loaded {}/{} Outfit Replacements", loadedReplacements.size(), total);
		for (const auto& pair : loadedReplacements) {
			logger::info("\t{}", *pair.first);
			logger::info("\t\t{}", pair.second);
		}

		logger::info("Current {} Outfit Replacements", newReplacements.size());
		for (const auto& pair : newReplacements) {
			if (const auto actor = RE::TESForm::LookupByID<RE::Actor>(pair.first); actor) {
				logger::info("\t{}", *actor);
			}
			logger::info("\t\t{}", pair.second);
		}

		logger::info("Merging...");
#endif
		std::uint32_t revertedCount = 0;
		std::uint32_t updatedCount = 0;
		for (const auto& it : loadedReplacements) {
			const auto& actor = it.first;
			const auto& replacement = it.second;
			if (auto newIt = newReplacements.find(actor->formID); newIt != newReplacements.end()) {  // If we have some new replacement for this actor
				newIt->second.original = replacement.original; // we want to forward original outfit from the previous replacement to the new one. (so that a chain of outfits like this A->B->C becomes A->C and we'll be able to revert to the very first outfit)
				++updatedCount;
#ifndef NDEBUG
				logger::info("\tUpdating Outfit Replacement for {}", *actor);
				logger::info("\t\t{}", newIt->second);
#endif
			} else if (auto npc = actor->GetActorBase(); !replacement.distributed || npc && replacement.distributed == npc->defaultOutfit) {  // If the replacement is not valid or there is no new replacement, and an actor is currently wearing the same outfit that was distributed to them last time, we want to revert whatever outfit was in previous replacement
#ifndef NDEBUG
				logger::info("\tReverting Outfit Replacement for {}", *actor);
				logger::info("\t\t{:R}", replacement);
#endif
				if (manager->SetDefaultOutfit(actor, replacement.original)) {
					++revertedCount;
				}
			}
		}
#ifndef NDEBUG
		if (revertedCount) {
			logger::info("Reverted {} no longer existing Outfit Replacements", revertedCount);
		}
		if (updatedCount) {
			logger::info("Updated {} existing Outfit Replacements", updatedCount);
		}
#endif
	}

	void Manager::Save(SKSE::SerializationInterface* interface)
	{
#ifndef NDEBUG
		logger::info("{:*^30}", "SAVING");
#endif
		auto replacements = Manager::GetSingleton()->replacements;
#ifndef NDEBUG
		logger::info("Saving {} distributed outfits...", replacements.size());
#endif
		std::uint32_t savedCount = 0;
		for (const auto& pair : replacements) {
			if (!Data::Save(interface, pair.first, pair.second.original, pair.second.distributed)) {
#ifndef NDEBUG
				if (const auto actor = RE::TESForm::LookupByID<RE::Actor>(pair.first); actor) {
					logger::error("Failed to save Outfit Replacement ({}) for {}", pair.second, *actor);
				}
#endif
				continue;
			}
#ifndef NDEBUG
			if (const auto actor = RE::TESForm::LookupByID<RE::Actor>(pair.first); actor) {
				logger::info("\tSaved Outfit Replacement ({}) for actor {}", pair.second, *actor);
			}
#endif
			++savedCount;
		}
#ifndef NDEBUG
		logger::info("Saved {} replacements", savedCount);
#endif
	}
}
