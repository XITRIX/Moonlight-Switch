#include "stream_profiles_view.hpp"
#include "StreamProfileResolver.hpp"
#include "VideoCodecSupport.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

using namespace brls;
using namespace brls::literals;

namespace {
const std::vector<int> resolutions = {-1, 360, 480, 540, 720, 1080, 1440};
const std::vector<int> scales = {50, 75, 100, 200};
const std::vector<int> frameRates = {30, 40, 60, 120};
const std::vector<FramePacingMode> pacingModes = {
    FramePacingMode::LOWEST_LATENCY,
    FramePacingMode::BALANCED,
    FramePacingMode::SMOOTHEST_VIDEO,
};

template <typename T>
int indexOf(const std::vector<T>& values, T value, int fallback = 0) {
    const auto it = std::find(values.begin(), values.end(), value);
    return it == values.end() ? fallback : static_cast<int>(it - values.begin());
}

std::string bitrateText(int bitrate) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1) << bitrate / 1000.0f
           << " Mbps";
    return stream.str();
}
}

StreamProfilesView::StreamProfilesView(const Host& host) : host(host) {
    inflateFromXMLRes("xml/views/stream_profiles.xml");

    localWifi->setText(StreamProfileResolver::contextName(
        StreamProfileContext::LocalWifi));
    localEthernet->setText(StreamProfileResolver::contextName(
        StreamProfileContext::LocalEthernet));
    remote->setText(StreamProfileResolver::contextName(
        StreamProfileContext::Remote));
    localWifi->registerClickAction([this](View*) {
        loadContext(StreamProfileContext::LocalWifi);
        return true;
    });
    localEthernet->registerClickAction([this](View*) {
        loadContext(StreamProfileContext::LocalEthernet);
        return true;
    });
    remote->registerClickAction([this](View*) {
        loadContext(StreamProfileContext::Remote);
        return true;
    });

    resolution->init("settings/resolution"_i18n,
                     {"settings/resolution_native"_i18n, "360p", "480p",
                      "540p", "720p", "1080p", "1440p"},
                     0, [this](int selected) {
                         if (!refreshing && selected >= 0 &&
                             static_cast<size_t>(selected) < resolutions.size()) {
                             profile.resolution = resolutions[selected];
                             saveProfile();
                         }
                     });
    resolutionScale->init("settings/resolution_scale"_i18n,
                          {"0.5x", "0.75x", "1.0x", "2.0x"}, 2,
                          [this](int selected) {
                              if (!refreshing && selected >= 0 &&
                                  static_cast<size_t>(selected) < scales.size()) {
                                  profile.nativeResolutionScale = scales[selected];
                                  saveProfile();
                              }
                          });
    fps->init("settings/fps"_i18n, {"30", "40", "60", "120"}, 2,
              [this](int selected) {
                  if (!refreshing && selected >= 0 &&
                      static_cast<size_t>(selected) < frameRates.size()) {
                      profile.fps = frameRates[selected];
                      saveProfile();
                  }
              });

    codecs = {H264, H265};
    if (isAv1HardwareDecodingAvailable())
        codecs.push_back(AV1);
    std::vector<std::string> codecNames;
    for (auto value : codecs)
        codecNames.push_back(getVideoCodecName(value));
    codec->init("settings/video_codec"_i18n, codecNames, 1,
                [this](int selected) {
                    if (!refreshing && selected >= 0 &&
                        static_cast<size_t>(selected) < codecs.size()) {
                        profile.videoCodec = codecs[selected];
                        saveProfile();
                    }
                });

    std::vector<std::string> pacingNames;
    for (auto value : pacingModes)
        pacingNames.push_back(getFramePacingModeName(value));
    framePacing->init("settings/frame_pacing"_i18n, pacingNames, 1,
                      [this](int selected) {
                          if (!refreshing && selected >= 0 &&
                              static_cast<size_t>(selected) < pacingModes.size()) {
                              profile.framePacingMode = pacingModes[selected];
                              saveProfile();
                          }
                      });

    enabled->init("stream_profiles/use_custom"_i18n, false,
                  [this](bool value) {
                      if (refreshing)
                          return;
                      if (value) {
                          profile = StreamProfileResolver::profileFrom(
                              StreamProfileResolver::globalDefaults());
                      } else {
                          profile = {};
                      }
                      saveProfile();
                      refreshControls();
                  });
    requestHdr->init("settings/request_hdr"_i18n, false,
                     [this](bool value) {
                         if (!refreshing) {
                             profile.requestHdr = value;
                             saveProfile();
                         }
                     });
    upscaling->init("settings/upscaling"_i18n, false,
                    [this](bool value) {
                        if (!refreshing) {
                            profile.upscalingMode =
                                value ? UPSCALING_FSR1 : UPSCALING_OFF;
                            saveProfile();
                        }
                    });
    dithering->init("settings/dithering"_i18n, false,
                    [this](bool value) {
                        if (!refreshing) {
                            profile.dithering = value;
                            saveProfile();
                            refreshControls();
                        }
                    });
    dithering->getProgressEvent()->subscribe([this](float progress) {
        if (!refreshing) {
            profile.ditheringStrength = std::round(progress * 9.0f) + 1.0f;
            saveProfile();
            refreshControls();
        }
    });
    rcas->init("settings/rcas_sharpening"_i18n, false,
               [this](bool value) {
                   if (!refreshing) {
                       profile.rcas = value;
                       saveProfile();
                       refreshControls();
                   }
               });
    rcas->getProgressEvent()->subscribe([this](float progress) {
        if (!refreshing) {
            profile.rcasStrength = progress;
            saveProfile();
            refreshControls();
        }
    });

    bitrateSlider->getProgressEvent()->subscribe([this](float progress) {
        if (!refreshing) {
            profile.bitrate = static_cast<int>(progress * 99500.0f + 500.0f);
            saveProfile();
        }
    });

#ifndef SUPPORT_HDR
    requestHdr->setVisibility(Visibility::GONE);
#endif
#ifndef SUPPORT_UPSCALING
    imageAdjustmentsHeader->setVisibility(Visibility::GONE);
    upscaling->setVisibility(Visibility::GONE);
    dithering->setVisibility(Visibility::GONE);
    rcas->setVisibility(Visibility::GONE);
#endif

    loadContext(StreamProfileContext::LocalWifi);
}

