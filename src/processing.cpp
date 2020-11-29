#include "processing.h"

extern std::unordered_set<RE::TESBoundObject*> equipOnLoadForms;


TESObjectLoadedEventHandler* TESObjectLoadedEventHandler::GetSingleton()
{
	static TESObjectLoadedEventHandler singleton;
	return &singleton;
}

auto TESObjectLoadedEventHandler::ProcessEvent(const RE::TESObjectLoadedEvent* a_event, RE::BSTEventSource<RE::TESObjectLoadedEvent>* a_dispatcher)
	-> EventResult
{
	if (!a_event || !a_event->loaded) {
		return EventResult::kContinue;
	}

	auto form = RE::TESForm::LookupByID(a_event->formID);
	if (!form) {
		return EventResult::kContinue;
	}

	auto actor = form->As<RE::Actor>();
	if (!actor) {
		return EventResult::kContinue;
	}

	auto inventory = actor->GetInventory();
	for (auto& [item, count] : inventory) {
		auto it = std::find(equipOnLoadForms.begin(), equipOnLoadForms.end(), item);
		if (it != equipOnLoadForms.end()) {
			RE::ActorEquipManager::GetSingleton()->EquipObject(actor, *it);
		}
	}
	
	return EventResult::kContinue;
}