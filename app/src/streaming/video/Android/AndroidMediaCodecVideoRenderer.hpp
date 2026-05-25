#pragma once

#ifdef PLATFORM_ANDROID

#include "GLVideoRenderer.hpp"

class AndroidMediaCodecVideoRenderer : public IVideoRenderer {
  public:
    AndroidMediaCodecVideoRenderer() = default;
    ~AndroidMediaCodecVideoRenderer() override = default;

    void draw(NVGcontext* vg, int width, int height, AVFrame* frame, int imageFormat) override;
    VideoRenderStats* video_render_stats() override;

  private:
    bool isNewMediaCodecFrame(const AVFrame* frame) const;
    void markPresentedFrame(const AVFrame* frame);
    void recordPresentation(uint64_t renderTimeMs);

    GLVideoRenderer m_glRenderer;
    VideoRenderStats m_videoRenderStatsProgress = {};
    VideoRenderStats m_videoRenderStatsCache = {};
    uint64_t m_statsTimeAccumulator = 0;
    AVBufferRef* m_lastPresentedBufferRef = nullptr;
    uint8_t* m_lastPresentedBufferHandle = nullptr;
    int64_t m_lastPresentedPts = AV_NOPTS_VALUE;
    int64_t m_lastPresentedBestEffortTimestamp = AV_NOPTS_VALUE;
    bool m_usingHardwareFrames = false;
};

#endif