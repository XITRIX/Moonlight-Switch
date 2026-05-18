#include "FFmpegVideoDecoderPlatformHelpers.hpp"

#include <algorithm>
#include <Limelight.h>

#include "Settings.hpp"
#include "borealis.hpp"

#if defined(_WIN32) && defined(USE_D3D11_RENDERER)
extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
#include <libavutil/pixdesc.h>
}

#include <borealis/core/application.hpp>
#include <borealis/platforms/driver/d3d11.hpp>
#include <borealis/platforms/sdl/sdl_video.hpp>

namespace ffmpeg::decoder {

namespace {

enum AVPixelFormat selectD3D11HardwareFormat(AVCodecContext* avctx, const enum AVPixelFormat* pix_fmts) {
    auto* state = static_cast<D3D11State*>(avctx->opaque);
    if (state == nullptr) {
        return avcodec_default_get_format(avctx, pix_fmts);
    }

    state->zeroCopyActive = false;

    for (const enum AVPixelFormat* format = pix_fmts; *format != AV_PIX_FMT_NONE; ++format) {
        if (*format != state->hwPixelFormat) {
            continue;
        }

        const int err = configureD3D11FramesContext(*state, avctx, *format);
        if (err >= 0) {
            state->zeroCopyActive = state->deviceSharedWithRenderer;
            return *format;
        }

        char error[AV_ERROR_MAX_STRING_SIZE] = {0};
        brls::Logger::warning("FFmpeg: D3D11 zero-copy surface setup failed - {}",
            av_make_error_string(error, AV_ERROR_MAX_STRING_SIZE, err));

        state->hwPixelFormat = AV_PIX_FMT_NONE;
        av_buffer_unref(&avctx->hw_frames_ctx);
        av_buffer_unref(&avctx->hw_device_ctx);
        break;
    }

    if (state->hwSurfaceFormat != AV_PIX_FMT_NONE) {
        for (const enum AVPixelFormat* format = pix_fmts; *format != AV_PIX_FMT_NONE; ++format) {
            if (*format == state->hwSurfaceFormat) {
                brls::Logger::warning("FFmpeg: Falling back from D3D11 zero-copy to software pixel format {}",
                    av_get_pix_fmt_name(*format));
                return *format;
            }
        }
    }

    for (const enum AVPixelFormat* format = pix_fmts; *format != AV_PIX_FMT_NONE; ++format) {
        const AVPixFmtDescriptor* descriptor = av_pix_fmt_desc_get(*format);
        if (descriptor != nullptr && (descriptor->flags & AV_PIX_FMT_FLAG_HWACCEL) == 0) {
            brls::Logger::warning("FFmpeg: Falling back from D3D11 zero-copy to software pixel format {}",
                av_get_pix_fmt_name(*format));
            return *format;
        }
    }

