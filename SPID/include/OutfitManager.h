#pragma once

namespace Outfits
{
	class Manager :
		public ISingleton<Manager>,
		public RE::BSTEventSink<RE::TESFormDeleteEvent>,
		public RE::BSTEventSink<RE::TESDeathEvent>
	{
	public:
		void HandleMessage(SKSE::MessagingInterface::Message*);

		/// <summary>
		/// Checks whether the actor can technically wear a given outfit.
		/// Actor can wear an outfit when all of its components are compatible with actor's race.
		///
		/// This method doesn't validate any other logic.
		/// </summary>
		/// <param name="Actor">Target Actor to be tested</param>
		/// <param name="Outfit">An outfit that needs to be equipped</param>
		/// <returns>True if the actor can wear the outfit, false otherwise</returns>
		bool CanEquipOutfit(const RE::Actor*, RE::BGSOutfit*);

		/// <summary>
		/// Sets given outfit as default outfit for the actor.
		///
		/// SetDefaultOutfit does not apply outfits immediately, it only registers them within the manager.
		/// ApplyOutfit must be called afterwards to actually equip the outfit.
		/// 
		/// For actors that are loading as usual ApplyOutfit will be called automatically during their Load3D.
		/// For Death Distribution, ApplyOutfit will be called automatically during the event processing.
		/// For other runtime distributions, ApplyOutfit should be called manually.
		///
		/// </summary>
		/// <param name="Actor">Target Actor for whom the outfit will be set.</param>
		/// <param name="Outfit">A new outfit to set as the default.</param>
		/// <returns>True if the outfit was set, otherwise false</returns>
		bool SetDefaultOutfit(RE::Actor*, RE::BGSOutfit*);

		/// <summary>
		/// Applies tracked outfit replacement for given actor.
		/// 
		/// SetDefaultOutfit does not apply outfits immediately, it only registers them within the manager.
		/// 
		/// For actors that are loading as usual ApplyOutfit will be called automatically during their Load3D.
		/// For Death Distribution, ApplyOutfit will be called automatically during the event processing.
		/// For other runtime distributions, ApplyOutfit should be called manually.
		///
		/// If there is no replacement for the actor, then nothing will be done.
		/// This method also makes sure to properly remove previously distributed outfit.
		/// </summary>
		/// <param name="actor">Actor for whom outfit should be changed</param>
		/// <returns>True if the outfit was successfully set, false otherwise</returns>
		bool ApplyOutfit(RE::Actor*);

	protected:
		RE::BSEventNotifyControl ProcessEvent(const RE::TESFormDeleteEvent* a_event, RE::BSTEventSource<RE::TESFormDeleteEvent>*) override;

		/// <summary>
		/// TESDeathEvent is used to update outfit after potential Death Distribution of a new outfit.
		/// </summary>
		RE::BSEventNotifyControl ProcessEvent(const RE::TESDeathEvent*, RE::BSTEventSource<RE::TESDeathEvent>*) override;

	private:
		static void Load(SKSE::SerializationInterface*);
		static void Save(SKSE::SerializationInterface*);

		/// <summary>
		/// This method performs the actual change of the outfit.
		/// </summary>
		/// <param name="actor">Actor for whom outfit should be changed</param>
		/// <param name="outift">The outfit to be set</param>
		/// <returns>True if the outfit was successfully set, false otherwise</returns>
		bool ApplyOutfit(RE::Actor*, RE::BGSOutfit*);

		/// This re-creates game's function that performs a similar code, but crashes for unknown reasons :)
		void AddWornOutfit(RE::Actor*, const RE::BGSOutfit*);

		struct OutfitReplacement
		{
			/// The one that NPC had before SPID distribution.
			RE::BGSOutfit* original;

			/// The one that SPID distributed.
			RE::BGSOutfit* distributed;

			/// FormID of the outfit that was meant to be distributed, but was not recognized during loading (most likely source plugin is no longer active).
			RE::FormID unrecognizedDistributedFormID;

			OutfitReplacement() = default;
			OutfitReplacement(RE::BGSOutfit* original, RE::FormID unrecognizedDistributedFormID) :
				original(original), distributed(nullptr), unrecognizedDistributedFormID(unrecognizedDistributedFormID) {}
			OutfitReplacement(RE::BGSOutfit* original, RE::BGSOutfit* distributed) :
				original(original), distributed(distributed), unrecognizedDistributedFormID(0) {}
		};

		friend struct Load3D;

		friend fmt::formatter<Outfits::Manager::OutfitReplacement>;

		/// <summary>
		/// Map of Actor's FormID and corresponding Outfit Replacements that are being tracked by the manager.
		/// 
		/// This map is serialized in a co-save and is used to clean up no longer distributed outfits when loading a previous save.
		/// Latest distribution replacements always take priority over the ones stored in the save file.
		/// </summary>
		std::unordered_map<RE::FormID, OutfitReplacement> replacements;

		/// <summary>
		/// Flag indicating whether there is a loading of a save file in progress.
		/// 
		/// This flag is used to defer equipping outfits in Load3D hook, until after LoadGame event is processed.
		/// By doing so we can properly handle state of the outfits and determine if anything needs to be equipped.
		/// Among other things, this allows to avoid re-equipping looted outfits On Death Distribution after relaunching the game.
		/// </summary>
		bool isLoadingGame = false;
	};
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
		if (replacement.original && replacement.distributed) {
			if (reverse) {
				return fmt::format_to(a_ctx.out(), "{} 🔙 {}", *replacement.original, *replacement.distributed);
			} else {
				return fmt::format_to(a_ctx.out(), "{} ➡️ {}", *replacement.original, *replacement.distributed);
			}
		} else if (replacement.original) {
			return fmt::format_to(a_ctx.out(), "{} 🔙 CORRUPTED [{}:{:08X}]", *replacement.original, RE::FormType::Outfit, replacement.unrecognizedDistributedFormID);
		} else {
			return fmt::format_to(a_ctx.out(), "INVALID REPLACEMENT");
		}
	}

private:
	bool reverse = false;
};
