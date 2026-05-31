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
    [[nodiscard]] bool shouldUseUpscaling() const;
#ifdef SUPPORT_UPSCALING
    [[nodiscard]] bool shouldUseDithering() const;
    [[nodiscard]] bool shouldUseRcas() const;
    [[nodiscard]] bool ensureUpscalingResources();
    void updateDitheringConstants();
    void updateRcasConstants();
    void releaseUpscalingResources();
    void submitUpscalingPresentPass();
#endif
    void releaseImageSlots();

    bool m_is_initialized = false;

    int m_frame_width = 0;
    int m_frame_height = 0;
    int m_screen_width = 0;
    int m_screen_height = 0;

    brls::SwitchVideoContext* vctx = nullptr;
    dk::Device dev;
    dk::Queue queue;

    std::optional<CMemPool> pool_code;
    std::optional<CMemPool> pool_data;
#ifdef SUPPORT_UPSCALING
    std::optional<CMemPool> pool_images;
#endif

    dk::UniqueCmdBuf cmdbuf;
    dk::UniqueCmdBuf updateCmdbuf;
    CCmdMemRing<brls::FRAMEBUFFERS_COUNT> updateCmdMemRing;
#ifdef SUPPORT_UPSCALING
    dk::UniqueCmdBuf presentCmdbuf;
    CCmdMemRing<brls::FRAMEBUFFERS_COUNT> presentCmdMemRing;
#endif
    DkCmdList cmdlist = 0;

    CShader vertexShader;
    CShader fragmentShader;
  #ifdef SUPPORT_UPSCALING
    CShader upscalingFragmentShader;
    CShader rcasFragmentShader;
    CShader upscalingPassFragmentShader;
  #endif

    CMemPool::Handle vertexBuffer;
    CMemPool::Handle transformUniformBuffer;
  #ifdef SUPPORT_UPSCALING
    CMemPool::Handle ditheringUniformBuffer;
    CMemPool::Handle rcasUniformBuffer;
    DkFence upscalingFence = {};
  #endif

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
  #ifdef SUPPORT_UPSCALING
    CMemPool::Handle upscalingTargetHandle;
    dk::ImageLayout upscalingTargetLayout;
    dk::Image upscalingTargetImage;
    dk::ImageDescriptor upscalingTargetDesc;
    int upscalingTextureId = -1;
    CMemPool::Handle rcasTargetHandle;
    dk::ImageLayout rcasTargetLayout;
    dk::Image rcasTargetImage;
    dk::ImageDescriptor rcasTargetDesc;
    int rcasTextureId = -1;
    int m_upscaling_target_width = 0;
    int m_upscaling_target_height = 0;
  #endif
    int m_color_space = -1;
    bool m_color_full = false;
    bool m_dithering_enabled = false;
    bool m_upscaling_enabled = false;
  #ifdef SUPPORT_UPSCALING
    bool m_rcas_enabled = false;
    bool m_dithering_requested = false;
    bool m_upscaling_requested = false;
    bool m_rcas_requested = false;
    float m_dithering_strength = 3.0f;
    float m_rcas_strength = 0.2f;
  #endif

    VideoRenderStats m_video_render_stats = {};
};

#endif // __SWITCH__
