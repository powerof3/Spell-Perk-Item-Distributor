#pragma once
#include "LookupNPC.h"
#include "Testing.h"

namespace Outfits
{
	struct TestsHelper;

	class Manager :
		public ISingleton<Manager>,
		public RE::BSTEventSink<RE::TESFormDeleteEvent>,
		public RE::BSTEventSink<RE::TESDeathEvent>
	{
	public:
		void HandleMessage(SKSE::MessagingInterface::Message*);

		// TODO: This method should check both initial and distributed outfits in SPID 8 or something.
		/// <summary>
		/// Checks whether the NPC uses specified outfit as the default outfit.
		///
		/// This method checks initial outfit which was set in the plugins.
		/// Outfits distributed on top of the initial outfits won't be recognized in SPID 7.
		/// There should be a way to handle both of them in the future.
		/// </summary>
		/// <param name="NPC">Target NPC to be tested</param>
		/// <param name="Outfit">An outfit that needs to be checked</param>
		/// <returns>True if specified outfit was used as the default outift by the NPC</returns>
		bool HasDefaultOutfit(const RE::TESNPC*, const RE::BGSOutfit*) const;

		/// <summary>
		/// Checks whether the actor can technically wear a given outfit.
		/// Actor can wear an outfit when all of its components are compatible with actor's race.
		///
		/// This method doesn't validate any other logic.
		/// </summary>
		/// <param name="Actor">Target Actor to be tested</param>
		/// <param name="Outfit">An outfit that needs to be equipped</param>
		/// <returns>True if the actor can wear the outfit, false otherwise</returns>
		bool CanEquipOutfit(const RE::Actor*, const RE::BGSOutfit*) const;

		/// <summary>
		/// Sets given outfit as default outfit for the actor.
		///
		/// SetDefaultOutfit does not apply outfits immediately, it only registers them within the manager.
		/// The worn outfit will be resolved and applied once the actor is loaded.
		/// </summary>
		/// <returns>True if the outfit was registered, otherwise false</returns>
		bool SetDefaultOutfit(const NPCData& data, RE::BGSOutfit* outfit, bool isFinal)
		{
			return SetOutfit(data, outfit, false, isFinal);
		}

		/// <summary>
		/// Sets given outfit as death outfit for the actor.
		///
		/// SetDeathOutfit does not apply outfits immediately, it only registers them within the manager.
		/// The worn outfit will be resolved and applied once the actor is loaded.
		///
		/// Death outfits are outfits that come from On Death Distribution and have the highest priority.
		/// Once set, no other outfit can replace the death outfit.
		/// </summary>
		/// <returns>True if the outfit was registered, otherwise false</returns>
		bool SetDeathOutfit(const NPCData& data, RE::BGSOutfit* outfit, bool isFinal)
		{
			return SetOutfit(data, outfit, true, isFinal);
		}

		/// If there is a tracked worn replacement for given actor, it will be immediately reverted to the original.
		bool RevertOutfit(RE::Actor*);

	protected:
		/// TESFormDeleteEvent is used to delete no longer needed replacements when corresponding Actor is deleted.
		RE::BSEventNotifyControl ProcessEvent(const RE::TESFormDeleteEvent* a_event, RE::BSTEventSource<RE::TESFormDeleteEvent>*) override;

		/// TESDeathEvent is used to update outfit after potential Death Distribution of a new outfit.
		RE::BSEventNotifyControl ProcessEvent(const RE::TESDeathEvent*, RE::BSTEventSource<RE::TESDeathEvent>*) override;

	private:
		struct OutfitReplacement
		{
			/// The one that NPC had before SPID distribution.
			RE::BGSOutfit* original;

			/// The one that SPID distributed.
			RE::BGSOutfit* distributed;

			/// FormID of the outfit that was meant to be distributed, but was not recognized during loading (most likely source plugin is no longer active).
			RE::FormID unrecognizedDistributedFormID;

			/// Flag indicating whether the distributed outfit was given by On Death Distribution.
			bool isDeathOutfit;

			/// Flag indicating whether the distributed outfit is final and cannot be replaced with any other outfit.
			bool isFinalOutfit;

			/// Flag indicating that outfit replacement for the actor is suspended and that it should not be applied until resumed.
			///
			/// Replacement is marked as suspended when SPID detect an explicit call to SetOutfit
			bool isSuspended;

			/// Flag indicating whether the replacement points to the same original outfit.
			///
			/// Such replacements are used to "lock" the actor to their current outfit,
			/// so that future attempts to replace an outfit will be ignored.
			/// Refer to the Outfit Resolution table, specifically "Persist" action.
			bool IsIdentity() const { return original == distributed; }

			/// Flag indicating whether the replacement wasn't properly loaded and is now corrupted.
			///
			/// This can happen when the distributed outfit was removed from the game.
			/// In such cases the replacement should be reverted to the original outfit.
			bool IsCorrupted() const { return !distributed; }

			OutfitReplacement() = default;
			OutfitReplacement(RE::BGSOutfit* original, RE::FormID unrecognizedDistributedFormID = 0) :
				original(original),
				distributed(nullptr),
				isDeathOutfit(false),
				isFinalOutfit(false),
				isSuspended(false),
				unrecognizedDistributedFormID(unrecognizedDistributedFormID) {}
			OutfitReplacement(RE::BGSOutfit* original, RE::BGSOutfit* distributed, bool isDeathOutfit, bool isFinalOutfit, bool isSuspended) :
				original(original),
				distributed(distributed),
				isDeathOutfit(isDeathOutfit),
				isFinalOutfit(isFinalOutfit),
				isSuspended(isSuspended),
				unrecognizedDistributedFormID(0) {}

			friend struct TestsHelper;
		};

		using OutfitReplacementMap = std::unordered_map<RE::FormID, OutfitReplacement>;

		/// <summary>
		/// Performs the actual change of the outfit.
		/// </summary>
		/// <param name="actor">Actor for whom outfit should be changed</param>
		/// <param name="outift">The outfit to be set</param>
		/// <returns>True if the outfit was successfully set, false otherwise</returns>
		bool ApplyOutfit(RE::Actor*, RE::BGSOutfit*) const;

		/// <summary>
		/// Performs the actual reversion of the outfit.
		/// </summary>
		/// <param name=""></param>
		/// <param name=""></param>
		/// <returns></returns>
		bool RevertOutfit(RE::Actor*, const OutfitReplacement&) const;

		/// Restores outfit replacement for given actor.
		///
		/// When the actor is resurrected, their previously locked outfit replacement needs to be updated to allow further outfit changes.
		void RestoreOutfit(RE::Actor*);

		/// <summary>
		/// Resolves the outfit that should be worn by the actor.
		///
		/// The resolution looks into pendingReplacements and compares it against current outfit in wornReplacements.
		/// The result is properly updated wornReplacements entry.
		///
		/// See Outfit Resolution Table for detailed resolution logic: https://docs.google.com/spreadsheets/d/1JEhAql7hUURYC63k_fScZ9u8OWni6gN9jHzytNQt-m8/edit?usp=sharing
		/// </summary>
		/// <param name="Actor">Actor for whom outfit is being resolved</param>
		/// <param name="isDying">Flag indicating whether this method is called during Death Event</param>
		/// <returns>Pointer to a worn outfit replacement that needs to be applied. If resolution does not require updating the outfit then nullptr is returned.</returns>
		[[nodiscard]] const OutfitReplacement* const ResolveWornOutfit(RE::Actor*, bool isDying);
		[[nodiscard]] const OutfitReplacement* const ResolveWornOutfit(RE::Actor*, const OutfitReplacementMap::iterator pending, bool isDying);

		/// Resolves the outfit that is a candiate for equipping.
		const OutfitReplacement* const ResolvePendingOutfit(const NPCData&, RE::BGSOutfit*, bool isDeathOutfit, bool isFinalOutfit);

		/// Utility method that validates incoming outfit and uses it to resolve pending outfit.
		bool SetOutfit(const NPCData&, RE::BGSOutfit*, bool isDeathOutfit, bool isFinalOutfit);

		/// This re-creates game's function that performs a similar code, but crashes for unknown reasons :)
		void AddWornOutfit(RE::Actor*, const RE::BGSOutfit*) const;

		/// Lock for replacements.
		mutable Lock _lock;

		/// Map of Actor's FormID and corresponding Outfit Replacements that are being tracked by the manager.
		///
		/// This map is serialized in a co-save and represents the in-memory map of everying that affected NPCs wear.
		OutfitReplacementMap wornReplacements;

		/// Map of Actor's FormID and corresponding Outfit Replacements that are pending to be applied.
		///
		/// During distribution new outfit replacements are placed into this map through ResolvePendingOutfit.
		/// Depending on when a distribution is happening, these replacements will be applied either in Load3D or during SKSE::Load.
		OutfitReplacementMap pendingReplacements;

		/// Map of NPC's FormID and corresponding initial Outfit that is set in loaded plugins.
		///
		/// This map is used for filtering during distribution to be able to provide consistent filtering behavior.
		/// Once the Manager applies new outfit, all filters that use initial outfit of this NPC will stop working,
		/// the reason is that distributed outfits are baked into the save, and are loaded as part of NPC.
		///
		/// The map is constructed with TESNPC::LoadGame hook, at which point defaultOutfit hasn't been loaded yet from the save.
		std::unordered_map<RE::FormID, RE::BGSOutfit*> initialOutfits;

		/// Flag indicating whether there is a loading of a save file in progress.
		///
		/// This flag is used to defer equipping outfits in Load3D hook, until after SKSE::Load event is processed.
		/// By doing so we can properly handle state of the outfits and determine what needs to be equipped.
		bool isLoadingGame = false;

		// Make sure hooks can access private members
		friend struct Load3D;
		friend struct LoadGame;
		friend struct Resurrect;
		friend struct ResetReference;
		friend struct SetOutfit;

		friend struct TestsHelper;

		friend fmt::formatter<Outfits::Manager::OutfitReplacement>;

		static void Load(SKSE::SerializationInterface* interface);
		static void Save(SKSE::SerializationInterface* interface);
		static void Revert(SKSE::SerializationInterface* interface);

		static bool LoadReplacement(SKSE::SerializationInterface*, std::uint32_t version, RE::FormID& actorFormID, OutfitReplacement&);
		static bool SaveReplacement(SKSE::SerializationInterface*, const RE::FormID& actorFormID, const OutfitReplacement&);
	};

