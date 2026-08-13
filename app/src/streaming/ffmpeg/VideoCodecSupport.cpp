#include "VideoCodecSupport.hpp"

#include "FFmpegVideoDecoderPlatformHelpers.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
}

#if defined(PLATFORM_APPLE)
#include <VideoToolbox/VideoToolbox.h>
#endif

namespace {

bool decoderSupportsHardwareType(const AVCodec* decoder,
                                 AVHWDeviceType deviceType) {
    if (decoder == nullptr) {
        return false;
    }

    for (int index = 0;; index++) {
        const AVCodecHWConfig* config = avcodec_get_hw_config(decoder, index);
        if (config == nullptr) {
            return false;
        }

        if (config->device_type == deviceType &&
            (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) != 0) {
            return true;
        }
    }
}

bool canCreateHardwareDevice(AVHWDeviceType deviceType) {
    AVBufferRef* device = nullptr;
    const int error =
        av_hwdevice_ctx_create(&device, deviceType, nullptr, nullptr, 0);
    av_buffer_unref(&device);
    return error >= 0;
}

HardwareVideoCodecSupport probeHardwareVideoCodecSupport() {
    HardwareVideoCodecSupport support;

#if defined(PLATFORM_SWITCH) || defined(__PSV__)
    // Tegra X1 NVDEC and the Vita decoder do not support AV1.
    return support;
#elif defined(PLATFORM_ANDROID)
    if (avcodec_find_decoder_by_name("av1_mediacodec") == nullptr) {
        return support;
    }

    const int androidSupport = ffmpeg::decoder::androidAv1DecoderSupport();
    support.av1Main8 = (androidSupport & 0x1) != 0;
    support.av1Main10 = (androidSupport & 0x2) != 0;
    support.av1Backend = support.av1Main8 ? "mediacodec" : "none";
    return support;
#elif defined(PLATFORM_APPLE)
    const AVCodec* decoder = avcodec_find_decoder(AV_CODEC_ID_AV1);
    const bool ffmpegSupportsVideoToolbox =
        decoderSupportsHardwareType(decoder, AV_HWDEVICE_TYPE_VIDEOTOOLBOX);
    const bool deviceSupportsAv1 =
        VTIsHardwareDecodeSupported(static_cast<CMVideoCodecType>('av01'));
    support.av1Main8 = ffmpegSupportsVideoToolbox && deviceSupportsAv1;
    support.av1Main10 = support.av1Main8;
    support.av1Backend = support.av1Main8 ? "videotoolbox" : "none";
    return support;
#else
    const AVCodec* decoder = avcodec_find_decoder(AV_CODEC_ID_AV1);
    if (decoder == nullptr) {
        return support;
    }

#if defined(_WIN32) && defined(USE_D3D11_RENDERER)
    if (decoderSupportsHardwareType(decoder, AV_HWDEVICE_TYPE_D3D11VA) &&
        canCreateHardwareDevice(AV_HWDEVICE_TYPE_D3D11VA)) {
        support.av1Main8 = true;
        support.av1Main10 = true;
        support.av1Backend = "d3d11va";
    }
#elif defined(__linux__) && defined(PLATFORM_DESKTOP)
    constexpr AVHWDeviceType deviceTypes[] = {
        AV_HWDEVICE_TYPE_VAAPI,
        AV_HWDEVICE_TYPE_CUDA,
        AV_HWDEVICE_TYPE_VDPAU,
    };
    for (AVHWDeviceType deviceType : deviceTypes) {
        if (!decoderSupportsHardwareType(decoder, deviceType) ||
            !canCreateHardwareDevice(deviceType)) {
            continue;
        }

        support.av1Main8 = true;
        support.av1Main10 = true;
        support.av1Backend = av_hwdevice_get_type_name(deviceType);
        break;
    }
#endif
    return support;
#endif
}

} // namespace

const HardwareVideoCodecSupport& hardwareVideoCodecSupport() {
    static const HardwareVideoCodecSupport support =
        probeHardwareVideoCodecSupport();
    return support;
}

bool isAv1HardwareDecodingAvailable() {
    return Settings::instance().use_hw_decoding() &&
           hardwareVideoCodecSupport().av1Main8;
}

VideoCodec validatedVideoCodec(VideoCodec requestedCodec) {
    switch (requestedCodec) {
    case H264:
        return H264;
    case H265:
#if defined(__PSV__)
        return H264;
#else
        return H265;
#endif
    case AV1:
        if (isAv1HardwareDecodingAvailable()) {
            return AV1;
        }
#if defined(__PSV__)
        return H264;
#else
        return H265;
#endif
    default:
#if defined(__PSV__)
        return H264;
#else
        return H265;
#endif
    }
}

