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
			/// The outfit that the actor was given and is currently worn.
			RE::BGSOutfit* distributed;

			/// FormID of the outfit that was meant to be distributed, but was not recognized during loading (most likely source plugin is no longer active).
			RE::FormID unrecognizedDistributedFormID;

			/// Flag indicating whether the distributed outfit was given by On Death Distribution.
			bool isDeathOutfit;

			/// Flag indicating whether the distributed outfit is final and cannot be replaced with any other outfit.
			bool isFinalOutfit;

			/// Flag indicating whether the replacement wasn't properly loaded and is now corrupted.
			///
			/// This can happen when the distributed outfit was removed from the game.
			/// In such cases the replacement should be reverted to the original outfit.
			bool IsCorrupted() const { return !distributed; }

			OutfitReplacement() = default;
			OutfitReplacement(RE::FormID unrecognizedDistributedFormID) :
				distributed(nullptr),
				isDeathOutfit(false),
				isFinalOutfit(false),
				unrecognizedDistributedFormID(unrecognizedDistributedFormID) {}
			OutfitReplacement(RE::BGSOutfit* distributed, bool isDeathOutfit, bool isFinalOutfit) :
				distributed(distributed),
				isDeathOutfit(isDeathOutfit),
				isFinalOutfit(isFinalOutfit),
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

		/// Checks whether outfit distribution for given Actor is currently suspended.
		/// 
		/// Distribution is suspended when current defaultOutfit of the Actor (NPC) does not match the initial outfit.
		bool IsSuspendedReplacement(const RE::Actor*) const;

		/// Gets an outfit that was set in the plugins for the actor's ActorBase (NPC).
		RE::BGSOutfit* GetInitialOutfit(const RE::Actor*) const;

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
		[[nodiscard]] const OutfitReplacement* const ResolveWornOutfit(RE::Actor*, OutfitReplacementMap::iterator& pending, bool isDying);

		/// Resolves the outfit that is a candiate for equipping.
		const OutfitReplacement* const ResolvePendingOutfit(const NPCData&, RE::BGSOutfit*, bool isDeathOutfit, bool isFinalOutfit);

		/// Utility method that validates incoming outfit and uses it to resolve pending outfit.
		bool SetOutfit(const NPCData&, RE::BGSOutfit*, bool isDeathOutfit, bool isFinalOutfit);

		/// This re-creates game's function that performs a similar code, but crashes for unknown reasons :)
		void AddWornOutfit(RE::Actor*, RE::BGSOutfit*) const;

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
		/// It is used to determine when manual calls to SetOutfit should suspend/resume SPID-managed outfits.
		/// When SetOutfit attempts to set an outfit that is different from the one in initialOutfits, 
		/// any existing outfit replacement will be suspended (ignored). 
		/// An actor will only be able to resume the outfit replacement, once another call to SetOutfit is made with the initialOutfit.
		///
		/// The map is constructed with TESNPC::InitItemImpl hook.
		std::unordered_map<RE::FormID, RE::BGSOutfit*> initialOutfits;

		/// Flag indicating whether there is a loading of a save file in progress.
		///
		/// This flag is used to defer equipping outfits in Load3D hook, until after SKSE::Load event is processed.
		/// By doing so we can properly handle state of the outfits and determine what needs to be equipped.
		bool isLoadingGame = false;

		// Make sure hooks can access private members
		friend struct ShouldBackgroundClone;
		friend struct Load3D;
		friend struct InitItemImpl;
		friend struct Resurrect;
		friend struct ResetReference;
		friend struct SetOutfitActor;

		// Hooks handling.
		bool            ProcessShouldBackgroundClone(RE::Actor*, std::function<bool()> funcCall);
		RE::NiAVObject* ProcessLoad3D(RE::Actor*, std::function<RE::NiAVObject*()> funcCall);
		void            ProcessInitItemImpl(RE::TESNPC*, std::function<void()> funcCall);
		void            ProcessResurrect(RE::Actor*, std::function<void()> funcCall);
		bool            ProcessResetReference(RE::Actor*, std::function<bool()> funcCall);
		bool            ProcessSetOutfitActor(RE::Actor*, RE::BGSOutfit*, std::function<bool()> funcCall);

		friend struct TestsHelper;

		friend fmt::formatter<Outfits::Manager::OutfitReplacement>;

		static void Load(SKSE::SerializationInterface* interface);
		static void Save(SKSE::SerializationInterface* interface);

		static bool LoadReplacementV1(SKSE::SerializationInterface*, RE::FormID& actorFormID, OutfitReplacement&);
		static bool LoadReplacementV2(SKSE::SerializationInterface*, RE::FormID& actorFormID, OutfitReplacement&);
		static bool LoadReplacementV3(SKSE::SerializationInterface*, RE::FormID& actorFormID, OutfitReplacement&);
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

		if (replacement.distributed) {
			if (reverse) {
				return fmt::format_to(a_ctx.out(), "🔙{} {}", flags, *replacement.distributed);
			} else {
				return fmt::format_to(a_ctx.out(), "{}➡️ {}", flags, *replacement.distributed);
			}
		} else {
			if (replacement.unrecognizedDistributedFormID > 0) {
				if (reverse) {
					return fmt::format_to(a_ctx.out(), "🔙{} CORRUPTED [{}:{:08X}]", flags, RE::FormType::Outfit, replacement.unrecognizedDistributedFormID);
				} else {
					return fmt::format_to(a_ctx.out(), "{}➡️ CORRUPTED [{}:{:08X}]", flags, RE::FormType::Outfit, replacement.unrecognizedDistributedFormID);
				}
			} else {
				return fmt::format_to(a_ctx.out(), "🔄️ Use Default");
			}
		}
	}

private:
	bool reverse = false;
};
