#include "FFmpegVideoDecoderPlatformHelpers.hpp"

#include "borealis.hpp"

namespace ffmpeg::decoder {

#if defined(PLATFORM_SWITCH) && defined(BOREALIS_USE_DEKO3D)
AVHWDeviceType deko3dHardwareDeviceType() {
    return AV_HWDEVICE_TYPE_NVTEGRA;
}

int configureDeko3DDecoderContext(AVCodecContext* decoderContext, bool hw_decode_active) {
    if (!hw_decode_active) {
        brls::Logger::error("FFmpeg: Deko3D rendering requires NVTEGRA hardware decoding");
        return -1;
    }

    decoderContext->pix_fmt = AV_PIX_FMT_NVTEGRA;
    return 0;
}

bool useDeko3DZeroCopyHolder(bool hw_decode_active) {
    return hw_decode_active;
}

bool useDeko3DDirectHardwareFrames() {
    return true;
}
#endif

} // namespace ffmpeg::decoder