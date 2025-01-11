#include "OutfitManager.h"
#include "LookupNPC.h"

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
#ifndef NDEBUG
			logger::warn("Failed to load Outfit Replacement record: Unknown actor [{:08X}].", loadedActorFormID);
#endif
			return false;
		}

		RE::BGSOutfit* original;
		if (!details::Load(interface, original, id)) {
#ifndef NDEBUG
			logger::warn("Failed to load Outfit Replacement record: Unknown original outfit [{:08X}].", id);
#endif
			return false;
		}

		if (!details::Load(interface, distributed, id)) {
#ifndef NDEBUG
			logger::warn("Failed to load Outfit Replacement record: Unknown distributed outfit [{:08X}].", id);
#endif
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
#ifndef NDEBUG
			logger::warn("Failed to load Outfit Replacement record: Unknown actor [{:08X}].", loadedActorFormID);
#endif
			return false;
		}

		if (!details::Load(interface, distributed, id)) {
#ifndef NDEBUG
			logger::warn("Failed to load Outfit Replacement record: Unknown distributed outfit [{:08X}].", id);
#endif
			if (!id) {
				return false;
			}
			failedDistributedOutfitFormID = id;
		}

		if (!details::Read(interface, isDeathOutfit) ||
			!details::Read(interface, isFinalOutfit) ||
			!details::Read(interface, isSuspended)) {
#ifndef NDEBUG
			if (auto loadedActor = RE::TESForm::LookupByID<RE::Actor>(loadedActorFormID); loadedActor) {
				logger::warn("Failed to load attributes for Outfit Replacement record. Actor: {}", *loadedActor);
			}
#endif
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
#ifndef NDEBUG
			logger::warn("Failed to load Outfit Replacement record: Unknown actor [{:08X}].", loadedActorFormID);
#endif
			return false;
		}

		if (!details::Load(interface, distributed, id)) {
#ifndef NDEBUG
			logger::warn("Failed to load Outfit Replacement record: Unknown distributed outfit [{:08X}].", id);
#endif
			if (!id) {
				return false;
			}
			failedDistributedOutfitFormID = id;
		}

		if (!details::Read(interface, isDeathOutfit) ||
			!details::Read(interface, isFinalOutfit)) {
#ifndef NDEBUG
			if (auto loadedActor = RE::TESForm::LookupByID<RE::Actor>(loadedActorFormID); loadedActor) {
				logger::warn("Failed to load attributes for Outfit Replacement record. Actor: {}", *loadedActor);
			}
#endif
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
#ifndef NDEBUG
		LOG_HEADER("LOADING");
		std::unordered_map<RE::Actor*, OutfitReplacement> loadedReplacements;
#endif

		auto manager = Manager::GetSingleton();

		WriteLocker lock(manager->_lock);

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
#ifndef NDEBUG
							loadedReplacements[actor] = loadedReplacement;
#endif
						} else {
							manager->RevertOutfit(actor, loadedReplacement);
						}
					}
				}
			}
		}

		auto& pendingReplacements = manager->pendingReplacements;

