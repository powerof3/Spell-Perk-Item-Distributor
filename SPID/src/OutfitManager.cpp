#include "OutfitManager.h"

namespace Outfits
{
	constexpr std::uint32_t serializationKey = 'SPID';
	constexpr std::uint32_t serializationVersion = 1;

	using DistributedOutfit = std::pair<RE::FormID, RE::FormID>;

	namespace details
	{
		template <typename T>
		bool Write(SKSE::SerializationInterface* a_interface, const T& data)
		{
			return a_interface->WriteRecordData(&data, sizeof(T));
		}

		template <>
		bool Write(SKSE::SerializationInterface* a_interface, const std::string& data)
		{
			const std::size_t size = data.length();
			return a_interface->WriteRecordData(size) && a_interface->WriteRecordData(data.data(), static_cast<std::uint32_t>(size));
		}

		template <typename T>
		bool Read(SKSE::SerializationInterface* a_interface, T& result)
		{
			return a_interface->ReadRecordData(&result, sizeof(T));
		}

		template <>
		bool Read(SKSE::SerializationInterface* a_interface, std::string& result)
		{
			std::size_t size = 0;
			if (!a_interface->ReadRecordData(size)) {
				return false;
			}
			if (size > 0) {
				result.resize(size);
				if (!a_interface->ReadRecordData(result.data(), static_cast<std::uint32_t>(size))) {
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
		

		bool Load(SKSE::SerializationInterface* a_interface, std::pair<RE::FormID, RE::FormID>& data)
		{
			const bool result = details::Read(a_interface, data.first) &&
			                    details::Read(a_interface, data.second);

			if (!result) {
				logger::warn("Failed to load outfit with FormID [0x{:X}] for NPC with FormID [0x{:X}]", data.second, data.first);
				return false;
			}

			if (!a_interface->ResolveFormID(data.first, data.first)) {
				logger::warn("Failed to load outfit with FormID [0x{:X}] for NPC with FormID [0x{:X}]", data.second, data.first);
				return false;
			}

			if (!a_interface->ResolveFormID(data.second, data.second)) {
				return false;
			}

			return true;
		}

		bool Save(SKSE::SerializationInterface* a_interface, const std::pair<RE::Actor*, RE::BGSOutfit*>& data)
		{
			if (!a_interface->OpenRecord(recordType, serializationVersion)) {
				return false;
			}

			return details::Write(a_interface, data.first->formID) &&
			       details::Write(a_interface, data.second->formID);
		}
	}

	void Manager::Register()
	{
		const auto serializationInterface = SKSE::GetSerializationInterface();
		serializationInterface->SetUniqueID(serializationKey);
		serializationInterface->SetSaveCallback(Save);
		serializationInterface->SetLoadCallback(Load);
		serializationInterface->SetRevertCallback(Revert);
	}

	bool Manager::SetDefaultOutfit(RE::Actor* actor, RE::BGSOutfit* outfit)
	{
		if (!actor || !outfit) {
			return false;
		}

		if (auto previous = outfits.find(actor); previous != outfits.end()) {
			return false;
		} else {
			outfits[actor] = outfit;
			return true;
		}
	}

	void Manager::ApplyDefaultOutfit(RE::Actor* actor) {
		auto* npc = actor->GetActorBase();

		if (auto previous = outfits.find(actor); previous != outfits.end()) {
			if (previous->second != npc->defaultOutfit) {
				actor->SetDefaultOutfit(previous->second, false);  // Having true here causes infinite loading. It seems that it works either way.
			}
		}
	}

	void Manager::ResetDefaultOutfit(RE::Actor* actor, RE::BGSOutfit* previous)
	{
		if (!actor) {
			return;
		}

		auto* npc = actor->GetActorBase();
		auto  restore = npc->defaultOutfit;  // TODO: Probably need to add another outfit entry to keep reference to an outfit that was equipped before the previous one got distributed.

		if (previous) {
			auto previousOutfit = previous;
			outfits.erase(actor); // remove cached outfit as it's no longer needed.
			if (previousOutfit == npc->defaultOutfit) {
				return;
			}
			npc->defaultOutfit = previous; // restore reference to the previous default outfit, so that actor->SetDefaultOutfit would properly clean up.
		}

		actor->SetDefaultOutfit(restore, false); // Having true here causes infinite loading. It seems that it works either way.
	}

	void Manager::Load(SKSE::SerializationInterface* a_interface)
	{
		logger::info("{:*^30}", "LOADING");

		auto*   manager = Manager::GetSingleton();
		std::uint32_t loadedCount = 0;

		std::unordered_map<RE::Actor*, RE::BGSOutfit*> cachedOutfits;
		auto&                                          outfits = manager->outfits;
		
		std::uint32_t type, version, length;

		bool definitionsChanged = false;
		while (a_interface->GetNextRecordInfo(type, version, length)) {
			if (type == Data::recordType) {
				DistributedOutfit data{};
				if (Data::Load(a_interface, data)) {
					if (const auto actor = RE::TESForm::LookupByID<RE::Actor>(data.first); actor) {
						if (const auto outfit = RE::TESForm::LookupByID<RE::BGSOutfit>(data.second); outfit) {
#ifndef NDEBUG
							logger::info("\tLoaded outfit {} for actor {}", *outfit, *actor);
#endif
							cachedOutfits[actor] = outfit;
							++loadedCount;
						}
					}
				}
			}
		}

		logger::info("Loaded {} distributed outfits", loadedCount);

		
		auto keys1 = outfits | std::views::keys;
		auto keys2 = cachedOutfits | std::views::keys;

		std::set<RE::Actor*> newActors(keys1.begin(), keys1.end());
		std::set<RE::Actor*> cachedActors(keys2.begin(), keys2.end());

		std::unordered_set<RE::Actor*> actors;

		std::set_union(newActors.begin(), newActors.end(), cachedActors.begin(), cachedActors.end(), std::inserter(actors, actors.begin()));

		for (const auto& actor : actors) {
			if (const auto it = outfits.find(actor); it != outfits.end()) {
				if (const auto cit = cachedOutfits.find(actor); cit != cachedOutfits.end()) {
					if (it->second != cit->second) {
						manager->ResetDefaultOutfit(actor, cit->second);
					}
				} else {
					manager->ApplyDefaultOutfit(actor);
				}
			} else {
				if (const auto cit = cachedOutfits.find(actor); cit != cachedOutfits.end()) {
					manager->ResetDefaultOutfit(actor, cit->second);
				}
			}
		}
		// TODO: Reset outfits if needed.
		// 1) if nothing is in manager->outfits for given actor, but something is in cachedOutfits
		//		then Reset outfit
		// 2) if something is in manager->outfits for given actor and nothing in cachedOutfits
		//		it should be stored and equipped
		// 3) if something is in both sets we should compare two outfits, and
		// 3.1) if they are the same
		//			do nothing, just store in the set (no equip or reset)
		// 3.2) if they are not the same
		//			reset the cached outfit and equip a new one.
	}

	void Manager::Save(SKSE::SerializationInterface* a_interface)
	{
		logger::info("{:*^30}", "SAVING");

		auto outfits = Manager::GetSingleton()->outfits;

		logger::info("Saving {} distributed outfits...", outfits.size());

		std::uint32_t savedCount = 0;
		for (const auto& data : outfits) {
			if (!Data::Save(a_interface, data)) {
				logger::error("Failed to save outfit {} for {}", *data.second, *data.first);
				continue;
			}
#ifndef NDEBUG
			logger::info("\tSaved outfit {} for actor {}", *data.second, *data.first->GetActorBase());
#endif
			++savedCount;
		}

		logger::info("Saved {} names", savedCount);
	}

	void Manager::Revert(SKSE::SerializationInterface*)
	{
		logger::info("{:*^30}", "REVERTING");
		Manager::GetSingleton()->outfits.clear();
		logger::info("\tOutfits cache has been cleared.");
	}
}
