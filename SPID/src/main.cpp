#include "Distributor.h"

void MessageHandler(SKSE::MessagingInterface::Message* a_message)
{
	if (a_message->type == SKSE::MessagingInterface::kDataLoaded) {
		logger::info("{:*^30}", "LOOKUP");

		Cache::EditorID::GetSingleton()->FillMap();
		
		if (Lookup::GetForms()) {
			Distribute::ApplyToNPCs();
			Distribute::LeveledActor::Install();
			Distribute::DeathItemManager::Register();
		}
	}
}

class DistributionManager : public RE::BSTEventSink<SKSE::ModCallbackEvent>
{
public:
	static DistributionManager* GetSingleton()
	{
		static DistributionManager singleton;
		return &singleton;
	}

protected:
	using EventResult = RE::BSEventNotifyControl;

	EventResult ProcessEvent(const SKSE::ModCallbackEvent* a_event, RE::BSTEventSource<SKSE::ModCallbackEvent>*) override
	{
		if (a_event && a_event->eventName == "KID_KeywordDistributionDone") {
			logger::info("{:*^30}", "LOOKUP");
			logger::info("Starting distribution since KID is done...");
			
			Cache::EditorID::GetSingleton()->FillMap();
			
			if (Lookup::GetForms()) {
				Distribute::ApplyToNPCs();
				Distribute::LeveledActor::Install();
				Distribute::DeathItemManager::Register();

				auto modEvent = SKSE::GetModCallbackEventSource();
				modEvent->RemoveEventSink(GetSingleton());
			}
		}

		return EventResult::kContinue;
	}

private:
	DistributionManager() = default;
	DistributionManager(const DistributionManager&) = delete;
	DistributionManager(DistributionManager&&) = delete;

	~DistributionManager() = default;

	DistributionManager& operator=(const DistributionManager&) = delete;
	DistributionManager& operator=(DistributionManager&&) = delete;
};

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= fmt::format(FMT_STRING("{}.log"), Version::PROJECT);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::info);

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("[%H:%M:%S:%e] %v"s);

	logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);

	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = "powerofthree's Spell Perk Distributor";
	a_info->version = Version::MAJOR;

	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver < SKSE::RUNTIME_1_5_39) {
		logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
		return false;
	}

	return true;
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	logger::info("loaded plugin");

	SKSE::Init(a_skse);

	const auto kidHandle = GetModuleHandle(L"po3_KeywordItemDistributor");
	logger::info("Keyword Item Distributor (KID) detected : {}", kidHandle != nullptr);

	if (INI::Read()) {
		if (kidHandle != nullptr) {
			auto modEvent = SKSE::GetModCallbackEventSource();
			modEvent->AddEventSink(DistributionManager::GetSingleton());
		} else {
			auto messaging = SKSE::GetMessagingInterface();
			messaging->RegisterListener(MessageHandler);
		}
	}

	return true;
}
