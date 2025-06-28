#include "OutfitManager.h"

namespace Outfits
{
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
						}  // If Worn outfit is either death outfit or it was already looted, we don't allow changing it.
					}  // Regular Dead Distribution (not allowed to overwrite anything)
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
					}  // In both On Death and Regular Distributions if Worn outfit was already looted, we don't allow changing it.
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
					}  // Regular Dead Distribution (just forwards the outfit)
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
}
