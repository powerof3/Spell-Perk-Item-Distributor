#pragma once

namespace Distribute
{
	inline bool shouldDistribute{ false };
	inline bool loggedStats{ false };

    namespace detail
	{
		bool uses_template(const RE::TESNPC* a_npc);
	}

	namespace Actor
	{
	    inline std::once_flag lookupForms;

		inline std::uint64_t timeTaken{ 0 };
		inline std::uint32_t totalNPCs{ 0 };

		void Install();
	}

	namespace LeveledActor
	{
		void Install();
	}

	namespace Event
	{
		class Manager :
			public RE::BSTEventSink<RE::TESDeathEvent>,
			public RE::BSTEventSink<RE::TESFormDeleteEvent>
		{
		public:
			static Manager* GetSingleton()
			{
				static Manager singleton;
				return &singleton;
			}

			static void Register();

		protected:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESDeathEvent* a_event, RE::BSTEventSource<RE::TESDeathEvent>*) override;
			RE::BSEventNotifyControl ProcessEvent(const RE::TESFormDeleteEvent* a_event, RE::BSTEventSource<RE::TESFormDeleteEvent>*) override;

		private:
			Manager() = default;
			Manager(const Manager&) = delete;
			Manager(Manager&&) = delete;

			~Manager() override = default;

			Manager& operator=(const Manager&) = delete;
			Manager& operator=(Manager&&) = delete;
		};
	}

	// Distribute to all unique and static NPCs, after data load
	void OnInit();

	void LogStats();
}
