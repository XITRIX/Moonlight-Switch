#include "FFmpegVideoDecoderPlatformHelpers.hpp"

#include "borealis.hpp"
#include "Settings.hpp"
#include "../VideoTrace.hpp"
#include <atomic>
#include <chrono>
#include <thread>
#if defined(PLATFORM_SWITCH) && !defined(__SWITCH2__)
#include <deko3d.h>
#include <switch.h>
extern "C" DkSwapchain __real_dkSwapchainCreate(const DkSwapchainMaker*);
extern "C" DkSwapchain __wrap_dkSwapchainCreate(const DkSwapchainMaker* maker) {
    DkSwapchainMaker two = *maker;
    if (two.numImages > 2) two.numImages = 2;
    // Borealis may retain a third allocation, but only two images participate
    // in presentation. Avoid changing the dependency's class ABI.
    return __real_dkSwapchainCreate(&two);
}
extern "C" {
#include <Limelight.h>
}
extern "C" int __real_dkQueueAcquireImage(DkQueue, DkSwapchain);
extern "C" void __real_dkQueuePresentImage(DkQueue, DkSwapchain, int);
extern "C" int __wrap_dkQueueAcquireImage(DkQueue queue, DkSwapchain swapchain) {
    // Swap interval zero removes the compositor's backpressure. Without a
    // CPU cadence, the UI can submit thousands of frames before video arrives.
    // Pace to the Switch display rate while leaving presentation unsynchronized.
    if (!Settings::instance().video_vsync()) {
        static uint64_t deadline = 0;
        const uint64_t now = LiGetMicroseconds();
        if (!deadline || now > deadline + 50000) deadline = now;
        if (deadline > now) svcSleepThread(static_cast<int64_t>(deadline - now) * 1000);
        deadline += 16667;
    }
    const int slot = __real_dkQueueAcquireImage(queue, swapchain);
    return slot;
}
extern "C" void __wrap_dkQueuePresentImage(DkQueue queue, DkSwapchain swapchain, int slot) {
    __real_dkQueuePresentImage(queue, swapchain, slot);
}
extern "C" {
#include <libavutil/hwcontext_nvtegra.h>
}
static std::atomic<bool> nvdecMaximum{false};
extern "C" int __real_av_nvtegra_dfs_init(AVHWDeviceContext*, AVNVTegraChannel*, int, int, double);
extern "C" int __real_av_nvtegra_dfs_update(AVHWDeviceContext*, AVNVTegraChannel*, int, int);
extern "C" int __wrap_av_nvtegra_dfs_init(AVHWDeviceContext* ctx, AVNVTegraChannel* channel, int width, int height, double fps) {
    const int result = __real_av_nvtegra_dfs_init(ctx, channel, width, height, fps);
    auto* device = static_cast<AVNVTegraDeviceContext*>(ctx->hwctx);
    if (channel == &device->nvdec_channel) {
        nvdecMaximum = result >= 0 && Settings::instance().nvdec_max_performance();
        brls::Logger::info("LL7 NVDEC: mode={} initial_clock={} Hz init_result={}",
            nvdecMaximum ? "maximum (system limits)" : "battery (dynamic)", channel->clock, result);
    }
    return result;
}
extern "C" int __wrap_av_nvtegra_dfs_update(AVHWDeviceContext* ctx, AVNVTegraChannel* channel, int bytes, int cycles) {
    auto* device = static_cast<AVNVTegraDeviceContext*>(ctx->hwctx);
    // DFS init already requests the system-clamped maximum. Keep it for this
    // stream; leave uninit untouched so the library releases the clock request.
    if (channel == &device->nvdec_channel && nvdecMaximum.load()) return 0;
    return __real_av_nvtegra_dfs_update(ctx, channel, bytes, cycles);
}
#endif

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
