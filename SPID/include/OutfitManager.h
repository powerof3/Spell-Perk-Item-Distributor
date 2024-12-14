#pragma once

namespace Outfits
{
	enum class ReplacementResult
	{
		/// New outfit for the actor was successfully set.
		Set,
		/// New outfit for the actor is not valid and was skipped.
		Skipped,
		/// Outfit for the actor was already set and the new outfit was not allowed to overwrite it.
		NotOverwrittable
	};
	;

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
		/// <returns>Result of the replacement.</returns>
		ReplacementResult SetDefaultOutfit(RE::Actor*, RE::BGSOutfit*, bool allowOverwrites);

	protected:
		RE::BSEventNotifyControl ProcessEvent(const RE::TESFormDeleteEvent* a_event, RE::BSTEventSource<RE::TESFormDeleteEvent>*) override;

	private:
		static void Load(SKSE::SerializationInterface*);
		static void Save(SKSE::SerializationInterface*);
		static void Revert(SKSE::SerializationInterface*);

		/// <summary>
		/// This method performs the actual change of the outfit.
		/// </summary>
		/// <param name="actor">Actor for whom outfit should be changed</param>
		/// <param name="outift">The outfit to be set</param>
		/// <returns>True if the outfit was successfully set, false otherwise</returns>
		bool SetDefaultOutfit(RE::Actor*, RE::BGSOutfit*);

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

		friend fmt::formatter<Outfits::Manager::OutfitReplacement>;

		std::unordered_map<RE::FormID, OutfitReplacement> replacements;
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
	constexpr auto format(const Outfits::Manager::OutfitReplacement& replacement, FormatContext& a_ctx)
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
