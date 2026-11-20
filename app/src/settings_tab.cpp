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
#include "MouseScrollSettings.hpp"
#include "SystemButtonSettings.hpp"
#include "helper.hpp"
#include "button_selecting_dialog.hpp"
#include "mapping_layout_editor.hpp"
#include "halfcon_mapping_view.hpp"
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

using namespace brls::literals;

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

#if defined(__SWITCH__)
std::vector<std::string> audio_backends {
    "Audren",
};
#elif defined(__SDL2__) || defined(__SDL3__)
std::vector<std::string> audio_backends {
#if defined(__SDL3__)
    "SDL3",
#else
    "SDL2",
#endif
};
#else
std::vector<std::string> audio_backends;
#endif


SettingsTab::SettingsTab(int page) {
    // Inflate the tab from the XML file
    this->inflateFromXMLRes("xml/tabs/settings.xml");

    std::vector<std::string> resolutions = {
        "settings/resolution_native"_i18n,
        "360p",
        "480p",
        "540p",
        "720p",
        "1080p",
#if !defined(__PSV__)
        "1440p",
#endif
#ifdef __SWITCH2__
        "2160p (4K)",
#endif
    };
    resolution->setText("settings/resolution"_i18n);
    resolution->setData(resolutions);
    switch (Settings::instance().resolution()) {
        GET_SETTINGS(resolution, -1, 0);
        GET_SETTINGS(resolution, 360, 1);
        GET_SETTINGS(resolution, 480, 2);
        GET_SETTINGS(resolution, 540, 3);
        GET_SETTINGS(resolution, 720, 4);
        GET_SETTINGS(resolution, 1080, 5);
#if !defined(__PSV__)
        GET_SETTINGS(resolution, 1440, 6);
#endif
#ifdef __SWITCH2__
        GET_SETTINGS(resolution, 2160, 7);
#endif
        DEFAULT;
    }

    std::vector<std::string> nativeResolutionScales = {
        "0.5x", "0.75x", "1.0x",
#if !defined(__PSV__)
        "2.0x",
#endif
    };
    resolutionScale->setText("settings/resolution_scale"_i18n);
    resolutionScale->setData(nativeResolutionScales);
    switch (Settings::instance().native_resolution_scale()) {
        GET_SETTINGS(resolutionScale, 50, 0);
        GET_SETTINGS(resolutionScale, 75, 1);
        GET_SETTINGS(resolutionScale, 100, 2);
#if !defined(__PSV__)
        GET_SETTINGS(resolutionScale, 200, 3);
#endif
        default:
            resolutionScale->setSelection(2);
            break;
    }
    resolutionScale->getEvent()->subscribe([](int selected) {
        switch (selected) {
            SET_SETTING(0, set_native_resolution_scale(50));
            SET_SETTING(1, set_native_resolution_scale(75));
            SET_SETTING(2, set_native_resolution_scale(100));
#if !defined(__PSV__)
            SET_SETTING(3, set_native_resolution_scale(200));
#endif
            DEFAULT;
        }
    });

#ifdef SUPPORT_UPSCALING
    if (!isVideoUpscalingSupported()) {
        imageAdjustmentsHeader->removeFromSuperView(true);
        dithering->removeFromSuperView(true);
        upscaling->removeFromSuperView(true);
        upscalingMode->removeFromSuperView(true);
        rcas->removeFromSuperView(true);
    } else {
        auto updateDitheringControls = [this](bool enabled) {
            updateStrengthControl(dithering, enabled,
                                  getDitheringStrengthText(
                                      Settings::instance().dithering_strength()));
        };
        auto updateRcasControls = [this](bool enabled) {
            updateStrengthControl(rcas, enabled,
                                  getRcasStrengthText(
                                      Settings::instance().rcas_strength()));
        };

        dithering->init("settings/dithering"_i18n, Settings::instance().dithering(),
                        [updateDitheringControls](bool value) {
                            Settings::instance().set_dithering(value);
                            updateDitheringControls(value);
                        });
        const float ditheringStrength = Settings::instance().dithering_strength();
        dithering->getProgressEvent()->subscribe([this](float value) {
            const float strength = sliderProgressToDitheringStrength(value);
            Settings::instance().set_dithering_strength(strength);
            updateStrengthControl(dithering,
                                  Settings::instance().dithering(),
                                  getDitheringStrengthText(strength));
        });
        dithering->setProgress(
            ditheringStrengthToSliderProgress(ditheringStrength));
        updateDitheringControls(Settings::instance().dithering());

#if defined(PLATFORM_APPLE) && !defined(PLATFORM_TVOS)
        upscaling->removeFromSuperView(true);
        upscalingMode->init(
            "settings/upscaling"_i18n,
            {"hints/off"_i18n, "MetalFX", "FSR1"},
            (int)Settings::instance().upscaling_mode(),
            [](int value) { Settings::instance().set_upscaling_mode((UpscalingMode)value); });
#else
        upscalingMode->removeFromSuperView(true);
        upscaling->init("settings/upscaling"_i18n, Settings::instance().upscaling(),
                        [](bool value) { Settings::instance().set_upscaling(value); });
#endif
        rcas->init("settings/rcas_sharpening"_i18n, Settings::instance().rcas(),
                   [updateRcasControls](bool value) {
                       Settings::instance().set_rcas(value);
                       updateRcasControls(value);
                   });

        const float rcasStrength = Settings::instance().rcas_strength();
        rcas->getProgressEvent()->subscribe([this](float value) {
            Settings::instance().set_rcas_strength(value);
            updateStrengthControl(rcas,
                                  Settings::instance().rcas(),
                                  getRcasStrengthText(value));
        });
        rcas->setProgress(rcasStrength);
        updateRcasControls(Settings::instance().rcas());
    }
#else
    imageAdjustmentsHeader->removeFromSuperView(true);
    dithering->removeFromSuperView(true);
    upscaling->removeFromSuperView(true);
    upscalingMode->removeFromSuperView(true);
    rcas->removeFromSuperView(true);
#endif

    auto updateNativeResolutionScaleVisibility = [this]() {
        resolutionScale->setVisibility(
            resolution->getSelection() == 0 ? brls::Visibility::VISIBLE
                                            : brls::Visibility::GONE);
    };
    updateNativeResolutionScaleVisibility();

    resolution->getEvent()->subscribe([this, updateNativeResolutionScaleVisibility](int selected) {
        switch (selected) {
            SET_SETTING(0, set_resolution(-1));
            SET_SETTING(1, set_resolution(360));
            SET_SETTING(2, set_resolution(480));
            SET_SETTING(3, set_resolution(540));
            SET_SETTING(4, set_resolution(720));
            SET_SETTING(5, set_resolution(1080));
#if !defined(__PSV__)
            SET_SETTING(6, set_resolution(1440));
#endif
#ifdef __SWITCH2__
            SET_SETTING(7, set_resolution(2160));
#endif
            DEFAULT;
        }
        updateNativeResolutionScaleVisibility();
    });

#if defined(__PSV__)
    std::vector<std::string> fpss = {
        "24",
        "30",
        "40",
        "50",
        "60",
    };
#else
    std::vector<std::string> fpss = {
        "30",
        "40",
        "60",
        "120",
    };
#endif
    fps->setText("settings/fps"_i18n);
    fps->setData(fpss);
    switch (Settings::instance().fps()) {
#if defined(__PSV__)
        GET_SETTINGS(fps, 24, 0);
        GET_SETTINGS(fps, 30, 1);
        GET_SETTINGS(fps, 40, 2);
        GET_SETTINGS(fps, 50, 3);
        GET_SETTINGS(fps, 60, 4);
#else
        GET_SETTINGS(fps, 30, 0);
        GET_SETTINGS(fps, 40, 1);
        GET_SETTINGS(fps, 60, 2);
        GET_SETTINGS(fps, 120, 3);
#endif
        DEFAULT;
    }
    fps->getEvent()->subscribe([](int selected) {
        switch (selected) {
#if defined(__PSV__)
            SET_SETTING(0, set_fps(24));
            SET_SETTING(1, set_fps(30));
            SET_SETTING(2, set_fps(40));
            SET_SETTING(3, set_fps(50));
            SET_SETTING(4, set_fps(60));
#else
            SET_SETTING(0, set_fps(30));
            SET_SETTING(1, set_fps(40));
            SET_SETTING(2, set_fps(60));
            SET_SETTING(3, set_fps(120));
#endif
            DEFAULT;
        }
    });

#if defined(PLATFORM_SWITCH) && !defined(__SWITCH2__)
    // Each row edits the saved profile, never the active session snapshot.
    auto initProfileNumber = [](brls::SelectorCell* cell, const std::string& title, std::vector<int> values, int current, std::function<void(int)> setter) {
        if(std::find(values.begin(),values.end(),current)==values.end()) values.push_back(current);
        std::vector<std::string> labels;int selected=0;
        for(size_t i=0;i<values.size();++i){ labels.push_back(std::to_string(values[i]));if(values[i]==current)selected=(int)i; }
        cell->init(title,labels,selected,[values,setter](int n){setter(values.at(n));});
    };
    initProfileNumber(profile0res, "settings/profile_0_res"_i18n, {360,480,540,720,900,1080}, Settings::instance().stream_profile(0).resolution, [](int v){Settings::instance().stream_profile(0).resolution=v;});
    initProfileNumber(profile0fps, "settings/profile_0_fps"_i18n, {24,30,40,50,60}, Settings::instance().stream_profile(0).fps, [](int v){Settings::instance().stream_profile(0).fps=v;});
    initProfileNumber(profile0rate, "settings/profile_0_rate"_i18n, {500,1000,2000,5000,10000,15000,20000,25000,30000,40000,50000,75000,100000}, Settings::instance().stream_profile(0).bitrate, [](int v){Settings::instance().stream_profile(0).bitrate=v;});
    profile0power->init("settings/profile_0_power"_i18n, {"settings/nvdec_battery"_i18n,"settings/nvdec_max"_i18n}, Settings::instance().stream_profile(0).maximum ? 1 : 0, [](int v){Settings::instance().stream_profile(0).maximum=v==1;});
    initProfileNumber(profile1res, "settings/profile_1_res"_i18n, {360,480,540,720,900,1080}, Settings::instance().stream_profile(1).resolution, [](int v){Settings::instance().stream_profile(1).resolution=v;});
    initProfileNumber(profile1fps, "settings/profile_1_fps"_i18n, {24,30,40,50,60}, Settings::instance().stream_profile(1).fps, [](int v){Settings::instance().stream_profile(1).fps=v;});
    initProfileNumber(profile1rate, "settings/profile_1_rate"_i18n, {500,1000,2000,5000,10000,15000,20000,25000,30000,40000,50000,75000,100000}, Settings::instance().stream_profile(1).bitrate, [](int v){Settings::instance().stream_profile(1).bitrate=v;});
    profile1power->init("settings/profile_1_power"_i18n, {"settings/nvdec_battery"_i18n,"settings/nvdec_max"_i18n}, Settings::instance().stream_profile(1).maximum ? 1 : 0, [](int v){Settings::instance().stream_profile(1).maximum=v==1;});
#else
    profile0res->removeFromSuperView(true);
    profile0fps->removeFromSuperView(true);
    profile0rate->removeFromSuperView(true);
    profile0power->removeFromSuperView(true);
    profile1res->removeFromSuperView(true);
    profile1fps->removeFromSuperView(true);
    profile1rate->removeFromSuperView(true);
    profile1power->removeFromSuperView(true);
#endif
    saveDiagnosticLog->init("settings/save_diagnostic_log"_i18n, Settings::instance().save_diagnostic_log(),
        [](bool enabled) { Settings::instance().set_save_diagnostic_log(enabled); });
#if !defined(PLATFORM_SWITCH) || defined(__SWITCH2__)
    saveDiagnosticLog->removeFromSuperView(true);
#endif
    videoVsync->init("settings/video_vsync"_i18n, Settings::instance().video_vsync(),
        [](bool enabled) { Settings::instance().set_video_vsync(enabled); });
#if !defined(PLATFORM_SWITCH) || defined(__SWITCH2__)
    videoVsync->removeFromSuperView(true);
#endif
    nvdecPower->init("settings/nvdec_power"_i18n,
        {"settings/nvdec_battery"_i18n, "settings/nvdec_max"_i18n},
        Settings::instance().nvdec_max_performance() ? 1 : 0,
        [](int selected) { Settings::instance().set_nvdec_max_performance(selected == 1); });
#if !defined(PLATFORM_SWITCH) || defined(__SWITCH2__)
    nvdecPower->removeFromSuperView(true);
#endif
    videoPacing->init("settings/video_pacing"_i18n,
        {"settings/prefer_low_latency"_i18n, "settings/prefer_smooth_video"_i18n},
        Settings::instance().low_latency_video() ? 0 : 1,
        [](int selected) {
            Settings::instance().set_low_latency_video(selected == 0);
        });
#if !defined(PLATFORM_SWITCH) || defined(__SWITCH2__)
    videoPacing->removeFromSuperView(true);
#endif

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
        H264,
#if !defined(__PSV__)
        H265,
#endif
#ifdef __SWITCH2__
        AV1,
#endif
    };

    std::vector<std::string> supportedCodecNames;
    for (int i = 0; i < supportedCodecs.size(); i++) {
        std::string name = getVideoCodecName(supportedCodecs[i]);
#ifdef __SWITCH2__
        if (supportedCodecs[i] == H265)
            name += " (software)";
#endif
        supportedCodecNames.push_back(name);
    }

    auto it = find(supportedCodecs.begin(), supportedCodecs.end(), Settings::instance().video_codec());

    int selected = 0;
    if (it != supportedCodecs.end()) {
        int index = it - supportedCodecs.begin();
        selected = index;
    }

    codec->init("settings/video_codec"_i18n, supportedCodecNames, selected,
        [this, supportedCodecs](int selected)
        {
            Settings::instance().set_video_codec(supportedCodecs[selected]);
#ifdef __SWITCH2__
            hwDecoding->setOn(supportedCodecs[selected] != H265 && Settings::instance().use_hw_decoding());
            hwDecoding->setEnabled(supportedCodecs[selected] != H265);
#endif
        });

    requestHdr->init("settings/request_hdr"_i18n, Settings::instance().request_hdr(),
                     [](bool value) { Settings::instance().set_request_hdr(value); });

 #ifndef SUPPORT_HDR
    requestHdr->removeFromSuperView(true);
 #endif

    hwDecoding->init("settings/use_hw_decoding"_i18n,
#ifdef __SWITCH2__
        Settings::instance().video_codec() != H265 &&
#endif
            Settings::instance().use_hw_decoding(),
        [](bool value) { Settings::instance().set_use_hw_decoding(value); });

