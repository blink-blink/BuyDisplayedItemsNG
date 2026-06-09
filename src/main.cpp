#include "Hooks.h"
#include "Manager.h"
#include "NPCRegistry.h"
#include "Settings.h"
#include "Strings.h"
#include "UI.h"

void MessageHandler(SKSE::MessagingInterface::Message* a_message)
{
	switch (a_message->type) {
	case SKSE::MessagingInterface::kPostLoad:
		Settings::GetSingleton().Load();
		Strings::Load();
		SKSE::AllocTrampoline(14);
		Hooks::Install();
		break;
	case SKSE::MessagingInterface::kDataLoaded:
		Manager::Register();
		Manager::GetSingleton()->BuildShopFactionCache();
		Manager::GetSingleton()->BuildBuyPriceCache();
		NPCRegistry::GetSingleton().BuildCache();
		UI::Register();
		break;
	default:
		break;
	}
}

void InitializeLog()
{
	auto path = logger::log_directory();
	if (!path) {
		stl::report_and_fail("Failed to find standard logging directory"sv);
	}

	*path /= std::format("{}.log", Version::PROJECT);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
	auto log  = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

	log->set_level(spdlog::level::debug);
	log->flush_on(spdlog::level::debug);

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("[%l] %v"s);

	logger::info("{} v{}", Version::PROJECT, Version::NAME);
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	InitializeLog();

	logger::info("Game version : {}", a_skse->RuntimeVersion().string());

	SKSE::Init(a_skse, false);

	const auto messaging = SKSE::GetMessagingInterface();
	messaging->RegisterListener(MessageHandler);

	return true;
}
