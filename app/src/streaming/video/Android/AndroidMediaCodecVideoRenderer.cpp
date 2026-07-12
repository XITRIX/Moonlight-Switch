#ifdef PLATFORM_ANDROID

#include "AndroidMediaCodecVideoRenderer.hpp"

extern "C" {
#include <libavcodec/mediacodec.h>
#include <libavutil/error.h>
}

#include <Limelight.h>

#include "borealis.hpp"

bool AndroidMediaCodecVideoRenderer::isNewMediaCodecFrame(const AVFrame* frame) const {
    return frame->buf[0] != m_lastPresentedBufferRef ||
           frame->data[3] != m_lastPresentedBufferHandle ||
           frame->pts != m_lastPresentedPts ||
           frame->best_effort_timestamp != m_lastPresentedBestEffortTimestamp;
}

void AndroidMediaCodecVideoRenderer::markPresentedFrame(const AVFrame* frame) {
    m_lastPresentedBufferRef = frame->buf[0];
    m_lastPresentedBufferHandle = frame->data[3];
    m_lastPresentedPts = frame->pts;
    m_lastPresentedBestEffortTimestamp = frame->best_effort_timestamp;
}

void AndroidMediaCodecVideoRenderer::recordPresentation(uint64_t renderTimeMs) {
    if (!m_videoRenderStatsProgress.rendered_frames) {
        m_videoRenderStatsProgress.measurement_start_timestamp = LiGetMillis();
    }

    m_videoRenderStatsProgress.total_render_time += renderTimeMs;
    m_videoRenderStatsProgress.rendered_frames++;

    const int timeIntervalMs = 200;
    const uint64_t statsNow = LiGetMillis();
    if (statsNow - m_videoRenderStatsProgress.measurement_start_timestamp <
        timeIntervalMs) {
        return;
    }

    m_videoRenderStatsCache = m_videoRenderStatsProgress;
    m_videoRenderStatsProgress = {};

    const uint64_t elapsedTime =
        statsNow - m_videoRenderStatsCache.measurement_start_timestamp;
    m_videoRenderStatsCache.rendered_fps =
        elapsedTime && m_videoRenderStatsCache.rendered_frames > 1
        ? static_cast<float>(m_videoRenderStatsCache.rendered_frames - 1) /
              (static_cast<float>(elapsedTime) / 1000.0f)
        : 0.0f;
    m_videoRenderStatsCache.rendering_time =
        static_cast<float>(m_videoRenderStatsCache.total_render_time) /
        static_cast<float>(m_videoRenderStatsCache.rendered_frames);
}

void AndroidMediaCodecVideoRenderer::draw(NVGcontext* vg, int width, int height,
                                          AVFrame* frame, int imageFormat) {
    if (frame == nullptr) {
        return;
    }

    if (frame->format != AV_PIX_FMT_MEDIACODEC) {
        m_usingHardwareFrames = false;
        m_glRenderer.draw(vg, width, height, frame, imageFormat);
        return;
    }

    m_usingHardwareFrames = true;

    if (!isNewMediaCodecFrame(frame)) {
        return;
    }

    auto* buffer = reinterpret_cast<AVMediaCodecBuffer*>(frame->data[3]);
    if (buffer == nullptr) {
        brls::Logger::warning("AndroidVideoRenderer: MediaCodec frame had no presentation buffer");
        return;
    }

    const uint64_t beforeRender = LiGetMillis();
    const int err = av_mediacodec_release_buffer(buffer, 1);
    if (err < 0) {
        char error[AV_ERROR_MAX_STRING_SIZE] = {0};
        brls::Logger::warning("AndroidVideoRenderer: Failed to present MediaCodec buffer - {}",
                              av_make_error_string(error, sizeof(error), err));
        return;
    }

    markPresentedFrame(frame);
    recordPresentation(LiGetMillis() - beforeRender);
}

VideoRenderStats* AndroidMediaCodecVideoRenderer::video_render_stats() {
    if (!m_usingHardwareFrames) {
        return m_glRenderer.video_render_stats();
    }

    return &m_videoRenderStatsCache;
}

#endif