#ifndef NDEBUG
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
#endif

		// We don't increment iterator here, since ResolveWornOutfit will be erasing each pending entry
		for (auto it = pendingReplacements.begin(); it != pendingReplacements.end();) {
			if (auto actor = RE::TESForm::LookupByID<RE::Actor>(it->first); actor) {
				if (auto resolved = manager->ResolveWornOutfit(actor, it, false); resolved) {
#ifndef NDEBUG
					logger::info("\tActor: {}", *actor);
					logger::info("\t\tResolved: {}", *resolved);
					logger::info("\t\tDefault: {}", *(actor->GetActorBase()->defaultOutfit));
#endif
					manager->ApplyOutfit(actor, resolved->distributed);
				}
			}
		}
	}

	void Manager::Save(SKSE::SerializationInterface* interface)
	{
#ifndef NDEBUG
		LOG_HEADER("SAVING");
#endif
		auto        manager = Manager::GetSingleton();
		ReadLocker  lock(manager->_lock);
		const auto& replacements = manager->wornReplacements;
#ifndef NDEBUG
		logger::info("Saving {} distributed outfits...", replacements.size());
		std::uint32_t savedCount = 0;
#endif

		for (const auto& pair : replacements) {
			if (!SaveReplacement(interface, pair.first, pair.second)) {
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
			++savedCount;
#endif
		}

#ifndef NDEBUG
		logger::info("Saved {} replacements", savedCount);
#endif
	}
#pragma endregion

#pragma region Hooks
	struct ShouldBackgroundClone
	{
		static bool thunk(RE::Character* actor)
		{
#ifndef NDEBUG
			logger::info("Outfits: ShouldBackgroundClone({})", *(actor->As<RE::Actor>()));
#endif
			return Manager::GetSingleton()->ProcessShouldBackgroundClone(actor, [&] { return func(actor); });
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static inline constexpr std::size_t index{ 0 };
		static inline constexpr std::size_t size{ 0x6D };
	};

	/// This hook applies pending outfit replacements before loading 3D model. Outfit Replacements are created by SetDefaultOutfit.
	struct Load3D
	{
		static RE::NiAVObject* thunk(RE::Character* actor, bool a_backgroundLoading)
		{
#ifndef NDEBUG
			logger::info("Outfits: Load3D({}); Background: {}", *(actor->As<RE::Actor>()), a_backgroundLoading);
#endif
			return Manager::GetSingleton()->ProcessLoad3D(actor, [&] { return func(actor, a_backgroundLoading); });
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static inline constexpr std::size_t index{ 0 };
		static inline constexpr std::size_t size{ 0x6A };
	};

	/// This hook builds a map of initialOutfits for all NPCs that can have it.
	/// The map is later used to determine when manual calls to SetOutfit should suspend/resume SPID-managed outfits.
	struct InitItemImpl
	{
		static void thunk(RE::TESNPC* npc)
		{
			Manager::GetSingleton()->ProcessInitItemImpl(npc, [&] { func(npc); });
		}

		static inline REL::Relocation<decltype(thunk)> func;

		static inline constexpr std::size_t index{ 0 };
		static inline constexpr std::size_t size{ 0x13 };
	};

	/// This hook is used to track when NPCs come back to life to clear the isDeathOutfit flag and restore general outfit behavior,
	/// as alive NPCs are allowed to change their outfits (even if looted).
	///
	/// Note: recycleactor console command also triggers resurrect when needed.
	/// Note: NPCs who Start Dead cannot be resurrected.
	struct Resurrect
	{
		static void thunk(RE::Character* actor, bool resetInventory, bool attach3D)
		{
#ifndef NDEBUG
			logger::info("Resurrect({}); IsDead: {}, ResetInventory: {}, Attach3D: {}", *(actor->As<RE::Actor>()), actor->IsDead(), resetInventory, attach3D);
#endif
			return Manager::GetSingleton()->ProcessResurrect(actor, [&] { return func(actor, resetInventory, attach3D); });
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static inline constexpr std::size_t index{ 0 };
		static inline constexpr std::size_t size{ 0xAB };
	};

	/// This hook is used to clear outfit replacements info since the actor will be completely reset,
	/// thus eligible for a fresh distribution.
	struct ResetReference
	{
		static bool thunk(std::int64_t a1, std::int64_t a2, RE::TESObjectREFR* refr, std::int64_t a4, std::int64_t a5, std::int64_t a6, int a7, int* a8)
		{
			if (refr) {
				if (const auto actor = refr->As<RE::Actor>(); actor) {
#ifndef NDEBUG
					logger::info("RecycleActor({})", *(actor->As<RE::Actor>()));
#endif
					return Manager::GetSingleton()->ProcessResetReference(actor, [&] { return func(a1, a2, refr, a4, a5, a6, a7, a8); });
				}
			}
			return func(a1, a2, refr, a4, a5, a6, a7, a8);
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static inline constexpr REL::ID     relocation = RELOCATION_ID(21556, 22038);
		static inline constexpr std::size_t offset = OFFSET(0x4B, 0x4B);
	};

	/// This hook is used to suspend/resume outfit replacements when Papyrus function SetOutfit is called.
	struct SetOutfitActor
	{
		static bool thunk(RE::Actor* actor, RE::BGSOutfit* outfit, bool forceUpdate)
		{
			return Manager::GetSingleton()->ProcessSetOutfitActor(actor, outfit, [&] { return func(actor, outfit, forceUpdate); });
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static inline constexpr REL::ID     relocation = RELOCATION_ID(53931, 54755);
		static inline constexpr std::size_t offset = OFFSET(0x86, 0x86);
	};

	RE::BSEventNotifyControl Manager::ProcessEvent(const RE::TESFormDeleteEvent* a_event, RE::BSTEventSource<RE::TESFormDeleteEvent>*)
	{
		WriteLocker lock(_lock);
		if (a_event && a_event->formID != 0) {
			wornReplacements.erase(a_event->formID);
			pendingReplacements.erase(a_event->formID);
			initialOutfits.erase(a_event->formID);
		}
		return RE::BSEventNotifyControl::kContinue;
	}

	RE::BSEventNotifyControl Manager::ProcessEvent(const RE::TESDeathEvent* a_event, RE::BSTEventSource<RE::TESDeathEvent>*)
	{
		if (!a_event || a_event->dead) {
			return RE::BSEventNotifyControl::kContinue;
		}

		if (const auto actor = a_event->actorDying->As<RE::Actor>(); actor && !actor->IsPlayerRef()) {
			WriteLocker lock(_lock);
			if (const auto outfit = ResolveWornOutfit(actor, true); outfit) {
				ApplyOutfit(actor, outfit->distributed);
			}
		}

		return RE::BSEventNotifyControl::kContinue;
	}
#pragma endregion

	void Manager::HandleMessage(SKSE::MessagingInterface::Message* message)
	{
		switch (message->type) {
		case SKSE::MessagingInterface::kPostLoad:
			{
				logger::info("Outfit Manager:");

				const auto serializationInterface = SKSE::GetSerializationInterface();
				serializationInterface->SetUniqueID(serializationKey);
				serializationInterface->SetSaveCallback(Save);
				serializationInterface->SetLoadCallback(Load);

				if (const auto scripts = RE::ScriptEventSourceHolder::GetSingleton()) {
					scripts->AddEventSink<RE::TESFormDeleteEvent>(this);
					logger::info("\t\tRegistered for {}.", typeid(RE::TESFormDeleteEvent).name());
				}

				if (const auto scripts = RE::ScriptEventSourceHolder::GetSingleton()) {
					scripts->AddEventSink<RE::TESDeathEvent>(this);
					logger::info("\t\tRegistered for {}.", typeid(RE::TESDeathEvent).name());
				}

				stl::write_vfunc<RE::TESNPC, InitItemImpl>();
				logger::info("\t\tInstalled InitItemImpl hook.");

				stl::write_vfunc<RE::Character, ShouldBackgroundClone>();
				logger::info("\t\tInstalled ShouldBackgroundClone hook.");

				stl::write_vfunc<RE::Character, Load3D>();
				logger::info("\t\tInstalled Load3D hook.");

				stl::write_vfunc<RE::Character, Resurrect>();
				logger::info("\t\tInstalled Resurrect hook.");

				stl::write_thunk_call<ResetReference>();
				logger::info("\t\tInstalled ResetReference hook.");

				stl::write_thunk_call<SetOutfitActor>();
				logger::info("\t\tInstalled SetOutfit hook.");
			}
			break;
		case SKSE::MessagingInterface::kDataLoaded:
			{
				logger::info("\t\tTracking {} initial outfits. (in memory size: ~ {} Kb)", initialOutfits.size(), initialOutfits.size() * 12);  // 4 bytes for formID, 8 bytes for pointer
			}
			break;
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
		WriteLocker lock(_lock);
		if (const auto replacement = wornReplacements.find(actor->formID); replacement != wornReplacements.end()) {
			wornReplacements.erase(replacement);
			return RevertOutfit(actor, replacement->second);
		}
		return false;
	}

	const Manager::OutfitReplacement* const Manager::ResolveWornOutfit(RE::Actor* actor, bool isDying)
	{
		if (auto pending = pendingReplacements.find(actor->formID); pending != pendingReplacements.end()) {
			return ResolveWornOutfit(actor, pending, isDying);
		}

		return nullptr;
	}

	const Manager::OutfitReplacement* const Manager::ResolveWornOutfit(RE::Actor* actor, OutfitReplacementMap::iterator& pending, bool isDying)
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
					return &W;
				} else if (isDead) {        // Regular/Death Dead Distribution
					if (G.isDeathOutfit) {  // On Death Dead Distribution
						if (!W.isDeathOutfit && actor->HasOutfitItems(existing->second.distributed)) {
							W.distributed = G.distributed;
							W.isDeathOutfit = G.isDeathOutfit;
							W.isFinalOutfit = G.isFinalOutfit;
							return &W;
						}                      // If Worn outfit is either death outfit or it was already looted, we don't allow changing it.
					}                          // Regular Dead Distribution (not allowed to overwrite anything)
				} else {                       // Regular Alive Distribution
					assert(!G.isDeathOutfit);  // When alive only regular outfits can be worn.
					if (!W.isFinalOutfit) {
						W.distributed = G.distributed;
						W.isDeathOutfit = G.isDeathOutfit;
						W.isFinalOutfit = G.isFinalOutfit;
						return &W;
					}
				}
			} else {
				if (isDying) {  // On Death Dying Distribution
					return &wornReplacements.try_emplace(actor->formID, G).first->second;
				} else if (isDead) {  // Regular/Death Dead Distribution
					if (const auto npc = actor->GetActorBase(); npc && npc->defaultOutfit && actor->HasOutfitItems(npc->defaultOutfit)) {
						return &wornReplacements.try_emplace(actor->formID, G).first->second;
					}     // In both On Death and Regular Distributions if Worn outfit was already looted, we don't allow changing it.
				} else {  // Regular Alive Distribution
					return &wornReplacements.try_emplace(actor->formID, G).first->second;
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
		return nullptr;
	}

	const Manager::OutfitReplacement* const Manager::ResolvePendingOutfit(const NPCData& data, RE::BGSOutfit* outfit, bool isDeathOutfit, bool isFinalOutfit)
	{
		const auto actor = data.GetActor();
		const bool isDead = data.IsDead();
		const bool isDying = data.IsDying();

		assert(!isDeathOutfit || isDying || isDead);  // If outfit is death outfit then actor must be dying or dead, otherwise there is a mistake in the code.
		assert(!isDying || isDeathOutfit);            // If actor is dying then outfit must be from On Death Distribution, otherwise there is a mistake in the code.

		WriteLocker lock(_lock);
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

				return &G;
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
			return &G;
		} else {  // If this is the first outfit in the pending queue, then we just add it.
			return &pendingReplacements.try_emplace(actor->formID, outfit, isDeathOutfit, isFinalOutfit).first->second;
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

		// If outfit is nullptr, we just track that distribution didn't provide any outfit for this actor.
		if (outfit) {
#ifndef NDEBUG
			logger::info("Evaluating outfit for {}", *actor);
			if (defaultOutfit) {
				logger::info("\tDefault Outfit: {}", *defaultOutfit);
			} else {
				logger::info("\tDefault Outfit: None");
			}
			if (auto worn = wornReplacements.find(actor->formID); worn != wornReplacements.end()) {
				logger::info("\tWorn Outfit: {}", *worn->second.distributed);
			} else {
				logger::info("\tWorn Outfit: None");
			}
			logger::info("\tNew Outfit: {}", *outfit);
#endif
			if (!CanEquipOutfit(actor, outfit)) {
#ifndef NDEBUG
				logger::warn("\tAttempted to set Outfit {} that can't be worn by given actor.", *outfit);
#endif
				return false;
			}
		} else if (!defaultOutfit) {
#ifndef NDEBUG
			logger::warn("\tAttempted to track Outfit for actor that doesn't support outfits.");
#endif
			return false;
		}

		if (auto replacement = ResolvePendingOutfit(data, outfit, isDeathOutfit, isFinalOutfit); replacement) {
#ifndef NDEBUG
			if (replacement->distributed) {
				logger::info("\tResolved Pending Outfit: {}", *replacement->distributed);
			}
#endif
		}

		return true;
	}

	void Manager::AddWornOutfit(RE::Actor* actor, RE::BGSOutfit* outfit) const
	{
		if (const auto invChanges = actor->GetInventoryChanges()) {  // TODO: Add lock here maybe
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
								// queueEquip causes random crashes, probably when the equipping is executed.
								// forceEquip - actually it corresponds to the "PreventRemoval" flag in the game's function,
								//				which determines whether NPC/EquipItem call can unequip the item. See EquipItem Papyrus function.
								RE::ActorEquipManager::GetSingleton()->EquipObject(actor, entryList->object, xList, 1, nullptr, false, true, false, true);
							}
						}
					}
				}
			}
		}
	}

	bool Manager::ApplyOutfit(RE::Actor* actor, RE::BGSOutfit* outfit) const
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

		if (IsSuspendedReplacement(actor)) {
#ifndef NDEBUG
			logger::info("\t\tSkipping outfit equip because distribution is suspended for {}", *actor);
#endif
			return false;
		}

#ifndef NDEBUG
		logger::info("\t\tEquipping Outfit {}", *outfit);
#endif
		actor->InitInventoryIfRequired();
		actor->RemoveOutfitItems(nullptr);
		if (!actor->IsDisabled()) {
			AddWornOutfit(actor, outfit);
		}
		return true;
	}

	void Manager::RestoreOutfit(RE::Actor* actor)
	{
		WriteLocker lock(_lock);
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
#ifndef NDEBUG
		logger::info("\tReverting Outfit Replacement for {}", *actor);
		logger::info("\t\t{:R}", replacement);
#endif
		return ApplyOutfit(actor, actor->GetActorBase()->defaultOutfit);
	}

	RE::BGSOutfit* Manager::GetInitialOutfit(const RE::Actor* actor) const
	{
		const auto npc = actor->GetActorBase();
		if (npc) {
			if (const auto it = initialOutfits.find(npc->formID); it != initialOutfits.end()) {
				return it->second;
			}
		}

		return nullptr;
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
	bool Manager::ProcessShouldBackgroundClone(RE::Actor* actor, std::function<bool()> funcCall)
	{
		bool hasPending = false;
		{
			ReadLocker lock(_lock);
			hasPending = pendingReplacements.contains(actor->formID);
		}
		if (!hasPending) {
			SetOutfit(actor, nullptr, NPCData::IsDead(actor), false);
		}

		return funcCall();
	}

	RE::NiAVObject* Manager::ProcessLoad3D(RE::Actor* actor, std::function<RE::NiAVObject*()> funcCall)
	{
		if (!isLoadingGame) {
			WriteLocker lock(_lock);
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
#ifndef NDEBUG
			logger::info("Tracking initial outfit for {}: {}", *npc, *npc->defaultOutfit);
#endif
			WriteLocker lock(_lock);
			initialOutfits.try_emplace(npc->formID, npc->defaultOutfit);
		}
	}

	void Manager::ProcessResurrect(RE::Actor* actor, std::function<void()> funcCall)
	{
		RestoreOutfit(actor);
		funcCall();
		ReadLocker lock(_lock);
		if (const auto it = wornReplacements.find(actor->formID); it != wornReplacements.end()) {
			ApplyOutfit(actor, it->second.distributed);
		}
	}

	bool Manager::ProcessResetReference(RE::Actor* actor, std::function<bool()> funcCall)
	{
		RevertOutfit(actor);
		return funcCall();
	}

	bool Manager::ProcessSetOutfitActor(RE::Actor* actor, RE::BGSOutfit* outfit, std::function<bool()> funcCall)
	{
		ReadLocker lock(_lock);

		logger::info("SetOutfit({}) was called for actor {}.", *outfit, *actor);
		const auto initialOutfit = GetInitialOutfit(actor);
		if (initialOutfit && initialOutfit == outfit) {
			if (const auto it = wornReplacements.find(actor->formID); it != wornReplacements.end()) {
				logger::info("\t▶️ Resuming outfit distribution for {} as defaultOutfit has been reverted to its initial state", *actor);
				actor->GetActorBase()->SetDefaultOutfit(outfit);  // set outfit back to original
				return ApplyOutfit(actor, it->second.distributed);
			}
		}

		logger::info("\t⏸️ Suspending outfit distribution for {} due to manual change of the outfit", *actor);
		if (initialOutfit) {
			logger::info("\tTo resume distribution SetOutfit({}) should be called for this actor", *initialOutfit);
		}
		return funcCall();  // if suspended, just let the game handle outfit change as usual.
	}
#pragma endregion
}
