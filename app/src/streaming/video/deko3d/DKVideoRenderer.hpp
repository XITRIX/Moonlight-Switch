#if defined(__SWITCH__) && defined(BOREALIS_USE_DEKO3D)

#pragma once
#include "IVideoRenderer.hpp"
#include <deko3d.hpp>

#include <borealis.hpp>
#include <nanovg/framework/CShader.h>
#include <nanovg/framework/CExternalImage.h>
#include <nanovg/framework/CDescriptorSet.h>
#include <optional>

class DKVideoRenderer : public IVideoRenderer {
  public:
    DKVideoRenderer();
    ~DKVideoRenderer();

    void draw(NVGcontext* vg, int width, int height, AVFrame* frame) override;

    VideoRenderStats* video_render_stats() override;

  private:
    void checkAndInitialize(int width, int height, AVFrame* frame);

    bool m_is_initialized = false;

    dk::Device dev;
    dk::Queue queue;

    std::optional<CMemPool> pool_images;
    std::optional<CMemPool> pool_code;
    std::optional<CMemPool> pool_data;

    dk::UniqueCmdBuf cmdbuf;

    CDescriptorSet<4096U> *imageDescriptorSet;
    // CDescriptorSet<1> samplerDescriptorSet;

    CShader vertexShader;
    CShader fragmentShader;

    CMemPool::Handle vertexBuffer;

    dk::ImageLayout lumaMappingLayout; 
    dk::ImageLayout chromaMappingLayout; 
    dk::MemBlock mappingMemblock;

    dk::Image luma;
    dk::Image chroma;

    dk::ImageDescriptor lumaDesc;
    dk::ImageDescriptor chromaDesc;

    int lumaTextureId = 0;
    int chromaTextureId = 0;

    VideoRenderStats m_video_render_stats = {};
};

#endif // __SWITCH__