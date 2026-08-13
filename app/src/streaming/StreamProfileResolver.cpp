#include "StreamProfileResolver.hpp"
#include "VideoCodecSupport.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>

#ifdef __SWITCH__
#include <switch.h>
#endif

using namespace brls::literals;

namespace {
const ResolvedStreamSettings* activeSettings = nullptr;

bool isValidResolution(int value) {
    return value == -1 || value == 360 || value == 480 || value == 540 ||
           value == 720 || value == 1080 || value == 1440;
}

bool isValidScale(int value) {
    return value == 50 || value == 75 || value == 100 || value == 200;
}

bool isValidFps(int value) {
    return value == 24 || value == 30 || value == 40 || value == 50 ||
           value == 60 || value == 120;
}

bool isValidFramePacing(FramePacingMode value) {
    return value == FramePacingMode::LOWEST_LATENCY ||
           value == FramePacingMode::BALANCED ||
           value == FramePacingMode::SMOOTHEST_VIDEO;
}

bool isValidUpscaling(UpscalingMode value) {
    return value == UPSCALING_OFF || value == UPSCALING_METALFX ||
           value == UPSCALING_FSR1;
}
}

const ResolvedStreamSettings* activeResolvedStreamSettings() {
    return activeSettings;
}

void setActiveResolvedStreamSettings(const ResolvedStreamSettings* settings) {
    activeSettings = settings;
}

NetworkConnectionType StreamProfileResolver::currentNetworkConnectionType() {
#ifdef __SWITCH__
    NifmInternetConnectionType type;
    NifmInternetConnectionStatus status;
    u32 wifiStrength = 0;
    if (R_FAILED(nifmGetInternetConnectionStatus(&type, &wifiStrength,
                                                  &status)) ||
        status != NifmInternetConnectionStatus_Connected) {
        return NetworkConnectionType::UNKNOWN;
    }

    if (type == NifmInternetConnectionType_WiFi)
        return NetworkConnectionType::WIFI;
    if (type == NifmInternetConnectionType_Ethernet)
        return NetworkConnectionType::ETHERNET;
#endif
    return NetworkConnectionType::UNKNOWN;
}

std::optional<StreamProfileContext> StreamProfileResolver::resolveContext(
    const Host& host, const std::string& activeAddress,
    NetworkConnectionType connectionType) {
    if (activeAddress.empty())
        return std::nullopt;

    if (!host.remoteAddress.empty() && host.remoteAddress != host.address &&
        activeAddress == host.remoteAddress) {
        return StreamProfileContext::Remote;
    }

    if (host.address.empty() || activeAddress != host.address)
        return std::nullopt;

    if (connectionType == NetworkConnectionType::WIFI)
        return StreamProfileContext::LocalWifi;
    if (connectionType == NetworkConnectionType::ETHERNET)
        return StreamProfileContext::LocalEthernet;
    return std::nullopt;
}

ResolvedStreamSettings StreamProfileResolver::globalDefaults() {
    const auto& settings = Settings::instance();
    ResolvedStreamSettings resolved;
    resolved.resolution = settings.resolution();
    resolved.nativeResolutionScale = settings.native_resolution_scale();
    resolved.fps = settings.fps();
    resolved.bitrate = settings.bitrate();
    resolved.videoCodec = validatedVideoCodec(settings.video_codec());
    resolved.requestHdr = settings.request_hdr();
    resolved.framePacingMode = settings.frame_pacing_mode();
    resolved.upscalingMode = settings.upscaling_mode();
    resolved.dithering = settings.dithering();
    resolved.ditheringStrength = settings.dithering_strength();
    resolved.rcas = settings.rcas();
    resolved.rcasStrength = settings.rcas_strength();
    return resolved;
}

ResolvedStreamSettings StreamProfileResolver::resolve(
    const Host& host, const std::string& activeAddress,
    NetworkConnectionType connectionType) {
    const auto context = resolveContext(host, activeAddress, connectionType);
    if (!context)
        return globalDefaults();
    return resolve(host, *context);
}

