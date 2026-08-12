#pragma once

#include "Settings.hpp"

struct HardwareVideoCodecSupport {
    bool av1Main8 = false;
    bool av1Main10 = false;
    const char* av1Backend = "none";
};

const HardwareVideoCodecSupport& hardwareVideoCodecSupport();
bool isAv1HardwareDecodingAvailable();
VideoCodec validatedVideoCodec(VideoCodec requestedCodec);

