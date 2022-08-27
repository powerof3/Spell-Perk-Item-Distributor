#include "Distribute.h"
#include "LookupConfigs.h"
#include "LookupForms.h"

HMODULE kid{ nullptr };
HMODULE tweaks{ nullptr };

bool shouldDistribute{ false };

bool DoDistribute()
{
	if (Lookup::GetForms()) {
		Distribute::ApplyToNPCs();
		Distribute::LeveledActor::Install();
		Distribute::PlayerLeveledActor::Install();
		Distribute::DeathItem::Manager::Register();

		return true;
	}

	return false;
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

			DoDistribute();
			SKSE::GetModCallbackEventSource()->RemoveEventSink(GetSingleton());
		}

		return EventResult::kContinue;
	}

private:
	DistributionManager() = default;
	DistributionManager(const DistributionManager&) = delete;
	DistributionManager(DistributionManager&&) = delete;

	~DistributionManager() override = default;

	DistributionManager& operator=(const DistributionManager&) = delete;
	DistributionManager& operator=(DistributionManager&&) = delete;
};

void MessageHandler(SKSE::MessagingInterface::Message* a_message)
{
	switch (a_message->type) {
	case SKSE::MessagingInterface::kPostLoad:
		{
			kid = GetModuleHandle(L"po3_KeywordItemDistributor");
			logger::info("Keyword Item Distributor (KID) detected : {}", kid != nullptr);

			tweaks = GetModuleHandle(L"po3_Tweaks");
			logger::info("powerofthree's Tweaks (po3_tweaks) detected : {}", tweaks != nullptr);

			if (shouldDistribute = INI::Read(); shouldDistribute && kid != nullptr) {
				SKSE::GetModCallbackEventSource()->AddEventSink(DistributionManager::GetSingleton());
			}
		}
		break;
	case SKSE::MessagingInterface::kDataLoaded:
		{
			if (shouldDistribute && kid == nullptr) {
				logger::info("{:*^30}", "LOOKUP");
				DoDistribute();
			}
		}
		break;
	default:
		break;
	}
}

#ifdef SKYRIM_AE
extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() {
	SKSE::PluginVersionData v;
	v.PluginVersion(Version::MAJOR);
	v.PluginName("Spell Perk Item Distributor");
	v.AuthorName("powerofthree");
	v.UsesAddressLibrary(true);
	v.CompatibleVersions({ SKSE::RUNTIME_LATEST });

	return v;
}();
#else
extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT.data();
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
#endif

void InitializeLog()
{
	auto path = logger::log_directory();
	if (!path) {
		stl::report_and_fail("Failed to find standard logging directory"sv);
	}

	*path /= Version::PROJECT;
	*path += ".log"sv;
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::info);

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("[%H:%M:%S:%e] %v"s);

	logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	InitializeLog();

	SKSE::Init(a_skse);

	SKSE::GetMessagingInterface()->RegisterListener(MessageHandler);

	return true;
}
