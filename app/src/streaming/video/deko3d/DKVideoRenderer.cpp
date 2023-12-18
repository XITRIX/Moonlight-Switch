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
    // samplerDescriptorSet.allocate(*pool_data);

// Load the shaders
    vertexShader.load(*pool_code, "romfs:/shaders/basic_vsh.dksh");
    fragmentShader.load(*pool_code, "romfs:/shaders/texture_fsh.dksh");

// Load the vertex buffer
    vertexBuffer = pool_data->allocate(sizeof(QuadVertexData), alignof(Vertex));
    memcpy(vertexBuffer.getCpuAddr(), QuadVertexData.data(), vertexBuffer.getSize());

// Load the image
    texImage.load(*pool_images, *pool_data, dev, queue, "romfs:/cat-256x256.bc1", 256, 256, DkImageFormat_RGB_BC1);
    lumaTextureId = vctx->allocateImageIndex();
    chromaTextureId = vctx->allocateImageIndex();

// Configure persistent state in the queue
    {
        // Upload the image descriptor
        imageDescriptorSet->update(cmdbuf, lumaTextureId, texImage.getDescriptor());
        brls::Logger::debug("{}: Texture ID {}", __PRETTY_FUNCTION__, lumaTextureId);

        // // Submit the configuration commands to the queue
        queue.submitCommands(cmdbuf.finishList());
        queue.waitIdle();
        // cmdbuf.clear();
    }

    m_is_initialized = true;
}

void DKVideoRenderer::createFramebufferResources(int width, int height) {
    // Initialize state structs with deko3d defaults
}

void DKVideoRenderer::draw(NVGcontext* vg, int width, int height, AVFrame* frame) {
    // brls::Logger::info("{}: Check!!!!", __PRETTY_FUNCTION__);
    // int frameSize = frame->width * frame->height * 3 / 2;
    // int frameSize = av_image_get_buffer_size((AVPixelFormat) frame->format, frame->width, frame->height, DK_IMAGE_LINEAR_STRIDE_ALIGNMENT);
    // int frameSize = avpicture_get_size((AVPixelFormat) frame->format, frame->width, frame->height);
    // brls::Logger::debug("{}: frame data size {}", __PRETTY_FUNCTION__, frameSize);
    // brls::Logger::debug("{}: frame width {}", __PRETTY_FUNCTION__, frame->linesize[0]);
    checkAndInitialize(width, height, frame);

    dk::RasterizerState rasterizerState;
    dk::ColorState colorState;
    dk::ColorWriteState colorWriteState;

    // Clear the color buffer
    cmdbuf.clear();
    cmdbuf.clearColor(0, DkColorMask_RGBA, 0.0f, 0.0f, 0.0f, 0.0f);

//////////////////////////////////////////////////////////////
    AVTX1Map *map = ff_tx1_frame_get_fbuf_map(frame);
    brls::Logger::info("{}: Map size: {} | {} | {} | {}", __PRETTY_FUNCTION__, map->map.handle, map->map.has_init, map->map.cpu_addr, map->map.size);

    dk::ImageLayout layout; 
    dk::ImageLayoutMaker { dev }
        .setType(DkImageType_2D)
        .setFormat(DkImageFormat_R8_Unorm)
        .setDimensions(width, height, 1)
        .setFlags(DkImageFlags_UsageLoadStore | DkImageFlags_Usage2DEngine | DkImageFlags_UsageVideo)
        .initialize(layout);

    dk::MemBlock memblock = dk::MemBlockMaker { dev, ff_tx1_map_get_size(map) }
        .setFlags(DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image)
        .setStorage(ff_tx1_map_get_addr(map))
        .create();

    dk::Image luma;
    luma.initialize(layout, memblock, 0);

    dk::Image chroma;
    chroma.initialize(layout, memblock, frame->data[1] - frame->data[0]);

    dk::ImageDescriptor lumaDesc;
    lumaDesc.initialize(luma);

    dk::ImageDescriptor chromaDesc;
    chromaDesc.initialize(chroma);

    imageDescriptorSet->update(cmdbuf, lumaTextureId, lumaDesc);
    imageDescriptorSet->update(cmdbuf, chromaTextureId, chromaDesc);

    queue.submitCommands(cmdbuf.finishList());
    queue.waitIdle();

//////////////////////////////////////////////////////////////

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

    dkMemBlockDestroy(memblock);
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