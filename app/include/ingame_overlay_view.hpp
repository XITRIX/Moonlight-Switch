//
//  ingame_overlay.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 29.05.2021.
//

#pragma once

#include "streaming_view.hpp"
#include <borealis.hpp>

// MARK: - Ingame Overlay View
class IngameOverlay : public brls::Box {
  public:
    explicit IngameOverlay(StreamingView* streamView);
    ~IngameOverlay() override;

    brls::AppletFrame* getAppletFrame() override;
    bool isTranslucent() override { return true; }

  private:
    StreamingView* streamView;

    BRLS_BIND(brls::Box, backplate, "backplate");
    BRLS_BIND(brls::AppletFrame, applet, "applet");
};

// MARK: - Logout Tab
class LogoutTab : public brls::Box {
  public:
    explicit LogoutTab(StreamingView* streamView);

  private:
    StreamingView* streamView;

    BRLS_BIND(brls::DetailCell, disconnect, "disconnect");
    BRLS_BIND(brls::DetailCell, terminateButton, "terminate");
};

// MARK: - Debug Tab
class OptionsTab : public brls::Box {
  public:
    explicit OptionsTab(StreamingView* streamView);
    ~OptionsTab() override;

  private:
    StreamingView* streamView;

    BRLS_BIND(brls::DetailCell, inputOverlayButton, "input_overlay");
    BRLS_BIND(brls::SelectorCell, keyboardType, "keyboard_type");
    BRLS_BIND(brls::BooleanCell, touchscreenMouseMode, "touchscreen_mouse_mode");
    BRLS_BIND(brls::BooleanCell, swapStickToDpad, "swap_stick_to_dpad");
    BRLS_BIND(brls::Header, volumeHeader, "volume_header");
    BRLS_BIND(brls::Slider, volumeSlider, "volume_slider");
    BRLS_BIND(brls::Header, mouseHeader, "mouse_speed_header");
    BRLS_BIND(brls::Slider, mouseSlider, "mouse_speed_slider");
    BRLS_BIND(brls::BooleanCell, debugButton, "debug");
    BRLS_BIND(brls::BooleanCell, onscreenLogButton, "onscreen_log");
};
