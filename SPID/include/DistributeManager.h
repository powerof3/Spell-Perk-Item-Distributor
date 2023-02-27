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
	}

	namespace Actor
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

	void SetupDistribution();
}
