//
//  ingame_overlay.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 29.05.2021.
//

#ifdef PLATFORM_SWITCH
#include <borealis/platforms/switch/switch_input.hpp>
#endif

#include "helper.hpp"
#include "ingame_overlay_view.hpp"
#include "streaming_input_overlay.hpp"
#include "key_combo_settings.hpp"
#include "UpscalingSupport.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>

#define SET_SETTING(n, func)                                                   \
    case n:                                                                    \
        Settings::instance().func;                                             \
        break;

#define GET_SETTINGS(combo_box, n, i)                                          \
    case n:                                                                    \
        combo_box->setSelection(i);                                            \
        break;

#define DEFAULT                                                                \
    default:                                                                   \
        break;

using namespace brls;

namespace {
void updateStrengthControl(BooleanSliderCell* cell,
                           bool enabled, const std::string& detailText) {
    cell->setSliderVisibility(enabled ? brls::Visibility::VISIBLE
                                      : brls::Visibility::GONE);
    cell->setValueText(enabled ? detailText : "");
}

float sliderProgressToDitheringStrength(float progress) {
    return static_cast<float>(int(progress * 9.0f + 0.5f) + 1);
}

float ditheringStrengthToSliderProgress(float strength) {
    if (strength < 1.0f)
        strength = 1.0f;
    else if (strength > 10.0f)
        strength = 10.0f;

    return (strength - 1.0f) / 9.0f;
}

std::string getDitheringStrengthText(float strength) {
    return std::to_string(int(strength));
}

std::string getRcasStrengthText(float strength) {
    return std::to_string(int(strength * 100.0f)) + "%";
}
}

bool debug = false;

// MARK: - Ingame Overlay View
IngameOverlay::IngameOverlay(StreamingView* streamView)
    : streamView(streamView) {
    brls::Application::registerXMLView(
        "LogoutTab", [streamView]() { return new LogoutTab(streamView); });
    brls::Application::registerXMLView(
        "OptionsTab", [streamView]() { return new OptionsTab(streamView); });

    this->inflateFromXMLRes("xml/views/ingame_overlay/overlay.xml");

    addGestureRecognizer(
        new TapGestureRecognizer([this](TapGestureStatus status, Sound* sound) {
            if (status.state == GestureState::END)
                this->dismiss();
        }));

    applet->addGestureRecognizer(new TapGestureRecognizer(
        [](TapGestureStatus status, Sound* sound) {}));

    getAppletFrameItem()->title =
        streamView->getHost().hostname + ": " + streamView->getApp().name;
    updateAppletFrameItem();
}

brls::AppletFrame* IngameOverlay::getAppletFrame() { return applet; }

// MARK: - Logout Tab
LogoutTab::LogoutTab(StreamingView* streamView) : streamView(streamView) {
    this->inflateFromXMLRes("xml/views/ingame_overlay/logout_tab.xml");

    disconnect->setText("streaming/disconnect"_i18n);
    disconnect->registerClickAction([this, streamView](View* view) {
        this->dismiss([streamView] { streamView->terminate(false); });
        return true;
    });

    terminateButton->setText("streaming/terminate"_i18n);
    terminateButton->registerClickAction([this, streamView](View* view) {
        this->dismiss([streamView] { streamView->terminate(true); });
        return true;
    });
}

