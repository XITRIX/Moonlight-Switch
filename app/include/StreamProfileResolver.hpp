#pragma once

#include "Settings.hpp"
#include <optional>
#include <string>

enum class NetworkConnectionType {
    UNKNOWN,
    WIFI,
    ETHERNET,
};

struct ResolvedStreamSettings {
    int resolution = 720;
    int nativeResolutionScale = 100;
    int fps = 60;
    int bitrate = 10000;
    VideoCodec videoCodec = H265;
    bool requestHdr = false;
    FramePacingMode framePacingMode = FramePacingMode::BALANCED;
    UpscalingMode upscalingMode = UPSCALING_OFF;
    bool dithering = false;
    float ditheringStrength = 3.0f;
    bool rcas = true;
    float rcasStrength = 0.2f;
    std::optional<StreamProfileContext> context;
    bool profileApplied = false;
};

class StreamProfileResolver {
  public:
    static NetworkConnectionType currentNetworkConnectionType();
    static std::optional<StreamProfileContext>
    resolveContext(const Host& host, const std::string& activeAddress,
                   NetworkConnectionType connectionType);
    static ResolvedStreamSettings
    resolve(const Host& host, const std::string& activeAddress,
            NetworkConnectionType connectionType);
    static ResolvedStreamSettings resolve(const Host& host,
                                          StreamProfileContext context);
    static ResolvedStreamSettings globalDefaults();
    static StreamProfile profileFrom(const ResolvedStreamSettings& settings);
    static std::string contextName(StreamProfileContext context);
    static std::string summary(const ResolvedStreamSettings& settings);
};

// The streaming pipeline can read the immutable settings snapshot without
// mutating Settings. It is set for the lifetime of the active session.
const ResolvedStreamSettings* activeResolvedStreamSettings();
void setActiveResolvedStreamSettings(const ResolvedStreamSettings* settings);
