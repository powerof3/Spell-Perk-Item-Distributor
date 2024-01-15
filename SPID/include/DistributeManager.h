#pragma once

namespace Distribute
{
	inline RE::BGSKeyword* processed{ nullptr };
	inline RE::BGSKeyword* processedOnLoad{ nullptr };
	inline RE::BGSKeyword* processedOutfit{ nullptr };

	namespace detail
	{
		bool should_process_NPC(RE::TESNPC* a_npc, RE::BGSKeyword* a_keyword = processed);
		void force_equip_outfit(RE::Actor* a_actor, const RE::TESNPC* a_npc);
	}

	namespace Actor
	{
		void Install();
	}

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

	void Setup();
	void DoInitialDistribution();
	void LogResults(std::uint32_t a_actorCount);
}
