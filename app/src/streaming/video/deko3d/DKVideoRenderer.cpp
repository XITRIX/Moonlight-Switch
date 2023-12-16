#if defined(__SWITCH__) && defined(BOREALIS_USE_DEKO3D)

#include "DKVideoRenderer.hpp"
#include <borealis/platforms/switch/switch_platform.hpp>

#include <array>

namespace
{
    static constexpr unsigned StaticCmdSize = 0x10000;

    struct Vertex
    {
        float position[3];
        float color[3];
    };

    constexpr std::array VertexAttribState =
    {
        DkVtxAttribState{ 0, 0, offsetof(Vertex, position), DkVtxAttribSize_3x32, DkVtxAttribType_Float, 0 },
        DkVtxAttribState{ 0, 0, offsetof(Vertex, color),    DkVtxAttribSize_3x32, DkVtxAttribType_Float, 0 },
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
    texId = vctx->allocateImageIndex();

// Configure persistent state in the queue
    {
        // Upload the image descriptor
        imageDescriptorSet->update(cmdbuf, texId, texImage.getDescriptor());
        brls::Logger::debug("{}: Texture ID {}", __PRETTY_FUNCTION__, texId);

        // Configure a sampler
        // dk::Sampler sampler;
        // sampler.setFilter(DkFilter_Linear, DkFilter_Linear);
        // sampler.setWrapMode(DkWrapMode_ClampToEdge, DkWrapMode_ClampToEdge, DkWrapMode_ClampToEdge);

        // // Upload the sampler descriptor
        // dk::SamplerDescriptor samplerDescriptor;
        // samplerDescriptor.initialize(sampler);
        // samplerDescriptorSet.update(cmdbuf, 0, samplerDescriptor);

        // Bind the image and sampler descriptor sets
        // imageDescriptorSet.bindForImages(cmdbuf);
        // samplerDescriptorSet.bindForSamplers(cmdbuf);

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
    // brls::Logger::info("{}: {} / {}", __PRETTY_FUNCTION__, width, height);
    checkAndInitialize(width, height, frame);

    dk::RasterizerState rasterizerState;
    dk::ColorState colorState;
    dk::ColorWriteState colorWriteState;

    // Clear the color buffer
    cmdbuf.clear();
    cmdbuf.clearColor(0, DkColorMask_RGBA, 0.0f, 0.0f, 0.0f, 0.0f);

    // imageDescriptorSet->update(cmdbuf, texId, texImage.getDescriptor());

    // Bind state required for drawing the triangle
    cmdbuf.bindShaders(DkStageFlag_GraphicsMask, { vertexShader, fragmentShader });
    cmdbuf.bindTextures(DkStage_Fragment, 0, dkMakeTextureHandle(texId, 0));
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