#if defined(PLATFORM_SWITCH) && defined(BOREALIS_USE_DEKO3D)

#define FF_API_AVPICTURE

#include "DKVideoRenderer.hpp"
#include <borealis/platforms/switch/switch_platform.hpp>

#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext_nvtegra.h>
#include <libavutil/imgutils.h>

#include <array>
#include <cstddef>

static const glm::vec3 gl_color_offset(bool color_full) {
    static const glm::vec3 limitedOffsets = {16.0f / 255.0f, 128.0f / 255.0f,
                                             128.0f / 255.0f};
    static const glm::vec3 fullOffsets = {0.0f, 128.0f / 255.0f, 128.0f / 255.0f};
    return color_full ? fullOffsets : limitedOffsets;
}

static const glm::mat3 gl_color_matrix(enum AVColorSpace color_space,
                                       bool color_full) {
    static const glm::mat3 bt601Lim = {1.1644f, 1.1644f, 1.1644f, 0.0f, -0.3917f,
                                       2.0172f, 1.5960f, -0.8129f, 0.0f};
    static const glm::mat3 bt601Full = {
        1.0f, 1.0f, 1.0f, 0.0f, -0.3441f, 1.7720f, 1.4020f, -0.7141f, 0.0f};
    static const glm::mat3 bt709Lim = {1.1644f, 1.1644f, 1.1644f, 0.0f, -0.2132f,
                                       2.1124f, 1.7927f, -0.5329f, 0.0f};
    static const glm::mat3 bt709Full = {
        1.0f, 1.0f, 1.0f, 0.0f, -0.1873f, 1.8556f, 1.5748f, -0.4681f, 0.0f};
    static const glm::mat3 bt2020Lim = {1.1644f, 1.1644f, 1.1644f, 0.0f, -0.1874f,
                                        2.1418f, 1.6781f, -0.6505f, 0.0f};
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
static enum AVColorSpace to_av_colorspace(int colorspace) {
    switch (colorspace) {
    case COLORSPACE_REC_709:
        return AVCOL_SPC_BT709;
    case COLORSPACE_REC_2020:
        return AVCOL_SPC_BT2020_NCL;
    case COLORSPACE_REC_601:
    default:
        return AVCOL_SPC_SMPTE170M;
    }
}

namespace
{
    static constexpr unsigned StaticCmdSize = 0x10000;
    static constexpr unsigned UpdateCmdSliceSize = 0x1000;

    struct Transformation
    {
        // std140 layout for:
        //   mat3 yuvmat; vec3 offset; vec4 uv_data;
        // mat3 is stored as 3 vec4 columns in std140.
        alignas(16) float yuvmat_col0[4];
        alignas(16) float yuvmat_col1[4];
        alignas(16) float yuvmat_col2[4];
        alignas(16) float offset[4];
        alignas(16) float uv_data[4];
    };

    static_assert(sizeof(Transformation) == 80,
                  "Transformation must match std140 layout");
    static_assert(offsetof(Transformation, offset) == 48,
                  "Unexpected std140 offset for offset");
    static_assert(offsetof(Transformation, uv_data) == 64,
                  "Unexpected std140 offset for uv_data");

    struct Vertex
    {
        float position[3];
        float uv[2];
    };

    constexpr std::array VertexAttribState = {
        DkVtxAttribState{0, 0, offsetof(Vertex, position), DkVtxAttribSize_3x32,
                         DkVtxAttribType_Float, 0},
        DkVtxAttribState{0, 0, offsetof(Vertex, uv), DkVtxAttribSize_2x32,
                         DkVtxAttribType_Float, 0},
    };

    constexpr std::array VertexBufferState = {
        DkVtxBufferState{sizeof(Vertex), 0},
    };

    constexpr std::array QuadVertexData = {
        Vertex{{-1.0f, +1.0f, 0.0f}, {0.0f, 0.0f}},
        Vertex{{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
        Vertex{{+1.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
        Vertex{{+1.0f, +1.0f, 0.0f}, {1.0f, 0.0f}},
    };

    static void getFrameColorInfo(AVFrame* frame, AVColorSpace& colorSpace,
                                  bool& colorFull) {
        colorFull = frame->color_range == AVCOL_RANGE_JPEG;

        // NVDEC/NVTEGRA frames can report JPEG range even when stream
        // negotiation requested limited range. This manifests as lifted blacks.
        if (frame->format == AV_PIX_FMT_NVTEGRA &&
            frame->color_range == AVCOL_RANGE_JPEG) {
            colorFull = false;
        }

        switch (frame->colorspace) {
        case AVCOL_SPC_SMPTE170M:
        case AVCOL_SPC_BT470BG:
            colorSpace = AVCOL_SPC_SMPTE170M;
            break;
        case AVCOL_SPC_BT709:
            colorSpace = AVCOL_SPC_BT709;
            break;
        case AVCOL_SPC_BT2020_NCL:
        case AVCOL_SPC_BT2020_CL:
            colorSpace = AVCOL_SPC_BT2020_NCL;
            break;
        default:
            colorSpace = to_av_colorspace(COLORSPACE_REC_601);
            break;
        }
    }
}

DKVideoRenderer::DKVideoRenderer() {}

DKVideoRenderer::~DKVideoRenderer() {
    if (vctx != nullptr) {
        queue.waitIdle();
    }

    frameMappings.clear();
    releaseImageSlots();
    vertexBuffer.destroy();
    transformUniformBuffer.destroy();
}

void DKVideoRenderer::checkAndInitialize(int width, int height, AVFrame* frame) {
    if (m_is_initialized)
        return;

    brls::Logger::info("{}: {} / {}", __PRETTY_FUNCTION__, width, height);

    m_frame_width = frame->width;
    m_frame_height = frame->height;

    m_screen_width = width;
    m_screen_height = height;

    vctx = (brls::SwitchVideoContext*)brls::Application::getPlatform()->getVideoContext();
    dev = vctx->getDeko3dDevice();
    queue = vctx->getQueue();

    // Create the memory pools
    pool_code.emplace(dev,
                      DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached |
                          DkMemBlockFlags_Code,
                      128 * 1024);
    pool_data.emplace(dev,
                      DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached,
                      1 * 1024 * 1024);

    // Static draw command buffer
    cmdbuf = dk::CmdBufMaker{dev}.create();
    CMemPool::Handle cmdmem = pool_data->allocate(StaticCmdSize);
    cmdbuf.addMemory(cmdmem.getMemBlock(), cmdmem.getOffset(), cmdmem.getSize());

    // Dynamic descriptor update command buffer with a tiny ring
    updateCmdbuf = dk::CmdBufMaker{dev}.create();
    updateCmdMemRing.allocate(*pool_data, UpdateCmdSliceSize);

    // Load the shaders
    vertexShader.load(*pool_code, "romfs:/shaders/basic_vsh.dksh");
    fragmentShader.load(*pool_code, "romfs:/shaders/texture_fsh.dksh");

    // Load the vertex buffer
    vertexBuffer = pool_data->allocate(sizeof(QuadVertexData), alignof(Vertex));
    memcpy(vertexBuffer.getCpuAddr(), QuadVertexData.data(), vertexBuffer.getSize());

    // Load the transform buffer
    transformUniformBuffer =
        pool_data->allocate(sizeof(Transformation), DK_UNIFORM_BUF_ALIGNMENT);

    // Allocate image indexes for planes
    lumaTextureId = vctx->allocateImageIndex();
    chromaTextureId = vctx->allocateImageIndex();

    if (lumaTextureId < 0 || chromaTextureId < 0) {
        brls::Logger::error("{}: Failed to reserve image descriptor slots",
                            __PRETTY_FUNCTION__);
        releaseImageSlots();
        return;
    }

    brls::Logger::debug("{}: Luma texture ID {}", __PRETTY_FUNCTION__,
                        lumaTextureId);
    brls::Logger::debug("{}: Chroma texture ID {}", __PRETTY_FUNCTION__,
                        chromaTextureId);

    updateFrameLayouts();
    recordStaticCommands(frame);

    m_is_initialized = true;
}

void DKVideoRenderer::updateFrameLayouts() {
    dk::ImageLayoutMaker{dev}
        .setType(DkImageType_2D)
        .setFormat(DkImageFormat_R8_Unorm)
        .setDimensions(m_frame_width, m_frame_height, 1)
        .setFlags(DkImageFlags_UsageLoadStore | DkImageFlags_Usage2DEngine |
                  DkImageFlags_UsageVideo)
        .initialize(lumaMappingLayout);

    dk::ImageLayoutMaker{dev}
        .setType(DkImageType_2D)
        .setFormat(DkImageFormat_RG8_Unorm)
        .setDimensions(m_frame_width / 2, m_frame_height / 2, 1)
        .setFlags(DkImageFlags_UsageLoadStore | DkImageFlags_Usage2DEngine |
                  DkImageFlags_UsageVideo)
        .initialize(chromaMappingLayout);
}

void DKVideoRenderer::recordStaticCommands(AVFrame* frame) {
    AVColorSpace colorSpace = AVCOL_SPC_UNSPECIFIED;
    bool colorFull = false;
    getFrameColorInfo(frame, colorSpace, colorFull);

    Transformation transformState = {};
    const glm::vec3 colorOffset = gl_color_offset(colorFull);
    const glm::mat3 colorMatrix = gl_color_matrix(colorSpace, colorFull);

    transformState.yuvmat_col0[0] = colorMatrix[0][0];
    transformState.yuvmat_col0[1] = colorMatrix[0][1];
    transformState.yuvmat_col0[2] = colorMatrix[0][2];
    transformState.yuvmat_col0[3] = 0.0f;

    transformState.yuvmat_col1[0] = colorMatrix[1][0];
    transformState.yuvmat_col1[1] = colorMatrix[1][1];
    transformState.yuvmat_col1[2] = colorMatrix[1][2];
    transformState.yuvmat_col1[3] = 0.0f;

    transformState.yuvmat_col2[0] = colorMatrix[2][0];
    transformState.yuvmat_col2[1] = colorMatrix[2][1];
    transformState.yuvmat_col2[2] = colorMatrix[2][2];
    transformState.yuvmat_col2[3] = 0.0f;

    transformState.offset[0] = colorOffset[0];
    transformState.offset[1] = colorOffset[1];
    transformState.offset[2] = colorOffset[2];
    transformState.offset[3] = 0.0f;

    const float frameAspect = static_cast<float>(m_frame_height) /
                              static_cast<float>(m_frame_width);
    const float screenAspect = static_cast<float>(m_screen_height) /
                               static_cast<float>(m_screen_width);

    if (frameAspect > screenAspect) {
        const float multiplier = frameAspect / screenAspect;
        transformState.uv_data[0] = 0.5f - 0.5f * (1.0f / multiplier);
        transformState.uv_data[1] = 0.0f;
        transformState.uv_data[2] = multiplier;
        transformState.uv_data[3] = 1.0f;
    } else {
        const float multiplier = screenAspect / frameAspect;
        transformState.uv_data[0] = 0.0f;
        transformState.uv_data[1] = 0.5f - 0.5f * (1.0f / multiplier);
        transformState.uv_data[2] = 1.0f;
        transformState.uv_data[3] = multiplier;
    }

    dk::RasterizerState rasterizerState;
    dk::ColorState colorState;
    dk::ColorWriteState colorWriteState;

    cmdbuf.clear();
    cmdbuf.clearColor(0, DkColorMask_RGBA, 0.0f, 0.0f, 0.0f, 0.0f);
    cmdbuf.bindShaders(DkStageFlag_GraphicsMask, {vertexShader, fragmentShader});
    cmdbuf.bindTextures(DkStage_Fragment, 0,
                        dkMakeTextureHandle(lumaTextureId, 0));
    cmdbuf.bindTextures(DkStage_Fragment, 1,
                        dkMakeTextureHandle(chromaTextureId, 0));
    cmdbuf.bindUniformBuffer(DkStage_Fragment, 0,
                             transformUniformBuffer.getGpuAddr(),
                             transformUniformBuffer.getSize());
    cmdbuf.pushConstants(transformUniformBuffer.getGpuAddr(),
                         transformUniformBuffer.getSize(), 0,
                         sizeof(transformState), &transformState);
    cmdbuf.bindRasterizerState(rasterizerState);
    cmdbuf.bindColorState(colorState);
    cmdbuf.bindColorWriteState(colorWriteState);
    cmdbuf.bindVtxBuffer(0, vertexBuffer.getGpuAddr(), vertexBuffer.getSize());
    cmdbuf.bindVtxAttribState(VertexAttribState);
    cmdbuf.bindVtxBufferState(VertexBufferState);
    cmdbuf.draw(DkPrimitive_Quads, QuadVertexData.size(), 1, 0, 0);
    cmdlist = cmdbuf.finishList();

    m_color_space = colorSpace;
    m_color_full = colorFull;
}

void DKVideoRenderer::updateRenderState(int width, int height, AVFrame* frame) {
    AVColorSpace colorSpace = AVCOL_SPC_UNSPECIFIED;
    bool colorFull = false;
    getFrameColorInfo(frame, colorSpace, colorFull);

    const bool frameSizeChanged =
        m_frame_width != frame->width || m_frame_height != frame->height;
    const bool screenSizeChanged =
        m_screen_width != width || m_screen_height != height;
    const bool colorChanged =
        m_color_space != static_cast<int>(colorSpace) || m_color_full != colorFull;

    if (!frameSizeChanged && !screenSizeChanged && !colorChanged) {
        return;
    }

    queue.waitIdle();

    if (frameSizeChanged) {
        m_frame_width = frame->width;
        m_frame_height = frame->height;
        frameMappings.clear();
        currentMappingIndex = -1;
        updateFrameLayouts();
    }

    m_screen_width = width;
    m_screen_height = height;
    recordStaticCommands(frame);
}

void DKVideoRenderer::updateFrameMapping(AVFrame* frame) {
    AVNVTegraMap* map = av_nvtegra_frame_get_fbuf_map(frame);
    if (!map || lumaTextureId < 0 || chromaTextureId < 0)
        return;

    uint32_t handle = av_nvtegra_map_get_handle(map);
    void* cpuAddr = av_nvtegra_map_get_addr(map);
    uint32_t size = av_nvtegra_map_get_size(map);
    uint32_t chromaOffset = static_cast<uint32_t>(frame->data[1] - frame->data[0]);

    int mappingIndex = -1;
    if (currentMappingIndex >= 0 &&
        currentMappingIndex < static_cast<int>(frameMappings.size())) {
        const auto& currentMapping = frameMappings[currentMappingIndex];
        if (currentMapping.handle == handle &&
            currentMapping.cpuAddr == cpuAddr &&
            currentMapping.size == size &&
            currentMapping.chromaOffset == chromaOffset) {
            return;
        }
    }

    for (size_t i = 0; i < frameMappings.size(); ++i) {
        const auto& mapping = frameMappings[i];
        if (mapping.handle == handle && mapping.cpuAddr == cpuAddr &&
            mapping.size == size && mapping.chromaOffset == chromaOffset) {
            mappingIndex = static_cast<int>(i);
            break;
        }
    }

    if (mappingIndex < 0) {
        FrameMapping mapping;
        mapping.handle = handle;
        mapping.cpuAddr = cpuAddr;
        mapping.size = size;
        mapping.chromaOffset = chromaOffset;

        mapping.memblock = dk::MemBlockMaker{dev, size}
                               .setFlags(DkMemBlockFlags_CpuUncached |
                                         DkMemBlockFlags_GpuCached |
                                         DkMemBlockFlags_Image)
                               .setStorage(cpuAddr)
                               .create();

        mapping.luma.initialize(lumaMappingLayout, mapping.memblock, 0);
        mapping.chroma.initialize(chromaMappingLayout, mapping.memblock,
                                  chromaOffset);

        mapping.lumaDesc.initialize(mapping.luma);
        mapping.chromaDesc.initialize(mapping.chroma);

        frameMappings.emplace_back(std::move(mapping));
        mappingIndex = static_cast<int>(frameMappings.size()) - 1;

        brls::Logger::debug("{}: Added mapping for handle {}", __PRETTY_FUNCTION__,
                            handle);
    }

    updateCmdMemRing.begin(updateCmdbuf);

    auto& active = frameMappings[mappingIndex];
    const bool updatedLuma =
        vctx->updateImageDescriptor(updateCmdbuf, lumaTextureId, active.lumaDesc);
    const bool updatedChroma = vctx->updateImageDescriptor(
        updateCmdbuf, chromaTextureId, active.chromaDesc);

    if (!updatedLuma || !updatedChroma) {
        brls::Logger::error("{}: Failed to update video descriptors",
                            __PRETTY_FUNCTION__);
    } else {
        vctx->invalidateImageDescriptors(updateCmdbuf);
        currentMappingIndex = mappingIndex;
    }

    queue.submitCommands(updateCmdMemRing.end(updateCmdbuf));
}

void DKVideoRenderer::releaseImageSlots() {
    if (!vctx) {
        return;
    }

    if (lumaTextureId >= 0) {
        vctx->freeImageIndex(lumaTextureId);
        lumaTextureId = -1;
    }

    if (chromaTextureId >= 0) {
        vctx->freeImageIndex(chromaTextureId);
        chromaTextureId = -1;
    }
}

int frames = 0;
uint64_t timeCount = 0;

void DKVideoRenderer::draw(NVGcontext* vg, int width, int height, AVFrame* frame,
                           int imageFormat) {
    checkAndInitialize(width, height, frame);
    if (!m_is_initialized) {
        return;
    }

    uint64_t before_render = LiGetMillis();

    if (!m_video_render_stats.rendered_frames) {
        m_video_render_stats.measurement_start_timestamp = before_render;
    }

    updateRenderState(width, height, frame);
    updateFrameMapping(frame);

    if (cmdlist != 0) {
        queue.submitCommands(cmdlist);
    }

    frames++;
    timeCount += LiGetMillis() - before_render;

    if (timeCount >= 5000) {
        brls::Logger::debug("FPS: {}", frames / 5.0f);
        frames = 0;
        timeCount -= 5000;
    }

    m_video_render_stats.total_render_time += LiGetMillis() - before_render;
    m_video_render_stats.rendered_frames++;
}

VideoRenderStats* DKVideoRenderer::video_render_stats() {
    if (!m_video_render_stats.rendered_frames) {
        m_video_render_stats.rendered_fps = 0.0f;
        m_video_render_stats.rendering_time = 0.0f;
        return &m_video_render_stats;
    }

    m_video_render_stats.rendered_fps =
        (float)m_video_render_stats.rendered_frames /
        ((float)(LiGetMillis() -
                 m_video_render_stats.measurement_start_timestamp) /
         1000.0f);

    m_video_render_stats.rendering_time =
        (float)m_video_render_stats.total_render_time /
        (float)m_video_render_stats.rendered_frames;

    return &m_video_render_stats;
}

#endif
