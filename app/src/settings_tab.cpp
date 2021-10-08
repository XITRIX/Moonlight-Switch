//
//  settings_tab.cpp
//  Moonlight
//
//  Created by XITRIX on 26.05.2021.
//

#include "settings_tab.hpp"
#include "button_selecting_dialog.hpp"
#include "Settings.hpp"
#include <iomanip>
#include <sstream>

#define SET_SETTING(n, func) \
    case n: \
        Settings::instance().func; \
        break;

#define GET_SETTINGS(combo_box, n, i) \
    case n: \
        combo_box->setSelection(i); \
        break;

#define DEFAULT \
    default: \
        break;

using namespace brls::literals;

SettingsTab::SettingsTab()
{
    // Inflate the tab from the XML file
    this->inflateFromXMLRes("xml/tabs/settings.xml");
    
    std::vector<std::string> resolutions = { "720p", "1080p" };
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
    
    std::vector<std::string> fpss = { "30", "60" };
    fps->setText("settings/fps"_i18n);
    fps->setData(fpss);
    switch (Settings::instance().fps()) {
        GET_SETTINGS(fps, 30, 0);
        GET_SETTINGS(fps, 60, 1);
        DEFAULT;
    }
    fps->getEvent()->subscribe([](int selected) {
        switch (selected) {
            SET_SETTING(0, set_fps(30));
            SET_SETTING(1, set_fps(60));
            DEFAULT;
        }
    });
    
    std::vector<std::string> decoders = { "settings/zero_threads"_i18n, "2", "3", "4" };
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
    
    codec->init("settings/video_codec"_i18n, { "settings/h264"_i18n, "settings/h265"_i18n }, Settings::instance().video_codec(), [](int selected) {
        Settings::instance().set_video_codec((VideoCodec)selected);
    });
    
    float progress = (Settings::instance().bitrate() - 500.0f) / 49500.0f;
    slider->getProgressEvent()->subscribe([this](float progress) {
        int bitrate = progress * 49500.0f + 500.0f;
        float fbitrate = bitrate / 1000.0f;
        std::stringstream stream;
        stream << std::fixed << std::setprecision(1) << fbitrate;
        header->setSubtitle(stream.str() + " Mbps");
        Settings::instance().set_bitrate(bitrate);
    });
    slider->setProgress(progress);
    
    optimal->init("settings/usops"_i18n, Settings::instance().sops(), [](bool value) {
        Settings::instance().set_sops(value);
    });
    
    pcAudio->init("settings/paop"_i18n, Settings::instance().play_audio(), [](bool value){
        Settings::instance().set_play_audio(value);
    });
    
    swapUi->init("settings/swap_ui"_i18n, Settings::instance().swap_ui_keys(), [](bool value) {
        Settings::instance().set_swap_ui_keys(value);
        brls::async([value] { brls::sync([value] { brls::Application::setSwapInputKeys(value); }); });
    });

    std::vector<std::string> layouts;
    for (KeyMappingLayout layout : *Settings::instance().get_mapping_laouts())
        layouts.push_back(layout.title);
    layouts.push_back("+ Create new layout");

    swapGame->setText("settings/keys_mapping"_i18n);
    swapGame->setData(layouts);
    swapGame->setSelection(Settings::instance().get_current_mapping_layout());
    swapGame->getEvent()->subscribe([this](int selected) {
        if (Settings::instance().get_mapping_laouts()->size() == selected) {
            this->swapGame->setSelection(Settings::instance().get_current_mapping_layout(), true);
            // Open mapping editor View
            return;
        }

        Settings::instance().set_current_mapping_layout(selected);
    });
    
    guideKeyButtons->setText("settings/guide_key_buttons"_i18n);
    setupButtonsSelectorCell(guideKeyButtons, Settings::instance().guide_key_options().buttons);
    guideKeyButtons->registerClickAction([this](View* view) {
        ButtonSelectingDialog* dialog = ButtonSelectingDialog::create("settings/guide_key_setup_message"_i18n, [this](auto buttons) {
            auto options = Settings::instance().guide_key_options();
            options.buttons = buttons;
            Settings::instance().set_guide_key_options(options);
            setupButtonsSelectorCell(guideKeyButtons, buttons);
        });
        
        dialog->open();
        return true;
    });
    
    overlayTime->init("settings/overlay_time"_i18n, { "settings/overlay_zero_time"_i18n, "1", "2", "3", "4", "5" }, Settings::instance().overlay_options().holdTime, [](int value) {
        auto options = Settings::instance().overlay_options();
        options.holdTime = value;
        Settings::instance().set_overlay_options(options);
    });
    
    overlayButtons->setText("settings/overlay_buttons"_i18n);
    setupButtonsSelectorCell(overlayButtons, Settings::instance().overlay_options().buttons);
    overlayButtons->registerClickAction([this](View* view) {
        ButtonSelectingDialog* dialog = ButtonSelectingDialog::create("settings/overlay_setup_message"_i18n, [this](auto buttons) {
            if (buttons.empty()) return;

            auto options = Settings::instance().overlay_options();
            options.buttons = buttons;
            Settings::instance().set_overlay_options(options);
            setupButtonsSelectorCell(overlayButtons, buttons);
        });
        
        dialog->open();
        return true;
    });
    
    volumeAmplification->init("settings/volume_amplification"_i18n, Settings::instance().get_volume_amplification(), [this] (auto value) {
        Settings::instance().set_volume_amplification(value);
        
        if (!value && Settings::instance().get_volume() > 100)
            Settings::instance().set_volume(100);
    });
    
    swapMouseKeys->init("settings/swap_mouse_keys"_i18n, Settings::instance().swap_mouse_keys(), [this](bool value) {
        Settings::instance().set_swap_mouse_keys(value);
    });
    
    swapMouseScroll->init("settings/swap_mouse_scroll"_i18n, Settings::instance().swap_mouse_scroll(), [this](bool value) {
        Settings::instance().set_swap_mouse_scroll(value);
    });

    float mouseProgress = (Settings::instance().get_mouse_speed_multiplier() / 100.0f);
    mouseSpeedSlider->getProgressEvent()->subscribe([this](float value) {
        float multiplier = value * 1.5f + 0.5f;
        std::stringstream stream;
        stream << std::fixed << std::setprecision(1) << multiplier;
        mouseSpeedHeader->setSubtitle("x" + stream.str());
        Settings::instance().set_mouse_speed_multiplier(value * 100);
    });
    mouseSpeedSlider->setProgress(mouseProgress);
    
    writeLog->init("settings/debugging_view"_i18n, Settings::instance().write_log(), [](bool value) {
        Settings::instance().set_write_log(value);
        brls::Application::enableDebuggingView(value);
    });
}

void SettingsTab::setupButtonsSelectorCell(brls::DetailCell* cell, std::vector<ControllerButton> buttons)
{
    cell->setDetailText(getTextFromButtons(buttons));
    cell->setDetailTextColor(getColorFromButtons(buttons));
}

std::string SettingsTab::getTextFromButtons(std::vector<ControllerButton> buttons)
{
    std::string buttonsText = "";
    if (buttons.size() > 0)
    {
        for (int i = 0; i < buttons.size(); i++) {
            buttonsText += brls::Hint::getKeyIcon(buttons[i], true);
            if (i < buttons.size() - 1)
                buttonsText += " + ";
        }
    }
    else
    {
        buttonsText = "hints/off"_i18n;
    }
    return buttonsText;
}

NVGcolor SettingsTab::getColorFromButtons(std::vector<brls::ControllerButton> buttons)
{
    Theme theme = Application::getTheme();
    return buttons.empty() ?
        theme["brls/text_disabled"] :
        theme["brls/list/listItem_value_color"];
}

SettingsTab::~SettingsTab()
{
    Settings::instance().save();
}

brls::View* SettingsTab::create()
{
    return new SettingsTab();
}
