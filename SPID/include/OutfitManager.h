#pragma once

namespace Outfits
{
	class Manager :
		public ISingleton<Manager>,
		public RE::BSTEventSink<RE::TESFormDeleteEvent>
	{
	public:
		static void Register();

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
		/// This method also makes sure to properly remove previously distributed outfit.
		/// </summary>
		/// <param name="Actor">Target Actor for whom the outfit will be set.</param>
		/// <param name="Outfit">A new outfit to set as the default.</param>
		/// <param name="allowOverwrites">If true, the outfit will be set even if the actor already has a distributed outfit.</param>
		/// <returns>True if the outfit was successfully set, false otherwise.</returns>
		bool SetDefaultOutfit(RE::Actor*, RE::BGSOutfit*, bool allowOverwrites);

		/// <summary>
		/// Indicates that given actor didn't receive any distributed outfit and will be using the original one.
		///
		/// This method helps distinguish cases when there was no outfit distribution for the actor vs when we're reloading the save and replacements cache was cleared.
		/// </summary>
		void UseOriginalOutfit(RE::Actor*);

	protected:
		RE::BSEventNotifyControl ProcessEvent(const RE::TESFormDeleteEvent* a_event, RE::BSTEventSource<RE::TESFormDeleteEvent>*) override;

	private:
		static void Load(SKSE::SerializationInterface*);
		static void Save(SKSE::SerializationInterface*);
		static void Revert(SKSE::SerializationInterface*);

		struct OutfitReplacement
		{
			/// The one that NPC had before SPID distribution.
			RE::BGSOutfit* original;

			/// The one that SPID distributed.
			RE::BGSOutfit* distributed;

			OutfitReplacement() = default;
			OutfitReplacement(RE::BGSOutfit* original) :
				original(original), distributed(nullptr) {}
			OutfitReplacement(RE::BGSOutfit* original, RE::BGSOutfit* distributed) :
				original(original), distributed(distributed) {}

			bool UsesOriginalOutfit() const
			{
				return original && !distributed;
			}
		};

		friend fmt::formatter<Outfits::Manager::OutfitReplacement>;

		std::unordered_map<RE::FormID, OutfitReplacement> replacements;
	};
}

template <>
struct fmt::formatter<Outfits::Manager::OutfitReplacement>
{
	template <class ParseContext>
	constexpr auto parse(ParseContext& a_ctx)
	{
		return a_ctx.begin();
	}

	template <class FormatContext>
	constexpr auto format(const Outfits::Manager::OutfitReplacement& replacement, FormatContext& a_ctx)
	{
		if (replacement.UsesOriginalOutfit()) {
			return fmt::format_to(a_ctx.out(), "NO REPLACEMENT (Uses {})", *replacement.original);
		} else if (replacement.original && replacement.distributed) {
			return fmt::format_to(a_ctx.out(), "{} -> {}", *replacement.original, *replacement.distributed);
		} else {
			return fmt::format_to(a_ctx.out(), "INVALID REPLACEMENT");
		}
	}
};
