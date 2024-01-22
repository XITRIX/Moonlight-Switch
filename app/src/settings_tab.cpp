//
//  settings_tab.cpp
//  Moonlight
//
//  Created by XITRIX on 26.05.2021.
//

#include "settings_tab.hpp"
#include "Settings.hpp"
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

    std::vector<std::string> resolutions = {"720p", "1080p"};
    resolution->setText("settings/resolution"_i18n);
    resolution->setData(resolutions);
    switch (Settings::instance().resolution()) {
        GET_SETTINGS(resolution, 720, 0);
        GET_SETTINGS(resolution, 1080, 1);
        DEFAULT;
    }
    resolution->getEvent()->subscribe([](int selected) {
        switch (selected) {
            SET_SETTING(0, set_resolution(720));
            SET_SETTING(1, set_resolution(1080));
            DEFAULT;
        }
    });

    std::vector<std::string> fpss = {"30", "60", "120"};
    fps->setText("settings/fps"_i18n);
    fps->setData(fpss);
    switch (Settings::instance().fps()) {
        GET_SETTINGS(fps, 30, 0);
        GET_SETTINGS(fps, 60, 1);
        GET_SETTINGS(fps, 120, 2);
        DEFAULT;
    }
    fps->getEvent()->subscribe([](int selected) {
        switch (selected) {
            SET_SETTING(0, set_fps(30));
            SET_SETTING(1, set_fps(60));
            SET_SETTING(2, set_fps(120));
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

    codec->init("settings/video_codec"_i18n,
                {
                    "settings/h264"_i18n,
                    "settings/h265"_i18n,
            //        "settings/av1"_i18n
                },
                Settings::instance().video_codec(), [](int selected) {
                    Settings::instance().set_video_codec((VideoCodec)selected);
                });

    hwDecoding->init("settings/use_hw_decoding"_i18n, Settings::instance().use_hw_decoding(),
                     [](bool value) { Settings::instance().set_use_hw_decoding(value); });

    const float mbpsMaxLimit = 100000;

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
