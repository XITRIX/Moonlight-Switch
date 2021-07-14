//
//  settings_tab.cpp
//  Moonlight
//
//  Created by XITRIX on 26.05.2021.
//

#include "settings_tab.hpp"
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
    
    writeLog->init("main/settings/debugging_view"_i18n, Settings::instance().write_log(), [](bool value) {
        Settings::instance().set_write_log(value);
        brls::Application::enableDebuggingView(value);
    });
}

SettingsTab::~SettingsTab()
{
    Settings::instance().save();
}

brls::View* SettingsTab::create()
{
    return new SettingsTab();
}
