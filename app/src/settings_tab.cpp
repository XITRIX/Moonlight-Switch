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
    resolution->setText("main/settings/resolution"_i18n);
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
    fps->setText("main/settings/fps"_i18n);
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
    
    std::vector<std::string> decoders = { "main/settings/zero_threads"_i18n, "2", "3", "4" };
    decoder->setText("main/settings/decoder_threads"_i18n);
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
    
    codec->init("main/settings/video_codec"_i18n, { "main/settings/h264"_i18n, "main/settings/h265"_i18n }, Settings::instance().video_codec(), [](int selected) {
        Settings::instance().set_video_codec((VideoCodec)selected);
    });
    
    float progress = (Settings::instance().bitrate() - 500.0f) / 149500.0f;
    slider->getProgressEvent()->subscribe([this](float progress) {
        int bitrate = progress * 149500.0f + 500.0f;
        float fbitrate = bitrate / 1000.0f;
        std::stringstream stream;
        stream << std::fixed << std::setprecision(1) << fbitrate;
        header->setSubtitle(stream.str() + " Mbps");
        Settings::instance().set_bitrate(bitrate);
    });
    slider->setProgress(progress);
    
    optimal->init("main/settings/usops"_i18n, Settings::instance().sops(), [](bool value) {
        Settings::instance().set_sops(value);
    });
    
    pcAudio->init("main/settings/paop"_i18n, Settings::instance().play_audio(), [](bool value){
        Settings::instance().set_play_audio(value);
    });
    
    swapUi->init("main/settings/swap_ui"_i18n, Settings::instance().swap_ui_keys(), [](bool value) {
        Settings::instance().set_swap_ui_keys(value);
        brls::async([value] { brls::sync([value] { brls::Application::setSwapInputKeys(value); }); });
    });
    
    swapGame->init("main/settings/swap_game"_i18n, Settings::instance().swap_game_keys(), [](bool value) {
        Settings::instance().set_swap_game_keys(value);
    });
    
    overlayTime->init("main/settings/overlay_time"_i18n, { "main/settings/overlay_zero_time"_i18n, "1", "2", "3", "4", "5" }, Settings::instance().overlay_options().holdTime, [](int value) {
        auto options = Settings::instance().overlay_options();
        options.holdTime = value;
        Settings::instance().set_overlay_options(options);
    });
    
    setupOverlayButtonsCell();
    overlayButtons->setText("main/settings/overlay_buttons"_i18n);
    overlayButtons->registerClickAction([this](View* view) {
        ButtonSelectingDialog* dialog = ButtonSelectingDialog::create("main/settings/overlay_setup_message"_i18n);
        dialog->addButton("main/cummon/cancel"_i18n, [dialog] { dialog->resetButtons(); });
        dialog->addButton("main/cummon/confirm"_i18n, [this, dialog] {
            auto options = Settings::instance().overlay_options();
            options.buttons = dialog->getButtons();
            Settings::instance().set_overlay_options(options);
            this->setupOverlayButtonsCell();
        });
        dialog->open();
        return true;
    });
    
    writeLog->init("main/settings/debugging_view"_i18n, Settings::instance().write_log(), [](bool value) {
        Settings::instance().set_write_log(value);
        brls::Application::enableDebuggingView(value);
    });
}

void SettingsTab::setupOverlayButtonsCell()
{
    std::string buttonsText = "";
    auto buttons = Settings::instance().overlay_options().buttons;
    for (int i = 0; i < buttons.size(); i++) {
        buttonsText += brls::Hint::getKeyIcon(buttons[i]);
        if (i < buttons.size() - 1)
            buttonsText += " + ";
    }
    overlayButtons->setDetailText(buttonsText);
}

SettingsTab::~SettingsTab()
{
    Settings::instance().save();
}

brls::View* SettingsTab::create()
{
    return new SettingsTab();
}