    return avcodec_default_get_format(avctx, pix_fmts);
}

} // namespace

void resetD3D11State(D3D11State& state) {
    state.zeroCopyActive = false;
    state.deviceSharedWithRenderer = false;
    state.unavailableReason = nullptr;
    state.hwPixelFormat = AV_PIX_FMT_NONE;
    state.hwSurfaceFormat = AV_PIX_FMT_NONE;
}

void prepareD3D11Setup(D3D11State& state, int video_format) {
    state.hwPixelFormat = AV_PIX_FMT_D3D11;
    state.hwSurfaceFormat = (video_format & VIDEO_FORMAT_MASK_10BIT) ? AV_PIX_FMT_P010 : AV_PIX_FMT_NV12;
}

int initializeD3D11HardwareDevice(D3D11State& state, AVBufferRef*& hw_device_ctx) {
    state.deviceSharedWithRenderer = false;
    state.unavailableReason = nullptr;

    auto* videoContext = static_cast<brls::SDLVideoContext*>(brls::Application::getPlatform()->getVideoContext());
    if (videoContext == nullptr) {
        brls::Logger::error("FFmpeg: SDL video context is unavailable for D3D11 decoding");
        return AVERROR(EINVAL);
    }

    auto* d3d11Context = videoContext->getD3D11Context();
    if (d3d11Context == nullptr) {
        brls::Logger::error("FFmpeg: Borealis D3D11 context is unavailable for hardware decoding");
        return AVERROR(EINVAL);
    }

    ID3D11Device* device = d3d11Context->getDevice();
    ID3D11DeviceContext* deviceContext = d3d11Context->getDeviceContext();
    if (device == nullptr || deviceContext == nullptr) {
        brls::Logger::error("FFmpeg: Failed to acquire shared D3D11 device for decoding");
        return AVERROR(EINVAL);
    }

    hw_device_ctx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
    if (hw_device_ctx == nullptr) {
        brls::Logger::error("FFmpeg: Couldn't allocate D3D11 hardware device context");
        return AVERROR(ENOMEM);
    }

    auto* deviceContextRef = reinterpret_cast<AVHWDeviceContext*>(hw_device_ctx->data);
    auto* d3d11DeviceContext = reinterpret_cast<AVD3D11VADeviceContext*>(deviceContextRef->hwctx);

    device->AddRef();
    deviceContext->AddRef();
    d3d11DeviceContext->device = device;
    d3d11DeviceContext->device_context = deviceContext;

    ID3D11VideoDevice* videoDevice = nullptr;
    const HRESULT videoHr = device->QueryInterface(IID_PPV_ARGS(&videoDevice));
    if (FAILED(videoHr) || videoDevice == nullptr) {
        av_buffer_unref(&hw_device_ctx);
        state.unavailableReason = "shared renderer device does not expose D3D11 video decode support";
        return AVERROR(ENOSYS);
    }

    const UINT decoderProfileCount = videoDevice->GetVideoDecoderProfileCount();
    videoDevice->Release();

    if (decoderProfileCount == 0) {
        av_buffer_unref(&hw_device_ctx);
        state.unavailableReason = "shared renderer device reports no video decoder profiles";
        return AVERROR(ENOSYS);
    }

    const int err = av_hwdevice_ctx_init(hw_device_ctx);
    if (err < 0) {
        av_buffer_unref(&hw_device_ctx);
    } else {
        state.deviceSharedWithRenderer = true;
    }

    return err;
}

int configureD3D11FramesContext(D3D11State& state, AVCodecContext* avctx, enum AVPixelFormat hwPixelFormat) {
    if (avctx->hw_device_ctx == nullptr) {
        return AVERROR(EINVAL);
    }

    AVBufferRef* framesRef = nullptr;
    int err = avcodec_get_hw_frames_parameters(avctx, avctx->hw_device_ctx, hwPixelFormat, &framesRef);
    if (err < 0) {
        return err;
    }

    auto* framesContext = reinterpret_cast<AVHWFramesContext*>(framesRef->data);
    auto* d3d11FramesContext = reinterpret_cast<AVD3D11VAFramesContext*>(framesContext->hwctx);

    framesContext->sw_format = state.hwSurfaceFormat;
    framesContext->initial_pool_size = std::max(
        framesContext->initial_pool_size,
        Settings::instance().frames_queue_size() + avctx->thread_count + 2);
    d3d11FramesContext->BindFlags |= D3D11_BIND_SHADER_RESOURCE;

    err = av_hwframe_ctx_init(framesRef);
    if (err >= 0) {
        av_buffer_unref(&avctx->hw_frames_ctx);
        avctx->hw_frames_ctx = av_buffer_ref(framesRef);
        if (avctx->hw_frames_ctx == nullptr) {
            err = AVERROR(ENOMEM);
        }
    }

    av_buffer_unref(&framesRef);
    return err;
}

void configureD3D11DecoderContext(D3D11State& state, AVCodecContext* decoderContext) {
    decoderContext->opaque = &state;
    decoderContext->get_format = &selectD3D11HardwareFormat;
    decoderContext->extra_hw_frames = std::max(Settings::instance().frames_queue_size(), 1);
}

void logD3D11HardwareInitFailure(const D3D11State& state, const char* error) {
    if (state.unavailableReason != nullptr) {
        brls::Logger::info("FFmpeg: Native D3D11 rendering is active, but {}. Using software video decode on this machine.", state.unavailableReason);
    } else {
        brls::Logger::warning("FFmpeg: Hardware D3D11 decode unavailable on the shared renderer device; continuing with software decode ({})", error);
    }
}

bool useD3D11ZeroCopyHolder(const D3D11State& state) {
    return state.zeroCopyActive;
}

} // namespace ffmpeg::decoder

#endif