ResolvedStreamSettings StreamProfileResolver::resolve(
    const Host& host, StreamProfileContext context) {
    ResolvedStreamSettings resolved = globalDefaults();
    resolved.context = context;

    const auto it = host.streamProfiles.find(context);
    if (it == host.streamProfiles.end() || !it->second.enabled)
        return resolved;

    const StreamProfile& profile = it->second;
    if (profile.resolution && isValidResolution(*profile.resolution))
        resolved.resolution = *profile.resolution;
    if (profile.nativeResolutionScale && isValidScale(*profile.nativeResolutionScale))
        resolved.nativeResolutionScale = *profile.nativeResolutionScale;
    if (profile.fps && isValidFps(*profile.fps))
        resolved.fps = *profile.fps;
    if (profile.bitrate && *profile.bitrate >= 500 && *profile.bitrate <= 150000)
        resolved.bitrate = *profile.bitrate;
    if (profile.videoCodec && (*profile.videoCodec == H264 ||
                               *profile.videoCodec == H265 ||
                               *profile.videoCodec == AV1))
        resolved.videoCodec = validatedVideoCodec(*profile.videoCodec);
    if (profile.requestHdr)
        resolved.requestHdr = *profile.requestHdr;
    if (profile.framePacingMode && isValidFramePacing(*profile.framePacingMode))
        resolved.framePacingMode = *profile.framePacingMode;
    if (profile.upscalingMode && isValidUpscaling(*profile.upscalingMode))
        resolved.upscalingMode = *profile.upscalingMode;
    if (profile.dithering)
        resolved.dithering = *profile.dithering;
    if (profile.ditheringStrength && *profile.ditheringStrength >= 1.0f &&
        *profile.ditheringStrength <= 10.0f)
        resolved.ditheringStrength = *profile.ditheringStrength;
    if (profile.rcas)
        resolved.rcas = *profile.rcas;
    if (profile.rcasStrength && *profile.rcasStrength >= 0.0f &&
        *profile.rcasStrength <= 1.0f)
        resolved.rcasStrength = *profile.rcasStrength;
#ifndef SUPPORT_HDR
    resolved.requestHdr = false;
#endif
#ifndef SUPPORT_UPSCALING
    resolved.upscalingMode = UPSCALING_OFF;
    resolved.dithering = false;
    resolved.rcas = false;
#endif
    resolved.profileApplied = true;
    return resolved;
}

StreamProfile StreamProfileResolver::profileFrom(
    const ResolvedStreamSettings& settings) {
    StreamProfile profile;
    profile.enabled = true;
    profile.resolution = settings.resolution;
    profile.nativeResolutionScale = settings.nativeResolutionScale;
    profile.fps = settings.fps;
    profile.bitrate = settings.bitrate;
    profile.videoCodec = settings.videoCodec;
    profile.requestHdr = settings.requestHdr;
    profile.framePacingMode = settings.framePacingMode;
    profile.upscalingMode = settings.upscalingMode;
    profile.dithering = settings.dithering;
    profile.ditheringStrength = settings.ditheringStrength;
    profile.rcas = settings.rcas;
    profile.rcasStrength = settings.rcasStrength;
    return profile;
}

std::string StreamProfileResolver::contextName(StreamProfileContext context) {
    switch (context) {
    case StreamProfileContext::LocalWifi:
        return "stream_profiles/local_wifi"_i18n;
    case StreamProfileContext::LocalEthernet:
        return "stream_profiles/local_ethernet"_i18n;
    case StreamProfileContext::Remote:
        return "stream_profiles/remote"_i18n;
    }
    return {};
}

std::string StreamProfileResolver::summary(
    const ResolvedStreamSettings& settings) {
    const std::string resolution = settings.resolution == -1
                                       ? "settings/resolution_native"_i18n
                                       : std::to_string(settings.resolution) + "p";
    std::ostringstream bitrate;
    bitrate << std::lround(settings.bitrate / 1000.0f) << " Mbps";
    return resolution + " · " + std::to_string(settings.fps) + " FPS · " +
           bitrate.str();
}