// MARK: - Options Tab
OptionsTab::OptionsTab(StreamingView* streamView) : streamView(streamView) {
    this->inflateFromXMLRes("xml/views/ingame_overlay/options_tab.xml");

    guideKeyButtons->setText("settings/guide_key_buttons"_i18n);
    setupKeyComboCell(guideKeyButtons,
                      Settings::instance().guide_key_options().buttons);
    guideKeyButtons->registerClickAction([this](View* view) {
        openKeyComboDialog(KeyComboAction::GUIDE, guideKeyButtons);
        return true;
    });

#ifndef PLATFORM_SWITCH
    guideBySystemButton->removeFromSuperView();
#else
    guideBySystemButton->init(
        "settings/use_system_button"_i18n,
        {"hints/off"_i18n, "settings/buttons/screenshot"_i18n, "settings/buttons/home"_i18n},
        (int) Settings::instance().get_guide_system_button(), [this](int value) {
            if (value != 0 && Settings::instance().get_overlay_system_button() == (ButtonOverrideType) value) {
                brls::sync([this, value](){
                    showError("settings/system_button_duplication_error"_i18n, [](){});
                });
                guideBySystemButton->setSelection((int) Settings::instance().get_guide_system_button(), true);
                return;
            }

            Settings::instance().set_guide_system_button((ButtonOverrideType) value);

            auto color = Settings::instance().get_guide_system_button() == ButtonOverrideType::NONE ?
                Application::getTheme()["brls/text_disabled"] : Application::getTheme()["brls/accent"];
            guideBySystemButton->setDetailTextColor(color);
        });
    auto color = Settings::instance().get_guide_system_button() == ButtonOverrideType::NONE ?
         Application::getTheme()["brls/text_disabled"] : Application::getTheme()["brls/accent"];
    guideBySystemButton->setDetailTextColor(color);
#endif

    hostCloseAppButtons->setText("settings/host_close_app"_i18n);
    setupKeyComboCell(
        hostCloseAppButtons,
        Settings::instance()
            .key_combo_options(KeyComboAction::HOST_CLOSE_APP)
            .buttons);
    hostCloseAppButtons->registerClickAction([this](View* view) {
        openKeyComboDialog(KeyComboAction::HOST_CLOSE_APP,
                           hostCloseAppButtons);
        return true;
    });

    hostSwitchWindowButtons->setText("settings/host_switch_window"_i18n);
    setupKeyComboCell(
        hostSwitchWindowButtons,
        Settings::instance()
            .key_combo_options(KeyComboAction::HOST_SWITCH_WINDOW)
            .buttons);
    hostSwitchWindowButtons->registerClickAction([this](View* view) {
        openKeyComboDialog(KeyComboAction::HOST_SWITCH_WINDOW,
                           hostSwitchWindowButtons);
        return true;
    });

    volumeHeader->setSubtitle(
        std::to_string(Settings::instance().get_volume()) + "%");
    float amplification =
        Settings::instance().get_volume_amplification() ? 500.0f : 100.0f;
    float progress = (float) Settings::instance().get_volume() / amplification;
    volumeSlider->getProgressEvent()->subscribe(
        [this, amplification](float progress) {
            int volume = int(progress * amplification);
            Settings::instance().set_volume(volume);
            volumeHeader->setSubtitle(std::to_string(volume) + "%");
        });
    volumeSlider->setProgress(progress);

    float rumbleForceProgress = Settings::instance().get_rumble_force();
    rumbleForceSlider->getProgressEvent()->subscribe([this](float value) {
        std::stringstream stream;
        stream << std::fixed << std::setprecision(1) << int(value * 100);
        rumbleForceHeader->setSubtitle(stream.str() + "%");
        Settings::instance().set_rumble_force(value);
    });
    rumbleForceSlider->setProgress(rumbleForceProgress);

    float mouseProgress =
        ((float) Settings::instance().get_mouse_speed_multiplier() / 100.0f);
    mouseSlider->getProgressEvent()->subscribe([this](float value) {
        float multiplier = value * 1.5f + 0.5f;
        std::stringstream stream;
        stream << std::fixed << std::setprecision(1) << multiplier;
        mouseHeader->setSubtitle("x" + stream.str());
        Settings::instance().set_mouse_speed_multiplier(int(value * 100));
    });
    mouseSlider->setProgress(mouseProgress);

    inputOverlayButton->setText("streaming/mouse_input"_i18n);
    inputOverlayButton->registerClickAction([this](View* view) {
        this->dismiss([this]() {
            auto* overlay =
                new StreamingInputOverlay(this->streamView);
            Application::pushActivity(new Activity(overlay));
        });
        return true;
    });

    std::vector<std::string> keyboardTypes = {
        "settings/keyboard_compact"_i18n, "settings/keyboard_fullsized"_i18n};
    keyboardType->setText("settings/keyboard_type"_i18n);
    keyboardType->setData(keyboardTypes);
    switch (Settings::instance().get_keyboard_type()) {
        GET_SETTINGS(keyboardType, COMPACT, 0)
        GET_SETTINGS(keyboardType, FULLSIZED, 1)
        DEFAULT
    }
    keyboardType->getEvent()->subscribe([](int selected) {
        switch (selected) {
            SET_SETTING(0, set_keyboard_type(COMPACT))
            SET_SETTING(1, set_keyboard_type(FULLSIZED))
            DEFAULT
        }
    });

    std::vector<std::string> keyboardFingersOptions = {
        "3", "4", "5", "hints/off"_i18n};
    keyboardFingers->setText("settings/keyboard_fingers"_i18n);
    keyboardFingers->setData(keyboardFingersOptions);
    switch (Settings::instance().get_keyboard_fingers()) {
        GET_SETTINGS(keyboardFingers, 3, 0)
        GET_SETTINGS(keyboardFingers, 4, 1)
        GET_SETTINGS(keyboardFingers, 5, 2)
        GET_SETTINGS(keyboardFingers, -1, 3)
        DEFAULT
    }
    keyboardFingers->getEvent()->subscribe([](int selected) {
        switch (selected) {
            SET_SETTING(0, set_keyboard_fingers(3))
            SET_SETTING(1, set_keyboard_fingers(4))
            SET_SETTING(2, set_keyboard_fingers(5))
            SET_SETTING(3, set_keyboard_fingers(-1))
            DEFAULT
        }
    });

    touchscreenMouseMode->init("settings/touchscreen_mouse_mode"_i18n,
                               Settings::instance().touchscreen_mouse_mode(),
                               [](bool value) {
                                   Settings::instance().set_touchscreen_mouse_mode(value);
                               });
    
    swapStickToDpad->init("settings/swap_stick_to_dpad"_i18n, Settings::instance().swap_joycon_stick_to_dpad(),
                          [](bool value) { Settings::instance().set_swap_joycon_stick_to_dpad(value); });

    onscreenLogButton->init("streaming/show_logs"_i18n,
                            Settings::instance().write_log(), [](bool value) {
                                Settings::instance().set_write_log(value);
                                brls::Application::enableDebuggingView(value);
                            });

    debugButton->init(
        "streaming/debug_info"_i18n, streamView->draw_stats,
        [streamView](bool value) { streamView->draw_stats = value; });

#ifdef SUPPORT_UPSCALING
    if (!isVideoUpscalingSupported()) {
        imageAdjustmentsHeader->removeFromSuperView(true);
        ditheringButton->removeFromSuperView(true);
        upscalingButton->removeFromSuperView(true);
        upscalingModeButton->removeFromSuperView(true);
        rcasButton->removeFromSuperView(true);
    } else {
        auto updateDitheringControls = [this](bool enabled) {
            updateStrengthControl(ditheringButton, enabled,
                                  getDitheringStrengthText(
                                      Settings::instance().dithering_strength()));
        };
        auto updateRcasControls = [this](bool enabled) {
            updateStrengthControl(rcasButton, enabled,
                                  getRcasStrengthText(
                                      Settings::instance().rcas_strength()));
        };

        ditheringButton->init(
            "settings/dithering"_i18n, Settings::instance().dithering(),
            [updateDitheringControls](bool value) {
                Settings::instance().set_dithering(value);
                updateDitheringControls(value);
            });

        const float ditheringStrength = Settings::instance().dithering_strength();
        ditheringButton->getProgressEvent()->subscribe([this](float value) {
            const float strength = sliderProgressToDitheringStrength(value);
            Settings::instance().set_dithering_strength(strength);
            updateStrengthControl(ditheringButton,
                                  Settings::instance().dithering(),
                                  getDitheringStrengthText(strength));
        });
        ditheringButton->setProgress(
            ditheringStrengthToSliderProgress(ditheringStrength));
        updateDitheringControls(Settings::instance().dithering());

#if defined(PLATFORM_APPLE) && !defined(PLATFORM_TVOS)
        upscalingButton->removeFromSuperView(true);
        upscalingModeButton->init(
            "settings/upscaling"_i18n,
            {"hints/off"_i18n, "MetalFX", "FSR1"},
            (int)Settings::instance().upscaling_mode(),
            [](int value) { Settings::instance().set_upscaling_mode((UpscalingMode)value); });
#else
        upscalingModeButton->removeFromSuperView(true);
        upscalingButton->init(
            "settings/upscaling"_i18n, Settings::instance().upscaling(),
            [](bool value) { Settings::instance().set_upscaling(value); });
#endif
        rcasButton->init(
            "settings/rcas_sharpening"_i18n, Settings::instance().rcas(),
            [updateRcasControls](bool value) {
                Settings::instance().set_rcas(value);
                updateRcasControls(value);
            });

        const float rcasStrength = Settings::instance().rcas_strength();
        rcasButton->getProgressEvent()->subscribe([this](float value) {
            Settings::instance().set_rcas_strength(value);
            updateStrengthControl(rcasButton,
                                  Settings::instance().rcas(),
                                  getRcasStrengthText(value));
        });
        rcasButton->setProgress(rcasStrength);
        updateRcasControls(Settings::instance().rcas());
    }
#else
    imageAdjustmentsHeader->removeFromSuperView(true);
    ditheringButton->removeFromSuperView(true);
    upscalingButton->removeFromSuperView(true);
    upscalingModeButton->removeFromSuperView(true);
    rcasButton->removeFromSuperView(true);
#endif
}

OptionsTab::~OptionsTab() { Settings::instance().save(); }
