#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
}

namespace ffmpeg::decoder {

#if defined(_WIN32) && defined(USE_D3D11_RENDERER)
struct D3D11State {
	bool zeroCopyActive = false;
	bool deviceSharedWithRenderer = false;
	const char* unavailableReason = nullptr;
	enum AVPixelFormat hwPixelFormat = AV_PIX_FMT_NONE;
	enum AVPixelFormat hwSurfaceFormat = AV_PIX_FMT_NONE;
};

void resetD3D11State(D3D11State& state);
void prepareD3D11Setup(D3D11State& state, int video_format);
int initializeD3D11HardwareDevice(D3D11State& state, AVBufferRef*& hw_device_ctx);
int configureD3D11FramesContext(D3D11State& state, AVCodecContext* avctx, enum AVPixelFormat hwPixelFormat);
void configureD3D11DecoderContext(D3D11State& state, AVCodecContext* decoderContext);
void logD3D11HardwareInitFailure(const D3D11State& state, const char* error);
bool useD3D11ZeroCopyHolder(const D3D11State& state);
#endif

#if defined(USE_METAL_RENDERER)
bool useMetalDirectHardwareFrames();
#endif

#if defined(PLATFORM_SWITCH) && defined(BOREALIS_USE_DEKO3D)
AVHWDeviceType deko3dHardwareDeviceType();
int configureDeko3DDecoderContext(AVCodecContext* decoderContext, bool hw_decode_active);
bool useDeko3DZeroCopyHolder(bool hw_decode_active);
bool useDeko3DDirectHardwareFrames();
#endif

} // namespace ffmpeg::decoder