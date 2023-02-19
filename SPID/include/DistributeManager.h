#pragma once

namespace Distribute
{
	inline bool shouldDistribute{ false };

	inline constexpr std::string_view processedKeywordEDID{ "SPID_Processed" };
	inline RE::BGSKeyword*            processedKeyword{ nullptr };

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

	void LookupFormsOnce();
}
