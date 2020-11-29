#pragma once

#include "enum.h"


class TESObjectLoadedEventHandler : public RE::BSTEventSink<RE::TESObjectLoadedEvent>
{
public:
	using EventResult = RE::BSEventNotifyControl;

	static TESObjectLoadedEventHandler* GetSingleton();

	virtual EventResult ProcessEvent(const RE::TESObjectLoadedEvent* a_event, RE::BSTEventSource<RE::TESObjectLoadedEvent>* a_dispatcher) override;

private:
	TESObjectLoadedEventHandler() = default;
	TESObjectLoadedEventHandler(const TESObjectLoadedEventHandler&) = delete;
	TESObjectLoadedEventHandler(TESObjectLoadedEventHandler&&) = delete;
	virtual ~TESObjectLoadedEventHandler() = default;

	TESObjectLoadedEventHandler& operator=(const TESObjectLoadedEventHandler&) = delete;
	TESObjectLoadedEventHandler& operator=(TESObjectLoadedEventHandler&&) = delete;
};


class TESInitScriptEventEventHandler : public RE::BSTEventSink<RE::TESInitScriptEvent>
{
public:
	using EventResult = RE::BSEventNotifyControl;

	static TESInitScriptEventEventHandler* GetSingleton();

	virtual EventResult ProcessEvent(const RE::TESInitScriptEvent* a_event, RE::BSTEventSource<RE::TESInitScriptEvent>* a_dispatcher) override;

private:
	TESInitScriptEventEventHandler() = default;
	TESInitScriptEventEventHandler(const TESInitScriptEventEventHandler&) = delete;
	TESInitScriptEventEventHandler(TESInitScriptEventEventHandler&&) = delete;
	virtual ~TESInitScriptEventEventHandler() = default;

	TESInitScriptEventEventHandler& operator=(const TESInitScriptEventEventHandler&) = delete;
	TESInitScriptEventEventHandler& operator=(TESInitScriptEventEventHandler&&) = delete;
};