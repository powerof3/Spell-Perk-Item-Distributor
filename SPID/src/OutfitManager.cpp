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

		auto* npc = actor->GetActorBase();

		if (auto previous = outfits.find(actor); previous != outfits.end()) {
			if (previous->second == outfit) {
				return true; // if the new default outfit is the same as previous one, we can skip the rest, because it is already equipped.
			}
			npc->defaultOutfit = previous->second; // restore reference to the previous default outfit, so that actor->SetDefaultOutfit would properly clean up.
		} else if (outfit == npc->defaultOutfit) {
			return true; // if there is no previously distributed outfit and the new default outfit is the same as the original default outfit, we can also skip.
		}
		
		// Otherwise we want to set the new default outfit and add it to our tracker map.
		if (actor->SetDefaultOutfit(outfit, false)) { // Having true here causes infinite loading. It seems that it works either way.
			outfits[actor] = outfit;
			return true;
		}

		return false;
	}

	void Manager::ResetDefaultOutfit(RE::Actor* actor)
	{
		if (!actor) {
			return;
		}

		auto* npc = actor->GetActorBase();
		auto  restore = npc->defaultOutfit;  // TODO: Probably need to add another outfit entry to keep reference to an outfit that was equipped before the previous one got distributed.

		if (auto previous = outfits.find(actor); previous != outfits.end()) {
			auto previousOutfit = previous->second;
			outfits.erase(previous); // remove cached outfit as it's no longer needed.
			if (previousOutfit == npc->defaultOutfit) {
				return;
			}
			npc->defaultOutfit = previous->second; // restore reference to the previous default outfit, so that actor->SetDefaultOutfit would properly clean up.
		}

		actor->SetDefaultOutfit(restore, false); // Having true here causes infinite loading. It seems that it works either way.
	}

	void Manager::Load(SKSE::SerializationInterface* a_interface)
	{
		logger::info("{:*^30}", "LOADING");

		auto*   manager = Manager::GetSingleton();
		std::uint32_t loadedCount = 0;

		auto& outfits = manager->outfits;
		
		std::uint32_t type, version, length;
		outfits.clear();
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
							outfits[actor] = outfit;
							++loadedCount;
						}
					}
				}
			}
		}

		logger::info("Loaded {} distributed outfits", loadedCount);
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
