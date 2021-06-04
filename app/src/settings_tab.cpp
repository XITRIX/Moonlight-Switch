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

SettingsTab::SettingsTab()
{
    // Inflate the tab from the XML file
    this->inflateFromXMLRes("xml/tabs/settings.xml");
    
    std::vector<std::string> resolutions = { "720p", "1080p" };
    resolution->setText("Resolution");
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
    fps->setText("FPS");
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
    
    std::vector<std::string> decoders = { "0 (No use threads)", "2", "3", "4" };
    decoder->setText("Decoder Threads");
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
    
    codec->init("Video codec", { "H.264", "HEVC (H.265, Experimental)" }, Settings::instance().video_codec(), [](int selected) {
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
    
    optimal->init("Use Streaming Optimal Playable Settings", Settings::instance().sops(), [](bool value) {
        Settings::instance().set_sops(value);
    });
    
    pcAudio->init("Play Audio on PC", Settings::instance().play_audio(), [](bool value){
        Settings::instance().set_play_audio(value);
    });
    
    writeLog->init("Show debugging view", Settings::instance().write_log(), [](bool value) {
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
