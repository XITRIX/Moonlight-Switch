//
//  ingame_overlay.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 29.05.2021.
//

#include "ingame_overlay_view.hpp"
#include <libretro-common/retro_timers.h>

using namespace brls;

bool debug = false;

// MARK: - Ingame Overlay View
IngameOverlay::IngameOverlay(StreamingView* streamView) :
    streamView(streamView)
{
    brls::Application::registerXMLView("LogoutTab", [streamView]() { return new LogoutTab(streamView); });
    brls::Application::registerXMLView("DebugTab", [streamView]() { return new DebugTab(streamView); });
    brls::Application::registerXMLView("KeysTab", [streamView]() { return new KeysTab(streamView); });
    
    this->inflateFromXMLRes("xml/views/ingame_overlay/overlay.xml");
    
    addGestureRecognizer(new TapGestureRecognizer([this](TapGestureStatus status, Sound* sound) {
        if (status.state == GestureState::END)
            this->dismiss();
    }));
    
    applet->addGestureRecognizer(new TapGestureRecognizer([this](TapGestureStatus status, Sound* sound) { }));
}

brls::AppletFrame* IngameOverlay::getAppletFrame()
{
    return applet;
}

// MARK: - Logout Tab
LogoutTab::LogoutTab(StreamingView* streamView) :
    streamView(streamView)
{
    this->inflateFromXMLRes("xml/views/ingame_overlay/logout_tab.xml");
    
    disconnect->setText("main/streaming/disconnect"_i18n);
    disconnect->registerClickAction([this, streamView](View* view) {
        this->dismiss([streamView] {
            streamView->terminate(false);
        });
        return true;
    });

    terminateButton->setText("main/streaming/terminate"_i18n);
    terminateButton->registerClickAction([this, streamView](View* view) {
        this->dismiss([streamView] {
            streamView->terminate(true);
        });
        return true;
    });
}

// MARK: - Debug Tab
DebugTab::DebugTab(StreamingView* streamView) :
    streamView(streamView)
{
    this->inflateFromXMLRes("xml/views/ingame_overlay/debug_tab.xml"); 
    
    onscreenLogButton->init("main/streaming/show_logs"_i18n, Settings::instance().write_log(), [](bool value) {
        Settings::instance().set_write_log(value);
        brls::Application::enableDebuggingView(value);
    });
    
    debugButton->init("main/streaming/debug_info"_i18n, streamView->draw_stats, [streamView](bool value) {
        streamView->draw_stats = value;
    });
}

// MARK: - Keys Tab
KeysTab::KeysTab(StreamingView* streamView) :
    streamView(streamView)
{
    this->inflateFromXMLRes("xml/views/ingame_overlay/keys_tab.xml");
    
    escButton->setText("main/streaming/esc"_i18n);
    escButton->registerClickAction([this, streamView](View* view) {
        async([] {
            sync([]{ LiSendKeyboardEvent(0x1B, KEY_ACTION_DOWN, 0); });
            retro_sleep(500);
            sync([]{ LiSendKeyboardEvent(0x1B, KEY_ACTION_UP, 0); });
        });
        return true;
    });
}
