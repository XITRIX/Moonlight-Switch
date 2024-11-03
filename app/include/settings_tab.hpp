//
//  settings_tab.hpp
//  Moonlight
//
//  Created by XITRIX on 26.05.2021.
//

#pragma once

#include <borealis.hpp>

class SettingsTab : public brls::Box {
  public:
    SettingsTab();
    ~SettingsTab();

    BRLS_BIND(brls::SelectorCell, resolution, "resolution");
    BRLS_BIND(brls::SelectorCell, fps, "fps");
    BRLS_BIND(brls::SelectorCell, codec, "codec");
    BRLS_BIND(brls::BooleanCell, requestHdr, "request_hdr");
    BRLS_BIND(brls::SelectorCell, decoder, "decoder");
    BRLS_BIND(brls::BooleanCell, hwDecoding, "use_hw_decoding");
    BRLS_BIND(brls::Header, header, "header");
    BRLS_BIND(brls::Slider, slider, "slider");
    BRLS_BIND(brls::SelectorCell, audioBackend, "audio_backend");
    BRLS_BIND(brls::BooleanCell, optimal, "optimal");
    BRLS_BIND(brls::BooleanCell, pcAudio, "pcAudio");
    BRLS_BIND(brls::BooleanCell, swapUi, "swap_ui");
    BRLS_BIND(brls::DetailCell, swapGame, "swap_game");
    BRLS_BIND(brls::Header, rumbleForceHeader, "rumble_slider_header");
    BRLS_BIND(brls::Slider, rumbleForceSlider, "rumble_slider");
    BRLS_BIND(brls::BooleanCell, swapStickToDpad, "swap_stick_to_dpad");
    BRLS_BIND(brls::DetailCell, guideKeyButtons, "guide_key_buttons");
    BRLS_BIND(brls::SelectorCell, guideBySystemButton, "guide_by_system_button");
    BRLS_BIND(brls::SelectorCell, overlayTime, "overlay_time");
    BRLS_BIND(brls::DetailCell, overlayButtons, "overlay_buttons");
    BRLS_BIND(brls::SelectorCell, overlayBySystemButton, "overlay_by_system_button");
    BRLS_BIND(brls::SelectorCell, mouseInputTime, "mouse_input_time");
    BRLS_BIND(brls::DetailCell, mouseInputButtons, "mouse_input_buttons");
    BRLS_BIND(brls::SelectorCell, keyboardType, "keyboard_type");
    BRLS_BIND(brls::SelectorCell, keyboardFingers, "keyboard_fingers");
    BRLS_BIND(brls::BooleanCell, volumeAmplification, "volume_amplification");
    BRLS_BIND(brls::BooleanCell, touchscreenMouseMode, "touchscreen_mouse_mode");
    BRLS_BIND(brls::BooleanCell, swapMouseKeys, "swap_mouse_keys");
    BRLS_BIND(brls::BooleanCell, swapMouseScroll, "swap_mouse_scroll");
    BRLS_BIND(brls::Header, mouseSpeedHeader, "mouse_speed_header");
    BRLS_BIND(brls::Slider, mouseSpeedSlider, "mouse_speed_slider");
    BRLS_BIND(brls::BooleanCell, writeLog, "writeLog");

    static brls::View* create();

  private:
    void setupButtonsSelectorCell(brls::DetailCell* cell,
                                  std::vector<brls::ControllerButton> buttons);
    std::string getTextFromButtons(std::vector<brls::ControllerButton> buttons);
    NVGcolor getColorFromButtons(std::vector<brls::ControllerButton> buttons);
};