	/// OutfitDistributor that sets given outfit as a default outfit.
	static bool SetDefaultOutfit(const NPCData& data, RE::BGSOutfit* outfit, bool isFinal)
	{
		return Manager::GetSingleton()->SetDefaultOutfit(data, outfit, isFinal);
	}

	/// OutfitDistributor that sets given outfit as a death outfit.
	static bool SetDeathOutfit(const NPCData& data, RE::BGSOutfit* outfit, bool isFinal)
	{
		return Manager::GetSingleton()->SetDeathOutfit(data, outfit, isFinal);
	}
}

template <>
struct fmt::formatter<Outfits::Manager::OutfitReplacement>
{
	template <class ParseContext>
	constexpr auto parse(ParseContext& ctx)
	{
		auto it = ctx.begin();
		auto end = ctx.end();

		if (it != end && *it == 'R') {
			reverse = true;
			++it;
		}

		// Check if there are any other format specifiers
		if (it != end && *it != '}') {
			throw fmt::format_error("OutfitReplacement only supports Reversing format");
		}

		return it;
	}

	template <class FormatContext>
	constexpr auto format(const Outfits::Manager::OutfitReplacement& replacement, FormatContext& a_ctx) const
	{
		std::string flags;
		if (replacement.isDeathOutfit) {
			flags += "💀";
		}
		if (replacement.isFinalOutfit) {
			flags += "♾️";
		}
		if (replacement.isSuspended) {
			flags += "⏸️";
		}

		if (replacement.original && replacement.distributed) {
			if (replacement.IsIdentity()) {
				return fmt::format_to(a_ctx.out(), "🔄️ {}", *replacement.original);
			}
			if (reverse) {
				return fmt::format_to(a_ctx.out(), "{} 🔙{} {}", *replacement.original, flags, *replacement.distributed);
			} else if (replacement.isDeathOutfit) {
				return fmt::format_to(a_ctx.out(), "{} {}➡ {}", *replacement.original, flags, *replacement.distributed);
			} else {
				return fmt::format_to(a_ctx.out(), "{} {}➡️ {}", *replacement.original, flags, *replacement.distributed);
			}
		} else if (replacement.original) {
			if (reverse) {
				return fmt::format_to(a_ctx.out(), "{} 🔙{} CORRUPTED [{}:{:08X}]", *replacement.original, flags, RE::FormType::Outfit, replacement.unrecognizedDistributedFormID);
			} else if (replacement.isDeathOutfit) {
				return fmt::format_to(a_ctx.out(), "{} {}➡ CORRUPTED [{}:{:08X}]", *replacement.original, flags, RE::FormType::Outfit, replacement.unrecognizedDistributedFormID);
			} else {
				return fmt::format_to(a_ctx.out(), "{} {}➡️ CORRUPTED [{}:{:08X}]", *replacement.original, flags, RE::FormType::Outfit, replacement.unrecognizedDistributedFormID);
			}
		} else {
			return fmt::format_to(a_ctx.out(), "INVALID REPLACEMENT");
		}
	}

private:
	bool reverse = false;
};
