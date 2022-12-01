#include "DistributeManager.h"
#include "Distribute.h"
#include "DistributePCLevelMult.h"
#include "LookupForms.h"

namespace Distribute
{
	bool detail::uses_template(const RE::TESNPC* a_npc)
	{
		return a_npc->UsesTemplate() || a_npc->baseTemplateForm != nullptr;
	}

	// Static actors
	namespace Actor
	{
		// Refires when quitting to main menu and loading the game again
		struct InitItemImpl
		{
			static void thunk(RE::TESNPC* a_this)
			{
				func(a_this);

				std::call_once(lookupForms, [] {
					logger::info("{:*^50}", "LOOKUP");

					const auto startTime = std::chrono::steady_clock::now();
					shouldDistribute = Lookup::GetForms();
					const auto endTime = std::chrono::steady_clock::now();

					if (shouldDistribute) {
						Lookup::LogFormLookup();

					    const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
						logger::info("Lookup took {}μs / {}ms", duration, duration / 1000.0f);

						if (Forms::GetTotalLeveledEntries() > 0) {
							logger::info("{:*^50}", "HOOKS");
							PlayerLeveledActor::Install();
						}
					}
				});

				if (!a_this->IsPlayer() && (!detail::uses_template(a_this) || a_this->IsUnique())) {
					const auto startTime = std::chrono::steady_clock::now();

					const auto npcData = std::make_unique<NPCData>(a_this);
					Distribute(*npcData, PCLevelMult::Input{ a_this, false, true });

					const auto endTime = std::chrono::steady_clock::now();

					if (!loggedStats) {
						timeTaken += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
						++totalNPCs;
					}
				}
			}
			static inline REL::Relocation<decltype(thunk)> func;

			static inline size_t index{ 0 };
			static inline constexpr std::size_t size{ 0x13 };
		};

		void Install()
		{
			stl::write_vfunc<RE::TESNPC, InitItemImpl>();
			logger::info("\tHooked actor init");
		}
	}

	// Leveled actors
	namespace LeveledActor
	{
		struct SetObjectReference
		{
			static void thunk(RE::Character* a_this, RE::TESNPC* a_npc)
			{
				func(a_this, a_npc);

				if (a_npc && detail::uses_template(a_npc) && !a_npc->IsUnique()) {
					const auto npcData = std::make_unique<NPCData>(a_this, a_npc);
					Distribute(*npcData, PCLevelMult::Input{ a_this, a_npc, false, false });
				}
			}
			static inline REL::Relocation<decltype(thunk)> func;

			static inline size_t index{ 0 };
			static inline size_t size{ 0x84 };
		};

		void Install()
		{
			stl::write_vfunc<RE::Character, SetObjectReference>();
			logger::info("\tHooked leveled actor init");
		}
	}

	void LogStats()
	{
		if (!loggedStats) {
			loggedStats = true;

			logger::info("{:*^50}", "STATS");
			auto avgTimeTaken = Actor::timeTaken / Actor::totalNPCs;
			logger::info("{} entries", Forms::GetTotalEntries());
			logger::info("{} static NPCs processed", Actor::totalNPCs);
			logger::info("Total distribution time : {}μs / {}ms", Actor::timeTaken, Actor::timeTaken / 1000.0f);
			logger::info("Average distribution time per NPC : {}μs / {}ms", avgTimeTaken, avgTimeTaken / 1000.0f);
		}
	}
}

namespace Distribute::Event
{
	struct detail  //AddObjectToContainer doesn't work with leveled items :s
	{
		static void add_item(RE::Actor* a_actor, RE::TESBoundObject* a_item, std::uint32_t a_itemCount, bool a_silent, std::uint32_t a_stackID, RE::BSScript::Internal::VirtualMachine* a_vm)
		{
			using func_t = decltype(&detail::add_item);
			REL::Relocation<func_t> func{ RELOCATION_ID(55945, 56489) };
			return func(a_actor, a_item, a_itemCount, a_silent, a_stackID, a_vm);
		}
	};

	void Manager::Register()
	{
		if (const auto scripts = RE::ScriptEventSourceHolder::GetSingleton()) {
			scripts->AddEventSink<RE::TESFormDeleteEvent>(GetSingleton());
			logger::info("\tRegistered for {}", typeid(RE::TESFormDeleteEvent).name());
			if (Forms::deathItems) {
				scripts->AddEventSink<RE::TESDeathEvent>(GetSingleton());
				logger::info("\tRegistered for {}", typeid(RE::TESDeathEvent).name());
			}
		}
	}

	RE::BSEventNotifyControl Manager::ProcessEvent(const RE::TESDeathEvent* a_event, RE::BSTEventSource<RE::TESDeathEvent>*)
	{
		constexpr auto is_NPC = [](auto&& a_ref) {
			return a_ref && !a_ref->IsPlayerRef();
		};

		if (a_event && a_event->dead && is_NPC(a_event->actorDying)) {
			const auto actor = a_event->actorDying->As<RE::Actor>();
			const auto npc = actor ? actor->GetActorBase() : nullptr;
			if (actor && npc) {
				const auto input = PCLevelMult::Input{ actor, npc, false, false };
				const auto npcData = std::make_unique<NPCData>(actor, npc);
				for_each_form<RE::TESBoundObject>(*npcData, Forms::deathItems, input, [&](auto* a_deathItem, IdxOrCount a_count) {
					detail::add_item(actor, a_deathItem, a_count, true, 0, RE::BSScript::Internal::VirtualMachine::GetSingleton());
					return true;
				});
			}
		}

		return RE::BSEventNotifyControl::kContinue;
	}

	RE::BSEventNotifyControl Manager::ProcessEvent(const RE::TESFormDeleteEvent* a_event, RE::BSTEventSource<RE::TESFormDeleteEvent>*)
	{
		if (a_event && a_event->formID != 0) {
			PCLevelMult::Manager::GetSingleton()->DeleteNPC(a_event->formID);
		}
		return RE::BSEventNotifyControl::kContinue;
	}
}
