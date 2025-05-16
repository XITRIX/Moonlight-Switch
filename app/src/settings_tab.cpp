//
//  settings_tab.cpp
//  Moonlight
//
//  Created by XITRIX on 26.05.2021.
//

#ifdef PLATFORM_SWITCH
#include <borealis/platforms/switch/switch_input.hpp>
#endif

#include "settings_tab.hpp"
#include "Settings.hpp"
#include "helper.hpp"
#include "button_selecting_dialog.hpp"
#include "mapping_layout_editor.hpp"
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

using namespace brls::literals;

std::vector<std::string> audio_backends {
    "SDL2",
#ifdef __SWITCH__
    "Audren",
#endif
};


SettingsTab::SettingsTab() {
    // Inflate the tab from the XML file
    this->inflateFromXMLRes("xml/tabs/settings.xml");

    std::vector<std::string> resolutions = {
        "Native", "360p", "480p", "720p", "1080p", 
// #if !defined(PLATFORM_SWITCH)
        "1440p"
// #endif
    };
    resolution->setText("settings/resolution"_i18n);
    resolution->setData(resolutions);
    switch (Settings::instance().resolution()) {
        GET_SETTINGS(resolution, -1, 0);
        GET_SETTINGS(resolution, 360, 1);
        GET_SETTINGS(resolution, 480, 2);
        GET_SETTINGS(resolution, 720, 3);
        GET_SETTINGS(resolution, 1080, 4);
        GET_SETTINGS(resolution, 1440, 5);
        DEFAULT;
    }
    resolution->getEvent()->subscribe([](int selected) {
        switch (selected) {
            SET_SETTING(0, set_resolution(-1));
            SET_SETTING(1, set_resolution(360));
            SET_SETTING(2, set_resolution(480));
            SET_SETTING(3, set_resolution(720));
            SET_SETTING(4, set_resolution(1080));
            SET_SETTING(5, set_resolution(1440));
            DEFAULT;
        }
    });

    std::vector<std::string> fpss = {
        "30", 
        "40", 
        "60", 
#if !defined(PLATFORM_SWITCH)
        "120",
#endif
        };
    fps->setText("settings/fps"_i18n);
    fps->setData(fpss);
    int i = 0;
    switch (Settings::instance().fps()) {
        GET_SETTINGS(fps, 30, 0);
        GET_SETTINGS(fps, 40, 1);
        GET_SETTINGS(fps, 60, 2);
        GET_SETTINGS(fps, 120, 3);
        DEFAULT;
    }
    fps->getEvent()->subscribe([](int selected) {
        switch (selected) {
            SET_SETTING(0, set_fps(30));
            SET_SETTING(1, set_fps(40));
            SET_SETTING(2, set_fps(60));
            SET_SETTING(3, set_fps(120));
            DEFAULT;
        }
    });

    std::vector<std::string> decoders = {"settings/zero_threads"_i18n, "2", "3",
                                         "4"};
    decoder->setText("settings/decoder_threads"_i18n);
    decoder->setData(decoders);
    switch (Settings::instance().decoder_threads()) {
        GET_SETTINGS(decoder, 0, 0);
        GET_SETTINGS(decoder, 2, 1);
        GET_SETTINGS(decoder, 3, 2);
        GET_SETTINGS(decoder, 4, 3);
        DEFAULT;
    }
    decoder->getEvent()->subscribe([](int selected) {
        switch (selected) {
            SET_SETTING(0, set_decoder_threads(0));
            SET_SETTING(1, set_decoder_threads(2));
            SET_SETTING(2, set_decoder_threads(3));
            SET_SETTING(3, set_decoder_threads(4));
            DEFAULT;
        }
    });

    std::vector<VideoCodec> supportedCodecs = {
#ifndef PLATFORM_ANDROID
        H264,
#endif
        H265,
    };

    std::vector<std::string> supportedCodecNames;
    for (int i = 0; i < supportedCodecs.size(); i++) {
        supportedCodecNames.push_back(getVideoCodecName(supportedCodecs[i]));
    }

    auto it = find(supportedCodecs.begin(), supportedCodecs.end(), Settings::instance().video_codec());

    int selected = 0;
    if (it != supportedCodecs.end()) {
        int index = it - supportedCodecs.begin();
        selected = index;
    }

    codec->init("settings/video_codec"_i18n, supportedCodecNames,
                selected, [supportedCodecs](int selected) {
                    Settings::instance().set_video_codec(supportedCodecs[selected]);
                });

    requestHdr->init("settings/request_hdr"_i18n, Settings::instance().request_hdr(),
                     [](bool value) { Settings::instance().set_request_hdr(value); });

 #ifndef SUPPORT_HDR
    requestHdr->removeFromSuperView(true);
 #endif

    hwDecoding->init("settings/use_hw_decoding"_i18n, Settings::instance().use_hw_decoding(),
                     [](bool value) { Settings::instance().set_use_hw_decoding(value); });

    hwDecoding->setEnabled(false);

#if defined(PLATFORM_SWITCH)
    const float mbpsMaxLimit = 100000;
#else
    const float mbpsMaxLimit = 150000;
#endif

    const float limitOffset = 500;
    const float limit = mbpsMaxLimit - limitOffset;

    float progress = (Settings::instance().bitrate() - limitOffset) / limit;
    slider->getProgressEvent()->subscribe([this, limitOffset, limit](float progress) {
        int bitrate = progress * limit + limitOffset;
        float fbitrate = bitrate / 1000.0f;
        std::stringstream stream;
        stream << std::fixed << std::setprecision(1) << fbitrate;
        header->setSubtitle(stream.str() + " Mbps");
        Settings::instance().set_bitrate(bitrate);
    });
    slider->setProgress(progress);

    audioBackend->init("settings/audio_backend"_i18n, audio_backends, Settings::instance().audio_backend(),
                       [](int selected) { Settings::instance().set_audio_backend((AudioBackend)selected); });

    optimal->init("settings/usops"_i18n, Settings::instance().sops(),
                  [](bool value) { Settings::instance().set_sops(value); });

    pcAudio->init(
        "settings/paop"_i18n, Settings::instance().play_audio(),
        [](bool value) { Settings::instance().set_play_audio(value); });

    swapUi->init("settings/swap_ui"_i18n, Settings::instance().swap_ui_keys(),
                 [](bool value) {
                     Settings::instance().set_swap_ui_keys(value);
                     brls::async([value] {
                         brls::sync([value] {
                             brls::Application::setSwapInputKeys(value);
                         });
                     });
                 });

    std::vector<std::string> layouts;
    for (KeyMappingLayout layout : *Settings::instance().get_mapping_laouts())
        layouts.push_back(layout.title);
    layouts.push_back("settings/keys_mapping_create_new"_i18n);

    swapGame->setText("settings/keys_mapping_title"_i18n);
    swapGame->setDetailTextColor(
        Application::getTheme()["brls/list/listItem_value_color"]);
    swapGame->setDetailText(
        layouts[Settings::instance().get_current_mapping_layout()]);

    swapGame->registerClickAction([this](View* view) {
        auto layouts = *Settings::instance().get_mapping_laouts();
        int current = Settings::instance().get_current_mapping_layout();

        std::vector<std::string> layoutTexts;
        for (KeyMappingLayout layout : layouts)
            layoutTexts.push_back(layout.title);
        layoutTexts.push_back("settings/keys_mapping_create_new"_i18n);

        Dropdown* dropdown = new Dropdown(
            swapGame->title->getFullText(), layoutTexts,
            [this, layoutTexts](int selected) {
                if (selected <
                    Settings::instance().get_mapping_laouts()->size()) {
                    Settings::instance().set_current_mapping_layout(selected);
                    swapGame->setDetailText(layoutTexts[selected]);
                }
            },
            Settings::instance().get_current_mapping_layout(),
            [this](int selected) {
                if (Settings::instance().get_mapping_laouts()->size() ==
                    selected) {
                    KeyMappingLayout layout;
                    layout.title = "settings/keys_mapping_new_title"_i18n;
                    layout.editable = true;
                    Settings::instance().get_mapping_laouts()->push_back(
                        layout);
                    Settings::instance().set_current_mapping_layout(selected);
                    swapGame->setDetailText(layout.title);

                    // Show layout editor View
                    MappingLayoutEditor* editor =
                        new MappingLayoutEditor(selected, [this] {
                            auto currentLayout =
                                Settings::instance().get_mapping_laouts()->at(
                                    Settings::instance()
                                        .get_current_mapping_layout());
                            this->swapGame->setDetailText(currentLayout.title);
                        });
                    this->present(editor);
                }
            });

        dropdown->registerAction(
            "common/edit"_i18n, BUTTON_Y, [this, dropdown](View* view) {
                Application::popActivity(
                    brls::TransitionAnimation::FADE, [this, dropdown]() {
                        RecyclerCell* cell = dynamic_cast<RecyclerCell*>(
                            dropdown->getDefaultFocus());
                        if (cell) {
                            // Show layout editor View
                            int index = cell->getIndexPath().row;
                            MappingLayoutEditor* editor =
                                new MappingLayoutEditor(index, [this] {
                                    auto currentLayout =
                                        Settings::instance()
                                            .get_mapping_laouts()
                                            ->at(
                                                Settings::instance()
                                                    .get_current_mapping_layout());
                                    this->swapGame->setDetailText(
                                        currentLayout.title);
                                });
                            this->present(editor);
                        }
                    });
                return true;
            });
        dropdown->setActionAvailable(BUTTON_Y, current < layouts.size() &&
                                                   layouts[current].editable);

        dropdown->getCellFocusDidChangeEvent()->subscribe(
            [dropdown](RecyclerCell* cell) {
                auto layouts = Settings::instance().get_mapping_laouts();
                int index = cell->getIndexPath().row;
                int layoutsCount =
                    (int)Settings::instance().get_mapping_laouts()->size();
                dropdown->setActionAvailable(BUTTON_Y,
                                             index < layoutsCount &&
                                                 layouts->at(index).editable);
            });

        Application::pushActivity(new Activity(dropdown));
        return true;
    });


    deadzoneStickLeft->setText("settings/deadzone/stick_left"_i18n);
    deadzoneStickRight->setText("settings/deadzone/stick_right"_i18n);

    updateDeadZoneItems();

    deadzoneStickLeft->registerClickAction([this](View* view) {
        int currentValue = int(Settings::instance().get_deadzone_stick_left() * 100);
        bool res = Application::getImeManager()->openForNumber([&](long number) {
                                                        Settings::instance().set_deadzone_stick_left(float(number) / 100.f);
                                                        this->updateDeadZoneItems();
                                                    },
                                                    "settings/deadzone/stick_left"_i18n, "settings/deadzone/input_hint"_i18n, 2,
                                                    currentValue > 0 ? std::to_string(currentValue) : "", "",
                                                    "", 0);

        if (!res) {
            Settings::instance().set_deadzone_stick_left(0);
            this->updateDeadZoneItems();
        }

        return true;
    });

    deadzoneStickRight->registerClickAction([this](View* view) {
        int currentValue = int(Settings::instance().get_deadzone_stick_right() * 100);
        bool res = Application::getImeManager()->openForNumber([&](long number) {
                                                        Settings::instance().set_deadzone_stick_right(float(number) / 100.f);
                                                        this->updateDeadZoneItems();
                                                    },
                                                    "settings/deadzone/stick_right"_i18n, "settings/deadzone/input_hint"_i18n, 2,
                                                    currentValue > 0 ? std::to_string(currentValue) : "", "",
                                                    "", 0);

        if (!res) {
            Settings::instance().set_deadzone_stick_right(0);
            this->updateDeadZoneItems();
        }

        return true;
    });

    float rumbleForceProgress = Settings::instance().get_rumble_force();
    rumbleForceSlider->getProgressEvent()->subscribe([this](float value) {
        std::stringstream stream;
        stream << std::fixed << std::setprecision(1) << int(value * 100);
        rumbleForceHeader->setSubtitle(stream.str() + "%");
        Settings::instance().set_rumble_force(value);
    });
    rumbleForceSlider->setProgress(rumbleForceProgress);

    swapStickToDpad->init("settings/swap_stick_to_dpad"_i18n, Settings::instance().swap_joycon_stick_to_dpad(),
                          [](bool value) { Settings::instance().set_swap_joycon_stick_to_dpad(value); });

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
    guideBySystemButton->removeFromSuperView();
    overlayBySystemButton->removeFromSuperView();
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

    overlayBySystemButton->init(
        "settings/use_system_button"_i18n,
        {"hints/off"_i18n, "settings/buttons/screenshot"_i18n, "settings/buttons/home"_i18n},
        (int) Settings::instance().get_overlay_system_button(), [this](int value) {
            if (value != 0 && Settings::instance().get_guide_system_button() == (ButtonOverrideType) value) {
                brls::sync([this, value](){
                    showError("settings/system_button_duplication_error"_i18n, [](){});
                });
                overlayBySystemButton->setSelection((int) Settings::instance().get_overlay_system_button(), true);
                return;
            }

            Settings::instance().set_overlay_system_button((ButtonOverrideType) value);

            auto color = Settings::instance().get_overlay_system_button() == ButtonOverrideType::NONE ?
                Application::getTheme()["brls/text_disabled"] : Application::getTheme()["brls/accent"];
            overlayBySystemButton->setDetailTextColor(color);
        });
    color = Settings::instance().get_overlay_system_button() == ButtonOverrideType::NONE ?
         Application::getTheme()["brls/text_disabled"] : Application::getTheme()["brls/accent"];
    overlayBySystemButton->setDetailTextColor(color);
#endif

    overlayTime->init(
        "settings/overlay_time"_i18n,
        {"settings/overlay_zero_time"_i18n, "1", "2", "3", "4", "5"},
        Settings::instance().overlay_options().holdTime, [](int value) {
            auto options = Settings::instance().overlay_options();
            options.holdTime = value;
            Settings::instance().set_overlay_options(options);
        });

    overlayButtons->setText("settings/overlay_buttons"_i18n);
    setupButtonsSelectorCell(overlayButtons,
                             Settings::instance().overlay_options().buttons);
    overlayButtons->registerClickAction([this](View* view) {
        ButtonSelectingDialog* dialog = ButtonSelectingDialog::create(
            "settings/overlay_setup_message"_i18n, [this](auto buttons) {
                if (buttons.empty())
                    return;

                auto options = Settings::instance().overlay_options();
                options.buttons = buttons;
                Settings::instance().set_overlay_options(options);
                setupButtonsSelectorCell(overlayButtons, buttons);
            });

        dialog->open();
        return true;
    });

    mouseInputTime->init(
        "settings/overlay_time"_i18n,
        {"settings/overlay_zero_time"_i18n, "1", "2", "3", "4", "5"},
        Settings::instance().mouse_input_options().holdTime, [](int value) {
            auto options = Settings::instance().mouse_input_options();
            options.holdTime = value;
            Settings::instance().set_mouse_input_options(options);
        });

    mouseInputButtons->setText("settings/overlay_buttons"_i18n);
    setupButtonsSelectorCell(
        mouseInputButtons, Settings::instance().mouse_input_options().buttons);
    mouseInputButtons->registerClickAction([this](View* view) {
        ButtonSelectingDialog* dialog = ButtonSelectingDialog::create(
            "settings/mouse_input_setup_message"_i18n, [this](auto buttons) {
                auto options = Settings::instance().mouse_input_options();
                options.buttons = buttons;
                Settings::instance().set_mouse_input_options(options);
                setupButtonsSelectorCell(mouseInputButtons, buttons);
            });

        dialog->open();
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

    volumeAmplification->init(
        "settings/volume_amplification"_i18n,
        Settings::instance().get_volume_amplification(), [this](auto value) {
            Settings::instance().set_volume_amplification(value);

            if (!value && Settings::instance().get_volume() > 100)
                Settings::instance().set_volume(100);
        });

    touchscreenMouseMode->init("settings/touchscreen_mouse_mode"_i18n,
                               Settings::instance().touchscreen_mouse_mode(),
                               [this](bool value) {
                                   Settings::instance().set_touchscreen_mouse_mode(value);
                               });

    swapMouseKeys->init("settings/swap_mouse_keys"_i18n,
                        Settings::instance().swap_mouse_keys(),
                        [this](bool value) {
                            Settings::instance().set_swap_mouse_keys(value);
                        });

    swapMouseScroll->init("settings/swap_mouse_scroll"_i18n,
                          Settings::instance().swap_mouse_scroll(),
                          [this](bool value) {
                              Settings::instance().set_swap_mouse_scroll(value);
                          });

    float mouseProgress =
        (Settings::instance().get_mouse_speed_multiplier() / 100.0f);
    mouseSpeedSlider->getProgressEvent()->subscribe([this](float value) {
        float multiplier = value * 1.5f + 0.5f;
        std::stringstream stream;
        stream << std::fixed << std::setprecision(1) << multiplier;
        mouseSpeedHeader->setSubtitle("x" + stream.str());
        Settings::instance().set_mouse_speed_multiplier(value * 100);
    });
    mouseSpeedSlider->setProgress(mouseProgress);

    writeLog->init("settings/debugging_view"_i18n,
                   Settings::instance().write_log(), [](bool value) {
                       Settings::instance().set_write_log(value);
                       brls::Application::enableDebuggingView(value);
                   });
}

void SettingsTab::updateDeadZoneItems() {
    if (Settings::instance().get_deadzone_stick_left() > 0) {
        deadzoneStickLeft->setDetailTextColor(Application::getTheme()["brls/list/listItem_value_color"]);
        deadzoneStickLeft->setDetailText(fmt::format("{}%", int(Settings::instance().get_deadzone_stick_left() * 100.f)));
    } else {
        deadzoneStickLeft->setDetailTextColor(Application::getTheme()["brls/text_disabled"]);
        deadzoneStickLeft->setDetailText("hints/off"_i18n);
    }

    if (Settings::instance().get_deadzone_stick_right() > 0) {
        deadzoneStickRight->setDetailTextColor(Application::getTheme()["brls/list/listItem_value_color"]);
        deadzoneStickRight->setDetailText(fmt::format("{}%", int(Settings::instance().get_deadzone_stick_right() * 100)));
    } else {
        deadzoneStickRight->setDetailTextColor(Application::getTheme()["brls/text_disabled"]);
        deadzoneStickRight->setDetailText("hints/off"_i18n);
    }
}

void SettingsTab::setupButtonsSelectorCell(
    brls::DetailCell* cell, std::vector<ControllerButton> buttons) {
    cell->setDetailText(getTextFromButtons(buttons));
    cell->setDetailTextColor(getColorFromButtons(buttons));
}

std::string
SettingsTab::getTextFromButtons(std::vector<ControllerButton> buttons) {
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
SettingsTab::getColorFromButtons(std::vector<brls::ControllerButton> buttons) {
    Theme theme = Application::getTheme();
    return buttons.empty() ? theme["brls/text_disabled"]
                           : theme["brls/list/listItem_value_color"];
}

SettingsTab::~SettingsTab() { Settings::instance().save(); }

brls::View* SettingsTab::create() { return new SettingsTab(); }