#if defined(__SWITCH2__)
    hwDecoding->setEnabled(Settings::instance().video_codec() != H265);
#elif defined(__linux__) && defined(PLATFORM_DESKTOP)
    hwDecoding->setEnabled(true);
#else
    hwDecoding->setEnabled(false);
#endif

#if defined(__PSV__)
    const float mbpsMaxLimit = 20000;
#elif defined(PLATFORM_SWITCH)
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

#if defined(__SWITCH__)
    audioBackend->init("settings/audio_backend"_i18n, audio_backends, 0,
                       [](int) { Settings::instance().set_audio_backend(AUDREN); });
#elif defined(__SDL2__) || defined(__SDL3__)
    audioBackend->init("settings/audio_backend"_i18n, audio_backends, Settings::instance().audio_backend(),
                       [](int selected) { Settings::instance().set_audio_backend((AudioBackend)selected); });
#else
    audioBackend->removeFromSuperView(true);
#endif

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
#ifdef __SWITCH__
    halfconMapping->setText("halfcon/title"_i18n);
    halfconMapping->registerClickAction(
        [this](View*)
        {
            this->present(new HalfConMappingView());
            return true;
        });
#else
    halfconMapping->setVisibility(Visibility::GONE);
#endif
    swapGame->setDetailTextColor(Application::getTheme()["brls/list/listItem_value_color"]);
    swapGame->setDetailText(layouts[Settings::instance().get_current_mapping_layout()]);

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
        systemButtonOptions(),
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
        systemButtonOptions(),
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
        "3", "4", "5", "hints/off"_i18n};
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
    setupMouseScrollSettings(mouseScrollHeader, mouseScrollSlider);

