#if defined(__SWITCH__) && defined(BOREALIS_USE_DEKO3D)

#pragma once
#include "IVideoRenderer.hpp"
#include <deko3d.hpp>

#include <glm/mat4x4.hpp>

#include <borealis.hpp>
#include <borealis/platforms/switch/switch_video.hpp>
#include <nanovg/framework/CCmdMemRing.h>
#include <nanovg/framework/CShader.h>
#include <optional>
#include <vector>

class DKVideoRenderer : public IVideoRenderer {
  public:
    DKVideoRenderer();
    ~DKVideoRenderer();

    void draw(NVGcontext* vg, int width, int height, AVFrame* frame, int imageFormat) override;

    VideoRenderStats* video_render_stats() override;

  private:
    void checkAndInitialize(int width, int height, AVFrame* frame);
    void updateRenderState(int width, int height, AVFrame* frame);
    void updateFrameMapping(AVFrame* frame);
    void updateFrameLayouts();
    void recordStaticCommands(AVFrame* frame);
    void releaseImageSlots();

    bool m_is_initialized = false;

    int m_frame_width = 0;
    int m_frame_height = 0;
    int m_screen_width = 0;
    int m_screen_height = 0;

    brls::SwitchVideoContext* vctx = nullptr;
    dk::Device dev;
    dk::Queue queue;

    // std::optional<CMemPool> pool_images;
    std::optional<CMemPool> pool_code;
    std::optional<CMemPool> pool_data;

    dk::UniqueCmdBuf cmdbuf;
    dk::UniqueCmdBuf updateCmdbuf;
    CCmdMemRing<brls::FRAMEBUFFERS_COUNT> updateCmdMemRing;
    DkCmdList cmdlist = 0;

    CShader vertexShader;
    CShader fragmentShader;

    CMemPool::Handle vertexBuffer;
    CMemPool::Handle transformUniformBuffer;

    dk::ImageLayout lumaMappingLayout;
    dk::ImageLayout chromaMappingLayout;

    struct FrameMapping {
        uint32_t handle = 0;
        void* cpuAddr = nullptr;
        uint32_t size = 0;
        uint32_t chromaOffset = 0;
        dk::UniqueMemBlock memblock;
        dk::Image luma;
        dk::Image chroma;
        dk::ImageDescriptor lumaDesc;
        dk::ImageDescriptor chromaDesc;
    };

    std::vector<FrameMapping> frameMappings;
    int currentMappingIndex = -1;

    int lumaTextureId = -1;
    int chromaTextureId = -1;
    int m_color_space = -1;
    bool m_color_full = false;

    VideoRenderStats m_video_render_stats = {};
};

#endif // __SWITCH__
