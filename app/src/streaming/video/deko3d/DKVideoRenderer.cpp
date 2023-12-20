#if defined(__SWITCH__) && defined(BOREALIS_USE_DEKO3D)

#define FF_API_AVPICTURE

#include "DKVideoRenderer.hpp"
#include <borealis/platforms/switch/switch_platform.hpp>

#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext_tx1.h>
#include <libavutil/imgutils.h>

#include <array>

namespace
{
    static constexpr unsigned StaticCmdSize = 0x10000;

    struct Vertex
    {
        float position[3];
        float uv[2];
    };

    constexpr std::array VertexAttribState =
    {
        DkVtxAttribState{ 0, 0, offsetof(Vertex, position), DkVtxAttribSize_3x32, DkVtxAttribType_Float, 0 },
        DkVtxAttribState{ 0, 0, offsetof(Vertex, uv),    DkVtxAttribSize_2x32, DkVtxAttribType_Float, 0 },
    };

    constexpr std::array VertexBufferState =
    {
        DkVtxBufferState{ sizeof(Vertex), 0 },
    };
    
    constexpr std::array QuadVertexData =
    {
        Vertex{ { -1.0f, +1.0f, 0.0f }, { 0.0f, 0.0f } },
        Vertex{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } },
        Vertex{ { +1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } },
        Vertex{ { +1.0f, +1.0f, 0.0f }, { 1.0f, 0.0f } },
    };
}

DKVideoRenderer::DKVideoRenderer() {}

DKVideoRenderer::~DKVideoRenderer() {
    // Destroy the vertex buffer (not strictly needed in this case)
    vertexBuffer.destroy();
    dkMemBlockDestroy(mappingMemblock);
}

void DKVideoRenderer::checkAndInitialize(int width, int height, AVFrame* frame) {
    if (m_is_initialized) return;
    brls::Logger::info("{}: {} / {}", __PRETTY_FUNCTION__, width, height);

    auto *vctx = (brls::SwitchVideoContext *)brls::Application::getPlatform()->getVideoContext();
    this->dev = vctx->getDeko3dDevice();
    this->queue = vctx->getQueue();

// Create the memory pools
    pool_images.emplace(dev, DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image, 16*1024*1024);
    pool_code.emplace(dev, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Code, 128*1024);
    pool_data.emplace(dev, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached, 1*1024*1024);

// Create the static command buffer and feed it freshly allocated memory
    cmdbuf = dk::CmdBufMaker{dev}.create();
    CMemPool::Handle cmdmem = pool_data->allocate(StaticCmdSize);
    cmdbuf.addMemory(cmdmem.getMemBlock(), cmdmem.getOffset(), cmdmem.getSize());

// Create the image and sampler descriptor sets
    imageDescriptorSet = vctx->getImageDescriptor();

// Load the shaders
    vertexShader.load(*pool_code, "romfs:/shaders/basic_vsh.dksh");
    fragmentShader.load(*pool_code, "romfs:/shaders/texture_fsh.dksh");

// Load the vertex buffer
    vertexBuffer = pool_data->allocate(sizeof(QuadVertexData), alignof(Vertex));
    memcpy(vertexBuffer.getCpuAddr(), QuadVertexData.data(), vertexBuffer.getSize());

// Allocate image indexes for planes
    lumaTextureId = vctx->allocateImageIndex();
    chromaTextureId = vctx->allocateImageIndex();

    brls::Logger::debug("{}: Luma texture ID {}", __PRETTY_FUNCTION__, lumaTextureId);
    brls::Logger::debug("{}: Chroma texture ID {}", __PRETTY_FUNCTION__, chromaTextureId);

    AVTX1Map *map = ff_tx1_frame_get_fbuf_map(frame);
    brls::Logger::info("{}: Map size: {} | {} | {} | {}", __PRETTY_FUNCTION__, map->map.handle, map->map.has_init, map->map.cpu_addr, map->map.size);

    dk::ImageLayoutMaker { dev }
        .setType(DkImageType_2D)
        .setFormat(DkImageFormat_R8_Unorm)
        .setDimensions(width, height, 1)
        .setFlags(DkImageFlags_UsageLoadStore | DkImageFlags_Usage2DEngine | DkImageFlags_UsageVideo)
        .initialize(lumaMappingLayout);

    dk::ImageLayoutMaker { dev }
        .setType(DkImageType_2D)
        .setFormat(DkImageFormat_RG8_Unorm)
        .setDimensions(width / 2, height / 2, 1)
        .setFlags(DkImageFlags_UsageLoadStore | DkImageFlags_Usage2DEngine | DkImageFlags_UsageVideo)
        .initialize(chromaMappingLayout);

    mappingMemblock = dk::MemBlockMaker { dev, ff_tx1_map_get_size(map) }
        .setFlags(DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image)
        .setStorage(ff_tx1_map_get_addr(map))
        .create();

    luma.initialize(lumaMappingLayout, mappingMemblock, 0);
    chroma.initialize(chromaMappingLayout, mappingMemblock, frame->data[1] - frame->data[0]);

    lumaDesc.initialize(luma);
    chromaDesc.initialize(chroma);

    imageDescriptorSet->update(cmdbuf, lumaTextureId, lumaDesc);
    imageDescriptorSet->update(cmdbuf, chromaTextureId, chromaDesc);

    queue.submitCommands(cmdbuf.finishList());
    queue.waitIdle();

    m_is_initialized = true;
}

void DKVideoRenderer::draw(NVGcontext* vg, int width, int height, AVFrame* frame) {
    checkAndInitialize(width, height, frame);

    dk::RasterizerState rasterizerState;
    dk::ColorState colorState;
    dk::ColorWriteState colorWriteState;

    // Clear the color buffer
    cmdbuf.clear();
    cmdbuf.clearColor(0, DkColorMask_RGBA, 0.0f, 0.0f, 0.0f, 0.0f);

    // Bind state required for drawing the triangle
    cmdbuf.bindShaders(DkStageFlag_GraphicsMask, { vertexShader, fragmentShader });
    cmdbuf.bindTextures(DkStage_Fragment, 0, dkMakeTextureHandle(lumaTextureId, 0));
    cmdbuf.bindTextures(DkStage_Fragment, 1, dkMakeTextureHandle(chromaTextureId, 0));
    cmdbuf.bindRasterizerState(rasterizerState);
    cmdbuf.bindColorState(colorState);
    cmdbuf.bindColorWriteState(colorWriteState);
    cmdbuf.bindVtxBuffer(0, vertexBuffer.getGpuAddr(), vertexBuffer.getSize());
    cmdbuf.bindVtxAttribState(VertexAttribState);
    cmdbuf.bindVtxBufferState(VertexBufferState);

    // Draw the triangle
    cmdbuf.draw(DkPrimitive_Quads, QuadVertexData.size(), 1, 0, 0);

    // Finish off this command list
    queue.submitCommands(cmdbuf.finishList());
    queue.waitIdle();
}

VideoRenderStats* DKVideoRenderer::video_render_stats() {
    // brls::Logger::info("{}", __PRETTY_FUNCTION__);
    m_video_render_stats.rendered_fps =
        (float)m_video_render_stats.rendered_frames /
        ((float)(LiGetMillis() -
                 m_video_render_stats.measurement_start_timestamp) /
         1000);
    return &m_video_render_stats;
}

#endif