#include <ctime>
#include <cstdio>
#include <mutex>
#ifdef __SWITCH2__
#include "switch2_build_id.hpp"
#endif
//
//  main.cpp
//  Moonlight
//
//  Created by XITRIX on 26.05.2021.
//

// Switch include only necessary for demo videos recording
#ifdef __SWITCH__
#include <switch.h>
#include <sys/stat.h>
#endif

#ifdef __PSV__
extern "C" {
// PVR_PSP2 allocates its EGL/GLES state from the Sony libc heap. Match the
// memory model used by working Borealis Vita applications such as wiliwili.
unsigned int _newlib_heap_size_user      = 220 * 1024 * 1024;
unsigned int _pthread_stack_default_user = 2 * 1024 * 1024;
unsigned int sceLibcHeapSize             = 24 * 1024 * 1024;
}
#endif

#include <cstdlib>

#include <borealis.hpp>
#include <string>

#include "add_host_tab.hpp"
#include "host_tab.hpp"
#include "link_cell.hpp"
#include "main_activity.hpp"
#include "main_tabs_view.hpp"
#include "settings_tab.hpp"
#include "views/boolean_slider_cell.hpp"

#include "DiscoverManager.hpp"
#include "MoonlightSession.hpp"
#include "SwitchMoonlightSessionDecoderAndRenderProvider.hpp"


#if defined(_WIN32) && defined(__SDL2__)
#include <SDL.h>
#define SDL_MAIN
#endif

#if defined(__SDL3__)
#include <SDL3/SDL_main.h>
#elif defined(__SDL2__)
#include <SDL_main.h>
#endif
#include <main_args.hpp>

using namespace brls::literals; // for _i18n

#ifdef __SWITCH__
namespace {

s32 selectAllowedSwitchCore(u64 affinityMask, int ordinal) {
    s32 lastAllowedCore = -1;

    for (s32 core = 0; core < 64; core++)
    {
        if ((affinityMask & (1ULL << core)) == 0) {
            continue;
        }

        lastAllowedCore = core;
        if (ordinal == 0) {
            return core;
        }

        ordinal--;
    }

    return lastAllowedCore;
}

void preferSwitchCore(int ordinal) {
    s32 preferredCore = -1;
    u64 affinityMask = 0;
    if (R_FAILED(svcGetThreadCoreMask(&preferredCore, &affinityMask, CUR_THREAD_HANDLE))) {
        return;
    }

    s32 targetCore = selectAllowedSwitchCore(affinityMask, ordinal);
    if (targetCore >= 0 && targetCore != preferredCore) {
        svcSetThreadCoreMask(CUR_THREAD_HANDLE, targetCore, static_cast<u32>(affinityMask));
    }
}

} // namespace
#endif

