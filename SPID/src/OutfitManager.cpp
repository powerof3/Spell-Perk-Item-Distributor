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
		bool Load(SKSE::SerializationInterface* interface, T*& output)
		{
			RE::FormID id = 0;

			if (!details::Read(interface, id)) {
				return false;
			}

			if (!id) {  // If ID was 0 it means we don't have the outfit stored in this record.
				output = nullptr;
				return true;
			}

			if (!interface->ResolveFormID(id, id)) {
				return false;
			}

			if (const auto form = RE::TESForm::LookupByID<T>(id); form) {
				output = form;
				return true;
			}

			return false;
		}

		bool Load(SKSE::SerializationInterface* interface, RE::Actor*& loadedActor, RE::BGSOutfit*& loadedOriginalOutfit, RE::BGSOutfit*& loadedDistributedOutfit)
		{
			if (!Load(interface, loadedActor)) {
				logger::warn("Failed to load Outfit Replacement record: Corrupted actor.");
				return false;
			}

			if (!Load(interface, loadedOriginalOutfit)) {
				logger::warn("Failed to load Outfit Replacement record: Corrupted original outfit.");
				return false;
			}

			if (!Load(interface, loadedDistributedOutfit)) {
				logger::warn("Failed to load Outfit Replacement record: Corrupted distributed outfit.");
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
		serializationInterface->SetRevertCallback(Revert);

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

	bool CanEquipOutfit(const RE::Actor* actor, RE::BGSOutfit* outfit)
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

	bool Manager::SetDefaultOutfit(RE::Actor* actor, RE::BGSOutfit* outfit, bool allowOverwrites)
	{
		if (!actor || !outfit) {
			return false;
		}

		auto* npc = actor->GetActorBase();
		auto  defaultOutfit = npc->defaultOutfit;

		if (!allowOverwrites && replacements.find(actor->formID) != replacements.end()) {
			return true;
		}

		if (!CanEquipOutfit(actor, outfit)) {
#ifndef NDEBUG
			logger::warn("Attempted to equip Outfit that can't be worn by given actor. Actor: {}; Outfit: {}", *actor, *outfit);
#endif
			return false;
		}

		actor->SetDefaultOutfit(outfit, false);  // Having true here causes infinite loading. It seems that it works either way.

		if (auto previous = replacements.find(actor->formID); previous != replacements.end()) {
			previous->second.distributed = outfit;
		} else if (defaultOutfit) {
			replacements.try_emplace(actor->formID, defaultOutfit, outfit);
		}

		return true;
	}

	void Manager::UseOriginalOutfit(RE::Actor* actor)
	{
		if (auto npc = actor->GetActorBase(); npc && npc->defaultOutfit) {
			if (replacements.find(actor->formID) != replacements.end()) {
				logger::warn("Overwriting replacement for {}", *actor);
			}
			replacements.try_emplace(actor->formID, npc->defaultOutfit);
		}
	}

	void Manager::Load(SKSE::SerializationInterface* a_interface)
	{
		logger::info("{:*^30}", "LOADING");

		auto* manager = Manager::GetSingleton();

		std::unordered_map<RE::Actor*, OutfitReplacement> loadedReplacements;
		auto&                                             newReplacements = manager->replacements;

		std::uint32_t type, version, length;

		while (a_interface->GetNextRecordInfo(type, version, length)) {
			if (type == Data::recordType) {
				RE::Actor*     actor;
				RE::BGSOutfit* original;
				RE::BGSOutfit* distributed;
				if (Data::Load(a_interface, actor, original, distributed)) {
					OutfitReplacement replacement(original, distributed);
#ifndef NDEBUG
					logger::info("\tLoaded Outfit Replacement ({}) for actor {}", replacement, *actor);
#endif
					loadedReplacements[actor] = replacement;
				}
			}
		}

		logger::info("Loaded {} Outfit Replacements", loadedReplacements.size());
		for (const auto& pair : loadedReplacements) {
			logger::info("\t{}", *pair.first);
			logger::info("\t\t{}", pair.second);
		}

		logger::info("Cached {} Outfit Replacements", newReplacements.size());
		for (const auto& pair : newReplacements) {
			if (const auto actor = RE::TESForm::LookupByID<RE::Actor>(pair.first); actor) {
				logger::info("\t{}", *actor);
			}
			logger::info("\t\t{}", pair.second);
		}

		std::uint32_t revertedCount = 0;
		for (const auto& it : loadedReplacements) {
			const auto& actor = it.first;
			const auto& replacement = it.second;

			if (auto newIt = newReplacements.find(actor->formID); newIt != newReplacements.end()) {
				if (newIt->second.UsesOriginalOutfit()) {                                                                        // If new replacement uses original outfit
					if (!replacement.UsesOriginalOutfit() && replacement.distributed == actor->GetActorBase()->defaultOutfit) {  // but previous one doesn't and NPC still wears the distributed outfit
#ifndef NDEBUG
						logger::info("\tReverting Outfit Replacement for {}", *actor);
						logger::info("\t\t{}", replacement);
#endif
						if (actor->SetDefaultOutfit(replacement.original, false)) {  // Having true here causes infinite loading. It seems that it works either way.
							++revertedCount;
						}
					}
				} else {                                            // If new replacement
					newIt->second.original = replacement.original;  // if there was a previous distribution we want to forward original outfit from there to new distribution.
				}

			} else {  // If there is no new distribution, we want to keep the old one, assuming that whatever outfit is stored in this replacement is what NPC still wears in this save file
				newReplacements[actor->formID] = replacement;
			}
		}

		if (revertedCount) {
			logger::info("Reverted {} no longer existing Outfit Replacements", revertedCount);
		}
	}

	void Manager::Save(SKSE::SerializationInterface* interface)
	{
		logger::info("{:*^30}", "SAVING");

		auto replacements = Manager::GetSingleton()->replacements;

		logger::info("Saving {} distributed outfits...", replacements.size());

		std::uint32_t savedCount = 0;
		for (const auto& pair : replacements) {
			if (!Data::Save(interface, pair.first, pair.second.original, pair.second.distributed)) {
				if (const auto actor = RE::TESForm::LookupByID<RE::Actor>(pair.first); actor) {
					logger::error("Failed to save Outfit Replacement ({}) for {}", pair.second, *actor);
				}

				continue;
			}
#ifndef NDEBUG
			if (const auto actor = RE::TESForm::LookupByID<RE::Actor>(pair.first); actor) {
				logger::info("\tSaved Outfit Replacement ({}) for actor {}", pair.second, *actor);
			}
#endif
			++savedCount;
		}

		logger::info("Saved {} names", savedCount);
	}

	void Manager::Revert(SKSE::SerializationInterface*)
	{
		logger::info("{:*^30}", "REVERTING");
		Manager::GetSingleton()->replacements.clear();
		logger::info("\tOutfit Replacements have been cleared.");
	}
}
