//
//  main.cpp
//  Moonlight
//
//  Created by XITRIX on 26.05.2021.
//

// Switch include only necessary for demo videos recording
#ifdef __SWITCH__
#include <switch.h>
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

#include "DiscoverManager.hpp"
#include "MoonlightSession.hpp"
#include "SwitchMoonlightSessionDecoderAndRenderProvider.hpp"


#ifdef _WIN32
#include <SDL.h>
#define SDL_MAIN
#endif

#include <SDL_main.h>
#include <main_args.hpp>

using namespace brls::literals; // for _i18n

int main(int argc, char* argv[]) {
#ifdef __SWITCH__
    // Keep the main thread above others so that the program stays responsive
    // when doing software decoding.
    // Note: appletInitializeGamePlayRecording() and appletSetWirelessPriorityMode()
    // are handled by borealis SwitchPlatform — calling them here again would double-init
    // and may return unexpected result codes on HOS 22.x+.
    svcSetThreadPriority(CUR_THREAD_HANDLE, 0x20);
#endif

    // Set log level
    // We recommend to use INFO for real apps
    brls::Logger::setLogLevel(brls::LogLevel::LOG_DEBUG);

    // Init the app and i18n
    if (!brls::Application::init()) {
        brls::Logger::error("Unable to init Borealis application");
        return EXIT_FAILURE;
    }

    MoonlightSession::set_provider(
            new SwitchMoonlightSessionDecoderAndRenderProvider());

    brls::Application::createWindow("title"_i18n);

    auto home = Application::getPlatform()->getHomeDirectory("Moonlight-Switch");
    Settings::instance().set_working_dir(home);
    brls::Logger::info("Working dir, {}", home);

    // Have the application register an action on every activity that will quit
    // when you press BUTTON_START
    brls::Application::setGlobalQuit(false);
    brls::Application::setFPSStatus(false);

    // Register custom views (including tabs, which are views)
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

    // Run the app
    while (brls::Application::mainLoop())
        ;

    GameStreamClient::instance().stop();
    DiscoverManager::instance().pause();

    // Exit
#ifdef __SWITCH__
    // Note: nvExit() omitted — deko3D owns the nvInitialize() refcount.
    // Calling nvExit() here double-decrements it on HOS 22.x causing a panic.
#elif defined(PLATFORM_TVOS)
    exit(0);
#endif
    
    return EXIT_SUCCESS;
}
