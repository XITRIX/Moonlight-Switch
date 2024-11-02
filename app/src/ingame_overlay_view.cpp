//
//  ingame_overlay.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 29.05.2021.
//

#ifdef PLATFORM_SWITCH
#include <borealis/platforms/switch/switch_input.hpp>
#endif

#include "ingame_overlay_view.hpp"
#include "streaming_input_overlay.hpp"
#include "button_selecting_dialog.hpp"
#include <libretro-common/retro_timers.h>

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
        [this](TapGestureStatus status, Sound* sound) {}));

    getAppletFrameItem()->title =
        streamView->getHost().hostname + ": " + streamView->getApp().name;
    updateAppletFrameItem();
}

brls::AppletFrame* IngameOverlay::getAppletFrame() { return applet; }

IngameOverlay::~IngameOverlay() {}

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
    setupButtonsSelectorCell(guideKeyButtons,
                             Settings::instance().guide_key_options().buttons);
    guideKeyButtons->registerClickAction([this](View* view) {
        ButtonSelectingDialog* dialog = ButtonSelectingDialog::create(
            "settings/guide_key_setup_message"_i18n, [this](auto buttons) {
                auto options = Settings::instance().guide_key_options();
                options.buttons = buttons;
                Settings::instance().set_guide_key_options(options);
                setupButtonsSelectorCell(guideKeyButtons, buttons);
            });

        dialog->open();
        return true;
    });

#ifndef PLATFORM_SWITCH
    guideByScreenshot->removeFromSuperView();
#else
    guideByScreenshot->init("settings/guide_by_screenshot"_i18n, Settings::instance().replace_screenshot_with_guide_button(),
                            [](bool value) { 
                                Settings::instance().set_replace_screenshot_with_guide_button(value); 
                                ((SwitchInputManager*) brls::Application::getPlatform()->getInputManager())->setReplaceScreenshotWithGuideButton(Settings::instance().replace_screenshot_with_guide_button());
                            });
#endif

    volumeHeader->setSubtitle(
        std::to_string(Settings::instance().get_volume()) + "%");
    float amplification =
        Settings::instance().get_volume_amplification() ? 500.0f : 100.0f;
    float progress = Settings::instance().get_volume() / amplification;
    volumeSlider->getProgressEvent()->subscribe(
        [this, amplification](float progress) {
            int volume = progress * amplification;
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
        (Settings::instance().get_mouse_speed_multiplier() / 100.0f);
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
            StreamingInputOverlay* overlay =
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
        GET_SETTINGS(keyboardType, COMPACT, 0);
        GET_SETTINGS(keyboardType, FULLSIZED, 1);
        DEFAULT;
    }
    keyboardType->getEvent()->subscribe([](int selected) {
        switch (selected) {
            SET_SETTING(0, set_keyboard_type(COMPACT));
            SET_SETTING(1, set_keyboard_type(FULLSIZED));
            DEFAULT;
        }
    });

    std::vector<std::string> keyboardFingersOptions = {
        "3", "4", "5", "Disabled"};
    keyboardFingers->setText("settings/keyboard_fingers"_i18n);
    keyboardFingers->setData(keyboardFingersOptions);
    switch (Settings::instance().get_keyboard_fingers()) {
        GET_SETTINGS(keyboardFingers, 3, 0);
        GET_SETTINGS(keyboardFingers, 4, 1);
        GET_SETTINGS(keyboardFingers, 5, 2);
        GET_SETTINGS(keyboardFingers, -1, 3);
        DEFAULT;
    }
    keyboardFingers->getEvent()->subscribe([](int selected) {
        switch (selected) {
            SET_SETTING(0, set_keyboard_fingers(3));
            SET_SETTING(1, set_keyboard_fingers(4));
            SET_SETTING(2, set_keyboard_fingers(5));
            SET_SETTING(3, set_keyboard_fingers(-1));
            DEFAULT;
        }
    });

    touchscreenMouseMode->init("settings/touchscreen_mouse_mode"_i18n,
                               Settings::instance().touchscreen_mouse_mode(),
                               [this](bool value) {
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
}

OptionsTab::~OptionsTab() { Settings::instance().save(); }

std::string
OptionsTab::getTextFromButtons(std::vector<ControllerButton> buttons) {
    std::string buttonsText = "";
    if (buttons.size() > 0) {
        for (int i = 0; i < buttons.size(); i++) {
            buttonsText += brls::Hint::getKeyIcon(buttons[i], true);
            if (i < buttons.size() - 1)
                buttonsText += " + ";
        }
    } else {
        buttonsText = "hints/off"_i18n;
    }
    return buttonsText;
}

NVGcolor
OptionsTab::getColorFromButtons(std::vector<brls::ControllerButton> buttons) {
    Theme theme = Application::getTheme();
    return buttons.empty() ? theme["brls/text_disabled"]
                           : theme["brls/list/listItem_value_color"];
}

void OptionsTab::setupButtonsSelectorCell(brls::DetailCell* cell, std::vector<ControllerButton> buttons) {
    cell->setDetailText(getTextFromButtons(buttons));
    cell->setDetailTextColor(getColorFromButtons(buttons));
}