int main(int argc, char* argv[]) {


#ifdef __SWITCH2__
    // Keep diagnostics available after a failed launch or streaming session.
    mkdir("sdmc:/switch/Moonlight-Switch", 0777);
    if (FILE* log = fopen("sdmc:/switch/Moonlight-Switch/startup.log", "w"))
    {
        setvbuf(log, nullptr, _IONBF, 0);
        brls::Logger::setLogOutput(log);
        dup2(fileno(log), fileno(stderr));
        setvbuf(stderr, nullptr, _IONBF, 0);
    }
    brls::Logger::info("SWITCH2_BUILD: id={} renderer=native-vulkan", MOONLIGHT_SWITCH2_BUILD_ID);
    brls::Logger::info(
        "Switch 2 startup: libounce, native Vulkan UI/video over OunceVK; H.264/AV1 hardware available; HEVC software");
#endif
    // Enable recording for Twitter memes
#ifdef __SWITCH__
    appletInitializeGamePlayRecording();
    appletSetWirelessPriorityMode(AppletWirelessPriorityMode_OptimizedForWlan);

    // Keep the UI loop away from the hottest streaming worker core.
    preferSwitchCore(0);

    // Keep the main thread above others so that the program stays responsive
    // when doing software decoding
    svcSetThreadPriority(CUR_THREAD_HANDLE, 0x20);

    // auto at = appletGetAppletType();
    // g_application_mode = at == AppletType_Application || at == AppletType_SystemApplication;

    // // To get access to /dev/nvhost-nvjpg, we need nvdrv:{a,s,t}
    // // However, nvdrv:{a,s} have limited address space for gpu mappings
    // extern u32 __nx_nv_service_type, __nx_nv_transfermem_size;
    // __nx_nv_service_type     = NvServiceType_Factory;
    // __nx_nv_transfermem_size = (g_application_mode ? 16 : 3) * 0x100000;
#endif

    // Set log level
    // We recommend to use INFO for real apps
    brls::Logger::setLogLevel(brls::LogLevel::LOG_WARNING);

    // Init the app and i18n
    if (!brls::Application::init()) {
        brls::Logger::error("Unable to init Borealis application");
        return EXIT_FAILURE;
    }

    registerDeepLinkHandler();

#if defined(PLATFORM_VISIONOS)
    brls::Application::setMaximumUIScale(1.0f);
#endif

    MoonlightSession::set_provider(
            new SwitchMoonlightSessionDecoderAndRenderProvider());

    brls::Application::createWindow("title"_i18n);

    auto home = Application::getPlatform()->getHomeDirectory("Moonlight-Switch");
    Settings::instance().set_working_dir(home);
    Settings::instance().set_launch_path(argc > 0 ? argv[0] : "");
#if defined(PLATFORM_SWITCH) && !defined(__SWITCH2__)
    if (Settings::instance().save_diagnostic_log()) {
    // Use the NRO's directory, even when its folder differs from settings home.
    std::string traceDir = argc > 0 ? argv[0] : "";
    const auto slash = traceDir.find_last_of("/\\");
    traceDir = slash == std::string::npos ? "sdmc:/switch/Moonlight-Switch/" : traceDir.substr(0, slash + 1);
    const std::string tracePath = traceDir + "moonlight-LL27-log.txt";
    const std::string previousTracePath = traceDir + "moonlight-LL27-log-previous.txt";
    std::remove(previousTracePath.c_str());
    std::rename(tracePath.c_str(), previousTracePath.c_str());
    if (FILE* traceFile = std::fopen(tracePath.c_str(), "w")) {
        std::setvbuf(traceFile, nullptr, _IOFBF, 64 * 1024);
        std::fprintf(traceFile, "Moonlight LL27 basic log; timestamps=monotonic microseconds\n");
        std::fprintf(traceFile, "Executable: %s\nLog path: %s\nBuild: %s %s\nStarted epoch: %lld\n", argc > 0 ? argv[0] : "(unavailable)", tracePath.c_str(), __DATE__, __TIME__, static_cast<long long>(std::time(nullptr)));
        std::fflush(traceFile);
        brls::Logger::getLogEvent()->subscribe([traceFile](auto, brls::LogLevel level, const std::string& line) {
            static std::mutex traceMutex;
            std::lock_guard<std::mutex> lock(traceMutex);
            static uint64_t lastFlush = 0;
            const uint64_t now = LiGetMicroseconds();
            std::fprintf(traceFile, "%llu [%d] %s\n", (unsigned long long)now, (int)level, line.c_str());
            if (now < 3000000 || now - lastFlush >= 250000) {
                std::fflush(traceFile);
                lastFlush = now;
            }
        });
    } else {
        brls::Logger::error("LL27: could not create diagnostic file: {}", tracePath);
    }
    }
    brls::Logger::setLogLevel(Settings::instance().save_diagnostic_log() ? brls::LogLevel::LOG_INFO : brls::LogLevel::LOG_WARNING);
#endif

    brls::Logger::info("Working dir, {}", home);
#if defined(PLATFORM_SWITCH) && !defined(__SWITCH2__)
    brls::Application::setSwapInterval(Settings::instance().video_vsync() ? 1 : 0);
    brls::Logger::info("LL27: presentation buffers=2 swap_interval={}", Settings::instance().video_vsync() ? 1 : 0);
#endif

    // Have the application register an action on every activity that will quit
    // when you press BUTTON_START
    brls::Application::setGlobalQuit(false);
    brls::Application::setFPSStatus(false);

    // Register custom views (including tabs, which are views)
    brls::Application::registerXMLView("BooleanSliderCell", BooleanSliderCell::create);
    brls::Application::registerXMLView("LinkCell", LinkCell::create);

    brls::Application::registerXMLView("MainTabs", MainTabs::create);
    brls::Application::registerXMLView("HostTab", HostTab::create);
    brls::Application::registerXMLView("AddHostTab", AddHostTab::create);
    brls::Application::registerXMLView("SettingsTab", SettingsTab::create);

    // Add custom values to the theme
    brls::Theme::getLightTheme().addColor("captioned_image/caption",
                                   nvgRGB(2, 176, 183));
    brls::Theme::getDarkTheme().addColor("captioned_image/caption",
                                  nvgRGB(51, 186, 227));

    // Add custom values to the style
    brls::getStyle().addMetric("about/padding_top_bottom", 50);
    brls::getStyle().addMetric("about/padding_sides", 75);
    brls::getStyle().addMetric("about/description_margin", 50);

    // Create and push the main activity to the stack if cannot run game from arguments
    if (!startFromArgs(argc, argv)) {
        brls::Application::pushActivity(new MainActivity());
    }

    brls::Application::enableDebuggingView(Settings::instance().write_log());
    brls::Application::setSwapInputKeys(Settings::instance().swap_ui_keys());

    // Run the app. The Vita development loop waits for this marker so a
    // successful launch means at least one complete Borealis frame rendered.
#ifdef __PSV__
    bool vitaHealthReported = false;
#endif
    while (brls::Application::mainLoop()) {
#ifdef __SWITCH2__
        static bool switch2Ready = false;
        if (!switch2Ready)
        {
            brls::Logger::info("SWITCH2_HEALTH: first UI frame completed");
            switch2Ready = true;
        }
#endif
#ifdef __PSV__
        if (!vitaHealthReported) {
            brls::Logger::info("VITA_HEALTH: READY");
            vitaHealthReported = true;
        }
#endif
    }

    // Exit
#if defined(PLATFORM_TVOS)
    exit(0);
#endif
    
    return EXIT_SUCCESS;
}
