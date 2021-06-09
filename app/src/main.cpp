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

#include <stdio.h>
#include <stdlib.h>

#include <borealis.hpp>
#include <string>

#include "hosts_tabs_view.hpp"
#include "add_host_tab.hpp"
#include "main_activity.hpp"
#include "host_tab.hpp"
#include "settings_tab.hpp"

#include "MoonlightSession.hpp"
#include "SwitchMoonlightSessionDecoderAndRenderProvider.hpp"

#include "streaming_view.hpp"
#include "backward.hpp"

using namespace brls::literals; // for _i18n

int main(int argc, char* argv[])
{
    // Enable recording for Twitter memes
#ifdef __SWITCH__
    appletInitializeGamePlayRecording();
#endif

    // Set log level
    // We recommend to use INFO for real apps
    brls::Logger::setLogLevel(brls::LogLevel::DEBUG);

    // Init the app and i18n
    if (!brls::Application::init())
    {
        brls::Logger::error("Unable to init Borealis application");
        return EXIT_FAILURE;
    }
    
    
#ifdef __SWITCH__
    Settings::instance().set_working_dir("sdmc:/switch/moonlight");
#else
    Settings::instance().set_working_dir("moonlight-nx");
#endif
    
    MoonlightSession::set_provider(new SwitchMoonlightSessionDecoderAndRenderProvider());

    brls::Application::createWindow("demo/title"_i18n);

    // Have the application register an action on every activity that will quit when you press BUTTON_START
    brls::Application::setGlobalQuit(false);

    // Register custom views (including tabs, which are views)
    brls::Application::registerXMLView("HostsTabs", HostsTabs::create);
    brls::Application::registerXMLView("HostTab", HostTab::create);
    brls::Application::registerXMLView("ComponentsTab", AddHostTab::create);
    brls::Application::registerXMLView("SettingsTab", SettingsTab::create);

    // Add custom values to the theme
    brls::getLightTheme().addColor("captioned_image/caption", nvgRGB(2, 176, 183));
    brls::getDarkTheme().addColor("captioned_image/caption", nvgRGB(51, 186, 227));

    // Add custom values to the style
    brls::getStyle().addMetric("about/padding_top_bottom", 50);
    brls::getStyle().addMetric("about/padding_sides", 75);
    brls::getStyle().addMetric("about/description_margin", 50);

    // Create and push the main activity to the stack
    brls::Application::pushActivity(new MainActivity());
    
    brls::Application::enableDebuggingView(Settings::instance().write_log());

    // Run the app
    while (brls::Application::mainLoop())
        ;
    
    GameStreamClient::instance().stop();

    // Exit
    return EXIT_SUCCESS;
}
