//
//  ingame_overlay.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 29.05.2021.
//

#include "ingame_overlay_view.hpp"
#include "streaming_input_overlay.hpp"
#include <libretro-common/retro_timers.h>

#include <iomanip>
#include <sstream>

using namespace brls;

bool debug = false;

// MARK: - Ingame Overlay View
IngameOverlay::IngameOverlay(StreamingView* streamView) :
    streamView(streamView)
{
    brls::Application::registerXMLView("LogoutTab", [streamView]() { return new LogoutTab(streamView); });
    brls::Application::registerXMLView("OptionsTab", [streamView]() { return new OptionsTab(streamView); });
    
    this->inflateFromXMLRes("xml/views/ingame_overlay/overlay.xml");
    
    addGestureRecognizer(new TapGestureRecognizer([this](TapGestureStatus status, Sound* sound) {
        if (status.state == GestureState::END)
            this->dismiss();
    }));
    
    applet->addGestureRecognizer(new TapGestureRecognizer([this](TapGestureStatus status, Sound* sound) { }));

    getAppletFrameItem()->title = streamView->getHost().hostname + ": " + streamView->getApp().name;
    updateAppletFrameItem();
}

brls::AppletFrame* IngameOverlay::getAppletFrame()
{
    return applet;
}

IngameOverlay::~IngameOverlay() {
}

// MARK: - Logout Tab
LogoutTab::LogoutTab(StreamingView* streamView) :
    streamView(streamView)
{
    this->inflateFromXMLRes("xml/views/ingame_overlay/logout_tab.xml");
    
    disconnect->setText("streaming/disconnect"_i18n);
    disconnect->registerClickAction([this, streamView](View* view) {
        this->dismiss([streamView] {
            streamView->terminate(false);
        });
        return true;
    });

    terminateButton->setText("streaming/terminate"_i18n);
    terminateButton->registerClickAction([this, streamView](View* view) {
        this->dismiss([streamView] {
            streamView->terminate(true);
        });
        return true;
    });
}

// MARK: - Debug Tab
OptionsTab::OptionsTab(StreamingView* streamView) :
    streamView(streamView)
{
    this->inflateFromXMLRes("xml/views/ingame_overlay/options_tab.xml");
    
    volumeHeader->setSubtitle(std::to_string(Settings::instance().get_volume()) + "%");
    float amplification = Settings::instance().get_volume_amplification() ? 500.0f : 100.0f;
    float progress = Settings::instance().get_volume() / amplification;
    volumeSlider->getProgressEvent()->subscribe([this, amplification](float progress) {
        int volume = progress * amplification;
        Settings::instance().set_volume(volume);
        volumeHeader->setSubtitle(std::to_string(volume) + "%");
    });
    volumeSlider->setProgress(progress);

    float mouseProgress = (Settings::instance().get_mouse_speed_multiplier() / 100.0f);
    mouseSlider->getProgressEvent()->subscribe([this](float value) {
        float multiplier = value * 1.5f + 0.5f;
        std::stringstream stream;
        stream << std::fixed << std::setprecision(1) << multiplier;
        mouseHeader->setSubtitle("x" + stream.str());
        Settings::instance().set_mouse_speed_multiplier(value * 100);
    });
    mouseSlider->setProgress(mouseProgress);
    
    inputOverlayButton->setText("streaming/mouse_input"_i18n);
    inputOverlayButton->registerClickAction([this, streamView](View* view) {
        this->dismiss([this]() {
            StreamingInputOverlay* overlay = new StreamingInputOverlay(this->streamView);
            Application::pushActivity(new Activity(overlay));
        });
        return true;
    });
    
    onscreenLogButton->init("streaming/show_logs"_i18n, Settings::instance().write_log(), [](bool value) {
        Settings::instance().set_write_log(value);
        brls::Application::enableDebuggingView(value);
    });
    
    debugButton->init("streaming/debug_info"_i18n, streamView->draw_stats, [streamView](bool value) {
        streamView->draw_stats = value;
    });
}
