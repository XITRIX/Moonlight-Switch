#if defined(PLATFORM_SWITCH) && defined(BOREALIS_USE_DEKO3D)

#define FF_API_AVPICTURE

#include "DKVideoRenderer.hpp"
#include <borealis/platforms/switch/switch_platform.hpp>

#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext_nvtegra.h>
#include <libavutil/imgutils.h>

#include <array>

static const glm::vec3 gl_color_offset(bool color_full) {
    static const glm::vec3 limitedOffsets = {16.0f / 255.0f, 128.0f / 255.0f,
                                           128.0f / 255.0f};
    static const glm::vec3 fullOffsets = {0.0f, 128.0f / 255.0f, 128.0f / 255.0f};
    return color_full ? fullOffsets : limitedOffsets;
}

static const glm::mat3 gl_color_matrix(enum AVColorSpace color_space,
                                    bool color_full) {
    static const glm::mat3 bt601Lim = {1.1644f, 1.1644f, 1.1644f,  0.0f, -0.3917f,
                                     2.0172f, 1.5960f, -0.8129f, 0.0f};
    static const glm::mat3 bt601Full = {
        1.0f, 1.0f, 1.0f, 0.0f, -0.3441f, 1.7720f, 1.4020f, -0.7141f, 0.0f};
    static const glm::mat3 bt709Lim = {1.1644f, 1.1644f, 1.1644f,  0.0f, -0.2132f,
                                     2.1124f, 1.7927f, -0.5329f, 0.0f};
    static const glm::mat3 bt709Full = {
        1.0f, 1.0f, 1.0f, 0.0f, -0.1873f, 1.8556f, 1.5748f, -0.4681f, 0.0f};
    static const glm::mat3 bt2020Lim = {1.1644f, 1.1644f,  1.1644f,
                                      0.0f,    -0.1874f, 2.1418f,
                                      1.6781f, -0.6505f, 0.0f};
    static const glm::mat3 bt2020Full = {
        1.0f, 1.0f, 1.0f, 0.0f, -0.1646f, 1.8814f, 1.4746f, -0.5714f, 0.0f};

    switch (color_space) {
    case AVCOL_SPC_SMPTE170M:
    case AVCOL_SPC_BT470BG:
        return color_full ? bt601Full : bt601Lim;
    case AVCOL_SPC_BT709:
        return color_full ? bt709Full : bt709Lim;
    case AVCOL_SPC_BT2020_NCL:
    case AVCOL_SPC_BT2020_CL:
        return color_full ? bt2020Full : bt2020Lim;
    default:
        return bt601Lim;
    }
}

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
    transformUniformBuffer.destroy();
    dkMemBlockDestroy(mappingMemblock);
}

void DKVideoRenderer::checkAndInitialize(int width, int height, AVFrame* frame) {
    // if (m_is_initialized) return;

    if ((m_frame_width == frame->width) && (m_frame_height == frame->height) &&
        (m_screen_width == width) && (m_screen_height == height)) 
        return;

    brls::Logger::info("{}: {} / {}", __PRETTY_FUNCTION__, width, height);

    m_frame_width = frame->width;
    m_frame_height = frame->height;

    m_screen_width = width;
    m_screen_height = height;

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

    AVNVTegraMap *map = av_nvtegra_frame_get_fbuf_map(frame);
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

    mappingMemblock = dk::MemBlockMaker { dev, av_nvtegra_map_get_size(map) }
        .setFlags(DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image)
        .setStorage(av_nvtegra_map_get_addr(map))
        .create();

    luma.initialize(lumaMappingLayout, mappingMemblock, 0);
    chroma.initialize(chromaMappingLayout, mappingMemblock, frame->data[1] - frame->data[0]);

    lumaDesc.initialize(luma);
    chromaDesc.initialize(chroma);

    imageDescriptorSet->update(cmdbuf, lumaTextureId, lumaDesc);
    imageDescriptorSet->update(cmdbuf, chromaTextureId, chromaDesc);

// Load the transform buffer
    transformUniformBuffer = pool_data->allocate(sizeof(transformState), DK_UNIFORM_BUF_ALIGNMENT);

    bool colorFull = frame->color_range == AVCOL_RANGE_JPEG;

    transformState.offset = gl_color_offset(colorFull);
    transformState.yuvmat = gl_color_matrix(frame->colorspace, colorFull);

    float frameAspect = ((float)m_frame_height / (float)m_frame_width);
    float screenAspect = ((float)m_screen_height / (float)m_screen_width);

    if (frameAspect > screenAspect) {
        float multiplier = frameAspect / screenAspect;
        transformState.uv_data = { 0.5f - 0.5f * (1.0f / multiplier),
                    0.0f, multiplier, 1.0f };
    } else {
        float multiplier = screenAspect / frameAspect;
        transformState.uv_data = { 0.0f,
                    0.5f - 0.5f * (1.0f / multiplier), 1.0f, multiplier };
    }

    cmdbuf.pushConstants(
            transformUniformBuffer.getGpuAddr(), transformUniformBuffer.getSize(),
            0, sizeof(transformState), &transformState);

    queue.submitCommands(cmdbuf.finishList());
    queue.waitIdle();

    m_is_initialized = true;
}

void DKVideoRenderer::draw(NVGcontext* vg, int width, int height, AVFrame* frame, int imageFormat) {
    checkAndInitialize(width, height, frame);

    if (!m_video_render_stats.rendered_frames) {
        m_video_render_stats.measurement_start_timestamp = LiGetMillis();
    }

    uint64_t before_render = LiGetMillis();

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
    cmdbuf.bindUniformBuffer(DkStage_Vertex, 0, transformUniformBuffer.getGpuAddr(), transformUniformBuffer.getSize());
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
    // queue.waitIdle();

    m_video_render_stats.total_render_time += LiGetMillis() - before_render;
    m_video_render_stats.rendered_frames++;
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