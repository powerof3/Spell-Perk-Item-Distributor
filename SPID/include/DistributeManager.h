#pragma once

namespace Distribute
{
	inline constexpr std::string_view processed_EDID{ "SPID_Processed" };
	inline RE::BGSKeyword*            processed{ nullptr };

	inline constexpr std::string_view processedOutfit_EDID{ "SPID_ProcessedOutfit" };
	inline RE::BGSKeyword*            processedOutfit{ nullptr };

	namespace detail
	{
		bool should_process_NPC(RE::TESNPC* a_npc);
		void force_equip_outfit(RE::Actor* a_actor, const RE::TESNPC* a_npc);
	}

	namespace Actor
	{
		void Install();
	}

	/*namespace NPC
	{
		void Install();
	}*/

	namespace Event
	{
		class Manager :
		    public ISingleton<Manager>,
			public RE::BSTEventSink<RE::TESDeathEvent>,
			public RE::BSTEventSink<RE::TESFormDeleteEvent>
		{
		public:
			static void Register();

		protected:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESDeathEvent* a_event, RE::BSTEventSource<RE::TESDeathEvent>*) override;
			RE::BSEventNotifyControl ProcessEvent(const RE::TESFormDeleteEvent* a_event, RE::BSTEventSource<RE::TESFormDeleteEvent>*) override;
		};
	}

	void SetupDistribution();
}