StreamProfilesView::~StreamProfilesView() {
    Settings::instance().save();
}

void StreamProfilesView::loadContext(StreamProfileContext nextContext) {
    context = nextContext;
    host = Settings::instance().host(host).value_or(host);
    const auto it = host.streamProfiles.find(context);
    profile = it == host.streamProfiles.end() ? StreamProfile{} : it->second;
    refreshControls();
}

void StreamProfilesView::saveProfile() {
    Settings::instance().set_stream_profile(host, context, profile, false);
    host = Settings::instance().host(host).value_or(host);
    refreshSummary();
    refreshContextSummaries();
}

void StreamProfilesView::refreshControls() {
    refreshing = true;
    const ResolvedStreamSettings effective =
        StreamProfileResolver::resolve(host, context);
    enabled->setOn(profile.enabled, false);
    resolution->setSelection(indexOf(resolutions, effective.resolution), true);
    resolutionScale->setSelection(
        indexOf(scales, effective.nativeResolutionScale, 2), true);
    fps->setSelection(indexOf(frameRates, effective.fps, 2), true);
    codec->setSelection(indexOf(codecs, effective.videoCodec, 0), true);
    requestHdr->setOn(effective.requestHdr, false);
    framePacing->setSelection(
        indexOf(pacingModes, effective.framePacingMode, 1), true);
    bitrateHeader->setSubtitle(bitrateText(effective.bitrate));
    bitrateSlider->setProgress(
        std::clamp((effective.bitrate - 500.0f) / 99500.0f, 0.0f, 1.0f));
    upscaling->setOn(effective.upscalingMode != UPSCALING_OFF, false);
    dithering->setOn(effective.dithering, false);
    dithering->setProgress(
        std::clamp((effective.ditheringStrength - 1.0f) / 9.0f, 0.0f, 1.0f));
    dithering->setValueText(std::to_string(
        static_cast<int>(std::round(effective.ditheringStrength))));
    dithering->setSliderVisibility(
        effective.dithering ? Visibility::VISIBLE : Visibility::GONE);
    rcas->setOn(effective.rcas, false);
    rcas->setProgress(effective.rcasStrength);
    rcas->setValueText(std::to_string(
                           static_cast<int>(std::round(effective.rcasStrength * 100.0f))) +
                       "%");
    rcas->setSliderVisibility(effective.rcas ? Visibility::VISIBLE
                                             : Visibility::GONE);
    refreshing = false;
    refreshVisibility();
    refreshSummary();
    refreshContextSummaries();
}

void StreamProfilesView::refreshVisibility() {
    const auto visibility = profile.enabled ? Visibility::VISIBLE
                                            : Visibility::GONE;
    resolution->setVisibility(visibility);
    resolutionScale->setVisibility(
        profile.enabled &&
                StreamProfileResolver::resolve(host, context).resolution == -1
            ? Visibility::VISIBLE
            : Visibility::GONE);
    fps->setVisibility(visibility);
    codec->setVisibility(visibility);
#ifdef SUPPORT_HDR
    requestHdr->setVisibility(visibility);
#endif
    framePacing->setVisibility(visibility);
    bitrateHeader->setVisibility(visibility);
    bitrateSlider->setVisibility(visibility);
#ifdef SUPPORT_UPSCALING
    imageAdjustmentsHeader->setVisibility(visibility);
    upscaling->setVisibility(visibility);
    dithering->setVisibility(visibility);
    rcas->setVisibility(visibility);
#endif
}

void StreamProfilesView::refreshSummary() {
    const auto effective = StreamProfileResolver::resolve(host, context);
    summaryHeader->setTitle(StreamProfileResolver::contextName(context));
    summaryHeader->setSubtitle(
        std::string(profile.enabled ? "stream_profiles/configured"_i18n
                                    : "stream_profiles/inherited"_i18n) +
        " · " + StreamProfileResolver::summary(effective));
    bitrateHeader->setSubtitle(bitrateText(effective.bitrate));
    refreshVisibility();
}

void StreamProfilesView::refreshContextSummaries() {
    auto update = [this](DetailCell* cell, StreamProfileContext value) {
        const auto resolved = StreamProfileResolver::resolve(host, value);
        const auto it = host.streamProfiles.find(value);
        const bool configured =
            it != host.streamProfiles.end() && it->second.enabled;
        cell->setDetailText(
            std::string(configured ? "stream_profiles/configured"_i18n
                                   : "stream_profiles/inherited"_i18n) +
            " · " + StreamProfileResolver::summary(resolved));
    };
    update(localWifi, StreamProfileContext::LocalWifi);
    update(localEthernet, StreamProfileContext::LocalEthernet);
    update(remote, StreamProfileContext::Remote);
}
