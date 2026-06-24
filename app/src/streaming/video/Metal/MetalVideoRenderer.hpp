#pragma once
#if defined(USE_METAL_RENDERER)

#include "IVideoRenderer.hpp"
#include <SDL2/SDL.h>

class MetalVideoRenderer : public IVideoRenderer {
public:
    MetalVideoRenderer();
    ~MetalVideoRenderer();

    bool waitToRender();
    void draw(NVGcontext* vg, int width, int height, AVFrame* frame, int imageFormat) override;
    VideoRenderStats* video_render_stats() override;
private:
    struct MetalRendererState;

    bool updateColorSpaceForFrame(AVFrame* frame);
    bool updateVideoRegionSizeForFrame(AVFrame* frame);
    bool initialize(int imageFormat);
#ifdef SUPPORT_UPSCALING
    bool shouldUseUpscaling() const;
    bool shouldUseMetalFxUpscaling() const;
    bool shouldUseFsrUpscaling() const;
    bool shouldUseDithering() const;
    bool shouldUseRcas() const;
    bool ensureUpscalingResources(AVFrame* frame);
    void updateEasuParams(int inputWidth, int inputHeight, int outputWidth, int outputHeight);
    void updatePostProcessParams(bool ditheringEnabled);
    void updateRcasParams();
    void releaseUpscalingResources();
#endif

    VideoRenderStats m_video_render_stats_progress = {};
    VideoRenderStats m_video_render_stats_cache = {};
    uint64_t m_stats_time_accumulator = 0;
    SDL_Window* m_Window;
//    SDL_MetalView m_MetalView;

    bool initialized = false;
    int m_LastColorSpace = -1;
    bool m_LastFullRange = false;
    int m_LastFrameWidth = -1;
    int m_LastFrameHeight = -1;
    int m_LastDrawableWidth = -1;
    int m_LastDrawableHeight = -1;
    int m_LastVideoRegionWidth = -1;
    int m_LastVideoRegionHeight = -1;
    MetalRendererState* m_State = nullptr;
};

#endif // USE_METAL_RENDERER