#if defined(PLATFORM_SWITCH) && !defined(__SWITCH2__)
    // These values are now supplied by the two explicit profiles.
    for (brls::View* row : std::initializer_list<brls::View*>{fps,resolution,resolutionScale,nvdecPower,header,slider})
        row->setVisibility(brls::Visibility::GONE);
#endif
    audioReserve->init("settings/audio_reserve"_i18n, {"0 ms", "10 ms", "20 ms", "30 ms", "40 ms", "50 ms", "60 ms", "70 ms", "80 ms", "90 ms", "100 ms"},
        Settings::instance().audio_reserve_ms() / 10,
        [](int selected) { Settings::instance().set_audio_reserve_ms(selected * 10); });
#if defined(PLATFORM_SWITCH) && !defined(__SWITCH2__)
    brls::View* selectedGroup = page == 1 ? static_cast<brls::Box*>(handheldGroup) :
        page == 2 ? static_cast<brls::Box*>(dockedGroup) : page == 3 ? static_cast<brls::Box*>(debugGroup) : nullptr;
    if (selectedGroup) {
        for (auto* row : settingsContent->getChildren())
            row->setVisibility(row == selectedGroup ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    } else {
        handheldGroup->setVisibility(brls::Visibility::GONE);
        dockedGroup->setVisibility(brls::Visibility::GONE);
        debugGroup->setVisibility(brls::Visibility::GONE);
    }
#endif
    writeLog->init("settings/debugging_view"_i18n,
                   Settings::instance().write_log(), [](bool value) {
                       Settings::instance().set_write_log(value);
                       brls::Application::enableDebuggingView(value);
                   });
}

