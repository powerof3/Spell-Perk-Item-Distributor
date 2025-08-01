#include "DeathDistribution.h"
#include "DistributeManager.h"
#include "LookupConfigs.h"
#include "LookupForms.h"
#include "Outfits/OutfitManager.h"
#include "PCLevelMultManager.h"
#ifndef NDEBUG
#	include "Testing/OutfitManagerTests.h"
#	include "Testing/DistributionTests.h"
#	include "Testing/DeathDistributionTests.h"
#	include "Testing/Testing.h"
#endif

bool shouldLookupForms{ false };
bool shouldLogErrors{ false };
bool shouldDistribute{ false };

void MessageHandler(SKSE::MessagingInterface::Message* a_message)
{
	switch (a_message->type) {
	case SKSE::MessagingInterface::kPostLoad:
		{
			LOG_HEADER("DEPENDENCIES");

			const auto tweaks = GetModuleHandle(L"po3_Tweaks");
			logger::info("powerofthree's Tweaks (po3_tweaks) detected : {}", tweaks != nullptr);

			if (std::tie(shouldLookupForms, shouldLogErrors) = Distribution::INI::GetConfigs(); shouldLookupForms) {
				LOG_HEADER("HOOKS");
				Distribute::Actor::Install();
			}
		}
		break;
	case SKSE::MessagingInterface::kPostPostLoad:
		{
			LOG_HEADER("MERGES");
			MergeMapperPluginAPI::GetMergeMapperInterface001();  // Request interface
			if (g_mergeMapperInterface) {                        // Use Interface
				const auto version = g_mergeMapperInterface->GetBuildNumber();
				logger::info("\tGot MergeMapper interface buildnumber {}", version);
			} else {
				logger::info("INFO - MergeMapper not detected");
			}
		}
		break;
	case SKSE::MessagingInterface::kDataLoaded:
		{
			if (shouldDistribute = Lookup::LookupForms(); shouldDistribute) {
				Distribute::Setup();
			}

			if (shouldLogErrors) {
				const auto error = std::format("[SPID] Errors found when reading configs. Check {}.log in {} for more info\n", Version::PROJECT, SKSE::log::log_directory()->string());
				RE::ConsoleLog::GetSingleton()->Print(error.c_str());
			}
		}
		break;
	case SKSE::MessagingInterface::kPreLoadGame:
		{
			if (shouldDistribute) {
				const std::string savePath{ static_cast<char*>(a_message->data), a_message->dataLen };
				PCLevelMult::Manager::GetSingleton()->GetPlayerIDFromSave(savePath);
			}
		}
		break;
	case SKSE::MessagingInterface::kNewGame:
		{
			if (shouldDistribute) {
				PCLevelMult::Manager::GetSingleton()->SetNewGameStarted();
			}
		}
		break;
	default:
		break;
	}
	// The order at which managers handle messages is important,
	// since they need to register for events in specific order to work properly (e.g. Death event must be handled first by Death Manager, and then by Outfit Manager)
	Outfits::Manager::GetSingleton()->HandleMessage(a_message);
	DeathDistribution::Manager::GetSingleton()->HandleMessage(a_message);

	// Run tests after all hooks.
	switch (a_message->type) {
	case SKSE::MessagingInterface::kDataLoaded:
		{
#ifndef NDEBUG
			logger::info("Running tests...");
			Testing::Run();
#endif
		}
		break;
	}
}

#ifdef SKYRIM_AE
extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() {
	SKSE::PluginVersionData v;
	v.PluginVersion(Version::MAJOR);
	v.PluginName("Spell Perk Item Distributor");
	v.AuthorName("powerofthree");
	v.UsesAddressLibrary();
	v.UsesUpdatedStructs();
	v.CompatibleVersions({ SKSE::RUNTIME_SSE_LATEST });

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
	if (ver <
#	ifndef SKYRIMVR
		SKSE::RUNTIME_SSE_1_5_39
#	else
		SKSE::RUNTIME_VR_1_4_15
#	endif
	) {
		logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
		return false;
	}

	return true;
}
#endif

void InitializeLog()
{
	auto path = SKSE::log::log_directory();
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

	logger::info("Game version : {}", a_skse->RuntimeVersion().string());

	SKSE::Init(a_skse, false);

	SKSE::GetMessagingInterface()->RegisterListener(MessageHandler);

	return true;
}