void SettingsTab::willAppear(bool resetState)
{
    Box::willAppear(resetState);
    mouseSpeedSlider->setProgress(Settings::instance().get_mouse_speed_multiplier() / 100.0f);
    refreshMouseScrollSettings(mouseScrollSlider);
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

#if defined(PLATFORM_SWITCH) && !defined(__SWITCH2__)
namespace {
class HorizontalSettingsTabs final : public brls::Box {
public:
    HorizontalSettingsTabs() {
        this->inflateFromXMLString(R"xml(
            <brls:Box axis="column" width="auto" height="auto">
                <brls:Box axis="row" width="auto" height="64" paddingLeft="16" paddingRight="16">
                    <brls:Button id="settings_tab_0" width="0" height="52" grow="1" marginRight="6"/>
                    <brls:Button id="settings_tab_1" width="0" height="52" grow="1" marginRight="6"/>
                    <brls:Button id="settings_tab_2" width="0" height="52" grow="1" marginRight="6"/>
                    <brls:Button id="settings_tab_3" width="0" height="52" grow="1"/>
                </brls:Box>
                <brls:Box id="settings_pages_content" axis="column" width="auto" height="0" grow="1"/>
            </brls:Box>
        )xml");
        content = dynamic_cast<brls::Box*>(this->getView("settings_pages_content"));
        const std::string labels[] = {"settings/tab_general"_i18n, "settings/tab_handheld"_i18n,
                                     "settings/tab_docked"_i18n, "settings/debug"_i18n};
        for (int i = 0; i < 4; ++i) {
            buttons[i] = dynamic_cast<brls::Button*>(this->getView("settings_tab_" + std::to_string(i)));
            buttons[i]->setText(labels[i]);
            buttons[i]->registerClickAction([this, i](brls::View*) { select(i, true); return true; });
        }
        this->registerAction("settings/previous_tab"_i18n, brls::BUTTON_LB,
            [this](brls::View*) { select((selected + 3) % 4, true); return true; });
        this->registerAction("settings/next_tab"_i18n, brls::BUTTON_RB,
            [this](brls::View*) { select((selected + 1) % 4, true); return true; });
        select(0, false);
    }
    brls::View* getDefaultFocus() override { return page ? page->getDefaultFocus() : buttons[0]; }
private:
    void select(int index, bool focus) {
        if (page && selected == index) return;
        // Move focus off controls before deleting the previous page.
        if (focus) brls::Application::giveFocus(buttons[index]);
        if (page) { content->removeView(page); page = nullptr; }
        selected = index;
        for (int i = 0; i < 4; ++i)
            buttons[i]->setStyle(i == selected ? &brls::BUTTONSTYLE_PRIMARY : &brls::BUTTONSTYLE_BORDERLESS);
        page = new SettingsTab(selected);
        page->setGrow(1.0f);
        content->addView(page);
        if (focus && brls::Application::getInputType() != brls::InputType::TOUCH)
            brls::Application::giveFocus(page->getDefaultFocus());
    }
    brls::Box* content = nullptr;
    brls::Button* buttons[4]{};
    SettingsTab* page = nullptr;
    int selected = 0;
};
} // namespace
#endif

brls::View* SettingsTab::create() {
#if defined(PLATFORM_SWITCH) && !defined(__SWITCH2__)
    return new HorizontalSettingsTabs();
#else
    return new SettingsTab();
#endif
}
