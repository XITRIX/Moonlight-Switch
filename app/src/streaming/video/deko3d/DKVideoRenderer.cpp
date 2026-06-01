#if defined(PLATFORM_SWITCH) && defined(BOREALIS_USE_DEKO3D)

#define FF_API_AVPICTURE

#include "DKVideoRenderer.hpp"
#include "Settings.hpp"
#include <borealis/platforms/switch/switch_platform.hpp>

#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext_nvtegra.h>
#include <libavutil/imgutils.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
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
    using PostProcessClock = std::chrono::steady_clock;

    static constexpr unsigned StaticCmdSize = 0x10000;
    static constexpr unsigned UpdateCmdSliceSize = 0x1000;
#ifdef SUPPORT_UPSCALING
    static constexpr unsigned PresentCmdSliceSize = 0x8000;
    static constexpr float DefaultDitheringStrength = 3.0f;
    static constexpr float DefaultRcasStrength = 0.2f;
#endif

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

#ifdef SUPPORT_UPSCALING
    struct RcasConstants
    {
        alignas(16) float sharpness[4];
    };

    struct DitheringConstants
    {
        alignas(16) float control[4];
    };

    static_assert(sizeof(RcasConstants) == 16,
                  "RcasConstants must match std140 layout");
    static_assert(sizeof(DitheringConstants) == 16,
                  "DitheringConstants must match std140 layout");
#endif

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

    uint64_t toMicroseconds(PostProcessClock::duration duration) {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(duration)
                .count());
    }
}

DKVideoRenderer::DKVideoRenderer() {}

DKVideoRenderer::~DKVideoRenderer() {
    if (vctx != nullptr) {
        queue.waitIdle();
    }

    frameMappings.clear();
#ifdef SUPPORT_UPSCALING
    releaseUpscalingResources();
    ditheringUniformBuffer.destroy();
    rcasUniformBuffer.destroy();
#endif
    releaseImageSlots();
    vertexBuffer.destroy();
    transformUniformBuffer.destroy();
}

bool DKVideoRenderer::shouldUseUpscaling() const {
#ifdef SUPPORT_UPSCALING
    return Settings::instance().upscaling() && m_frame_width > 0 &&
           m_frame_height > 0 && m_screen_width > 0 && m_screen_height > 0 &&
           (m_screen_width > m_frame_width || m_screen_height > m_frame_height);
#else
    return false;
#endif
}

#ifdef SUPPORT_UPSCALING
bool DKVideoRenderer::shouldUseDithering() const {
    return Settings::instance().dithering() && Settings::instance().dithering_strength() > 0.0f && m_frame_width > 0 &&
           m_frame_height > 0 && m_screen_width > 0 && m_screen_height > 0;
}

void DKVideoRenderer::updateDitheringConstants() {
    if (!ditheringUniformBuffer) {
        return;
    }

    const float requestedStrength =
        std::clamp(Settings::instance().dithering_strength(), 1.0f, 10.0f);
    DitheringConstants ditheringConstants = {};
    ditheringConstants.control[0] = m_dithering_enabled ? 1.0f : 0.0f;
    ditheringConstants.control[1] = requestedStrength;
    memcpy(ditheringUniformBuffer.getCpuAddr(), &ditheringConstants,
           sizeof(ditheringConstants));
    m_dithering_strength = requestedStrength;
}

bool DKVideoRenderer::shouldUseRcas() const {
    return Settings::instance().rcas() && Settings::instance().rcas_strength() > 0.0f && m_frame_width > 0 &&
           m_frame_height > 0 && m_screen_width > 0 && m_screen_height > 0;
}

void DKVideoRenderer::updateRcasConstants() {
    if (!rcasUniformBuffer) {
        return;
    }

    const float requestedStrength =
        std::clamp(Settings::instance().rcas_strength(), 0.0f, 1.0f);
    if (requestedStrength == m_rcas_strength) {
        return;
    }

    RcasConstants rcasConstants = {};
    rcasConstants.sharpness[0] = requestedStrength;
    memcpy(rcasUniformBuffer.getCpuAddr(), &rcasConstants, sizeof(rcasConstants));
    m_rcas_strength = requestedStrength;
}
#endif

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
#ifdef SUPPORT_UPSCALING
    pool_images.emplace(dev,
                        DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image);
#endif

    // Static draw command buffer
    cmdbuf = dk::CmdBufMaker{dev}.create();
    CMemPool::Handle cmdmem = pool_data->allocate(StaticCmdSize);
    cmdbuf.addMemory(cmdmem.getMemBlock(), cmdmem.getOffset(), cmdmem.getSize());

    // Dynamic descriptor update command buffer with a tiny ring
    updateCmdbuf = dk::CmdBufMaker{dev}.create();
    updateCmdMemRing.allocate(*pool_data, UpdateCmdSliceSize);
#ifdef SUPPORT_UPSCALING
    presentCmdbuf = dk::CmdBufMaker{dev}.create();
    presentCmdMemRing.allocate(*pool_data, PresentCmdSliceSize);
#endif

    // Load the shaders
    vertexShader.load(*pool_code, "romfs:/shaders/basic_vsh.dksh");
    fragmentShader.load(*pool_code, "romfs:/shaders/texture_fsh.dksh");
#ifdef SUPPORT_UPSCALING
    if (!upscalingFragmentShader.load(*pool_code,
                                      "romfs:/shaders/upscaling_fsh.dksh")) {
        brls::Logger::warning("{}: Failed to load Switch upscaling shader",
                              __PRETTY_FUNCTION__);
    }
    if (!rcasFragmentShader.load(*pool_code, "romfs:/shaders/rcas_fsh.dksh")) {
        brls::Logger::warning("{}: Failed to load Switch RCAS shader",
                              __PRETTY_FUNCTION__);
    }
    if (!upscalingPassFragmentShader.load(*pool_code,
                                          "romfs:/shaders/upscaling_pass_fsh.dksh")) {
        brls::Logger::warning("{}: Failed to load Switch upscaling pass shader",
                              __PRETTY_FUNCTION__);
    }
#endif

    // Load the vertex buffer
    vertexBuffer = pool_data->allocate(sizeof(QuadVertexData), alignof(Vertex));
    memcpy(vertexBuffer.getCpuAddr(), QuadVertexData.data(), vertexBuffer.getSize());

    // Load the transform buffer
    transformUniformBuffer =
        pool_data->allocate(sizeof(Transformation), DK_UNIFORM_BUF_ALIGNMENT);
#ifdef SUPPORT_UPSCALING
    ditheringUniformBuffer =
        pool_data->allocate(sizeof(DitheringConstants), DK_UNIFORM_BUF_ALIGNMENT);
    rcasUniformBuffer =
        pool_data->allocate(sizeof(RcasConstants), DK_UNIFORM_BUF_ALIGNMENT);

    DitheringConstants ditheringConstants = {};
    ditheringConstants.control[0] = 0.0f;
    ditheringConstants.control[1] = DefaultDitheringStrength;
    memcpy(ditheringUniformBuffer.getCpuAddr(), &ditheringConstants,
           sizeof(ditheringConstants));
    m_dithering_strength = DefaultDitheringStrength;

    RcasConstants rcasConstants = {};
    rcasConstants.sharpness[0] = DefaultRcasStrength;
    memcpy(rcasUniformBuffer.getCpuAddr(), &rcasConstants, sizeof(rcasConstants));
    m_rcas_strength = DefaultRcasStrength;
#endif

    // Allocate image indexes for planes
    lumaTextureId = vctx->allocateImageIndex();
    chromaTextureId = vctx->allocateImageIndex();
#ifdef SUPPORT_UPSCALING
    upscalingTextureId = vctx->allocateImageIndex();
    rcasTextureId = vctx->allocateImageIndex();
#endif

    if (lumaTextureId < 0 || chromaTextureId < 0
#ifdef SUPPORT_UPSCALING
        || upscalingTextureId < 0 || rcasTextureId < 0
#endif
    ) {
        brls::Logger::error("{}: Failed to reserve image descriptor slots",
                            __PRETTY_FUNCTION__);
        releaseImageSlots();
        return;
    }

    brls::Logger::debug("{}: Luma texture ID {}", __PRETTY_FUNCTION__,
                        lumaTextureId);
    brls::Logger::debug("{}: Chroma texture ID {}", __PRETTY_FUNCTION__,
                        chromaTextureId);
#ifdef SUPPORT_UPSCALING
    brls::Logger::debug("{}: Upscaling texture ID {}", __PRETTY_FUNCTION__,
                        upscalingTextureId);
    brls::Logger::debug("{}: RCAS texture ID {}", __PRETTY_FUNCTION__,
                        rcasTextureId);
#endif

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

#ifdef SUPPORT_UPSCALING
bool DKVideoRenderer::ensureUpscalingResources() {
    if (!pool_images || upscalingTextureId < 0 || !upscalingPassFragmentShader ||
        m_screen_width <= 0 || m_screen_height <= 0) {
        return false;
    }

    const bool wantRcas = shouldUseRcas() && rcasTextureId >= 0 &&
                          rcasFragmentShader && rcasUniformBuffer;

    const bool sizeChanged = m_upscaling_target_width != m_screen_width ||
                             m_upscaling_target_height != m_screen_height;
    const bool rcasUsageChanged = wantRcas != static_cast<bool>(rcasTargetHandle);
    if (!sizeChanged && !rcasUsageChanged && upscalingTargetHandle) {
        return true;
    }

    releaseUpscalingResources();

    dk::ImageLayoutMaker{dev}
        .setType(DkImageType_2D)
        .setFormat(DkImageFormat_RGBA8_Unorm)
        .setDimensions(m_screen_width, m_screen_height, 1)
        .setFlags(DkImageFlags_UsageRender | DkImageFlags_UsageLoadStore |
                  DkImageFlags_Usage2DEngine)
        .initialize(upscalingTargetLayout);

    upscalingTargetHandle = pool_images->allocate(
        upscalingTargetLayout.getSize(), upscalingTargetLayout.getAlignment());
    if (!upscalingTargetHandle) {
        brls::Logger::error("{}: Failed to allocate upscaling render target",
                            __PRETTY_FUNCTION__);
        return false;
    }

    upscalingTargetImage.initialize(upscalingTargetLayout,
                                    upscalingTargetHandle.getMemBlock(),
                                    upscalingTargetHandle.getOffset());
    upscalingTargetDesc.initialize(upscalingTargetImage);

    if (wantRcas) {
        rcasTargetLayout = upscalingTargetLayout;
        rcasTargetHandle = pool_images->allocate(rcasTargetLayout.getSize(),
                                                 rcasTargetLayout.getAlignment());
        if (!rcasTargetHandle) {
            brls::Logger::warning("{}: Failed to allocate RCAS render target, falling back to EASU-only post pass",
                                  __PRETTY_FUNCTION__);
        } else {
            rcasTargetImage.initialize(rcasTargetLayout,
                                       rcasTargetHandle.getMemBlock(),
                                       rcasTargetHandle.getOffset());
            rcasTargetDesc.initialize(rcasTargetImage);
        }
    }

    updateCmdMemRing.begin(updateCmdbuf);
    const bool updated = vctx->updateImageDescriptor(updateCmdbuf,
                                                     upscalingTextureId,
                                                     upscalingTargetDesc);
    bool updatedRcas = true;
    if (wantRcas && rcasTargetHandle) {
        updatedRcas = vctx->updateImageDescriptor(updateCmdbuf, rcasTextureId,
                                                  rcasTargetDesc);
    }
    if (updated || (wantRcas && updatedRcas)) {
        vctx->invalidateImageDescriptors(updateCmdbuf);
    }
    queue.submitCommands(updateCmdMemRing.end(updateCmdbuf));

    if (!updated) {
        brls::Logger::error("{}: Failed to bind upscaling render target descriptor",
                            __PRETTY_FUNCTION__);
        releaseUpscalingResources();
        return false;
    }

    if (wantRcas && rcasTargetHandle && !updatedRcas) {
        brls::Logger::warning("{}: Failed to bind RCAS descriptor, falling back to EASU-only post pass",
                              __PRETTY_FUNCTION__);
        rcasTargetHandle.destroy();
        rcasTargetImage = dk::Image{};
        rcasTargetLayout = dk::ImageLayout{};
        rcasTargetDesc = dk::ImageDescriptor{};
    }
    m_upscaling_target_width = m_screen_width;
    m_upscaling_target_height = m_screen_height;
    return true;
}

void DKVideoRenderer::releaseUpscalingResources() {
    upscalingTargetHandle.destroy();
    upscalingTargetImage = dk::Image{};
    upscalingTargetLayout = dk::ImageLayout{};
    upscalingTargetDesc = dk::ImageDescriptor{};
    rcasTargetHandle.destroy();
    rcasTargetImage = dk::Image{};
    rcasTargetLayout = dk::ImageLayout{};
    rcasTargetDesc = dk::ImageDescriptor{};
    m_upscaling_target_width = 0;
    m_upscaling_target_height = 0;
    m_rcas_enabled = false;
}

bool DKVideoRenderer::submitUpscalingPresentPass() {
    if ((!m_dithering_enabled && !m_upscaling_enabled && !m_rcas_enabled) ||
        upscalingTextureId < 0 ||
        !upscalingTargetHandle || !upscalingPassFragmentShader) {
        return false;
    }

    updateDitheringConstants();

    if (m_rcas_enabled) {
        updateRcasConstants();
    }

    dk::Image* framebuffer = vctx->getFramebuffer();
    dk::Image* depthBuffer = vctx->getDepthBuffer();
    if (!framebuffer || !depthBuffer) {
        return false;
    }

    presentCmdMemRing.begin(presentCmdbuf);

    dk::ImageView colorTarget{*framebuffer};
    dk::ImageView depthTarget{*depthBuffer};
    dk::RasterizerState rasterizerState;
    dk::DepthStencilState depthStencilState;
    dk::ColorState colorState;
    dk::ColorWriteState colorWriteState;
    int finalTextureId = upscalingTextureId;

    presentCmdbuf.bindRasterizerState(rasterizerState);
    presentCmdbuf.bindDepthStencilState(
        depthStencilState.setDepthTestEnable(false)
            .setDepthWriteEnable(false)
            .setStencilTestEnable(false));
    presentCmdbuf.bindColorState(colorState);
    presentCmdbuf.bindColorWriteState(colorWriteState);
    presentCmdbuf.bindVtxBuffer(0, vertexBuffer.getGpuAddr(),
                                vertexBuffer.getSize());
    presentCmdbuf.bindVtxAttribState(VertexAttribState);
    presentCmdbuf.bindVtxBufferState(VertexBufferState);

    if (m_rcas_enabled && rcasTextureId >= 0 && rcasTargetHandle &&
        rcasFragmentShader && rcasUniformBuffer) {
        const auto sharpeningStageStart = PostProcessClock::now();
        dk::ImageView rcasTarget{rcasTargetImage};

        presentCmdbuf.bindRenderTargets(&rcasTarget);
        presentCmdbuf.setViewports(
            0, {{{0.0f, 0.0f, static_cast<float>(m_screen_width),
                  static_cast<float>(m_screen_height), 0.0f, 1.0f}}});
        presentCmdbuf.setScissors(
            0, {{{0, 0, static_cast<uint32_t>(m_screen_width),
                  static_cast<uint32_t>(m_screen_height)}}});
        presentCmdbuf.bindShaders(DkStageFlag_GraphicsMask,
                                  {vertexShader, rcasFragmentShader});
        presentCmdbuf.bindTextures(DkStage_Fragment, 0,
                                   dkMakeTextureHandle(upscalingTextureId, 0));
        presentCmdbuf.bindUniformBuffer(DkStage_Fragment, 0,
                                        rcasUniformBuffer.getGpuAddr(),
                                        rcasUniformBuffer.getSize());
        presentCmdbuf.draw(DkPrimitive_Quads, QuadVertexData.size(), 1, 0, 0);
        finalTextureId = rcasTextureId;

        m_video_render_stats.total_sharpening_time +=
            toMicroseconds(PostProcessClock::now() - sharpeningStageStart);
        m_video_render_stats.sharpened_frames++;
    }

    const auto ditheringStageStart = PostProcessClock::now();
    presentCmdbuf.bindRenderTargets(&colorTarget, &depthTarget);
    presentCmdbuf.setViewports(
        0, {{{0.0f, 0.0f, static_cast<float>(m_screen_width),
              static_cast<float>(m_screen_height), 0.0f, 1.0f}}});
    presentCmdbuf.setScissors(
        0, {{{0, 0, static_cast<uint32_t>(m_screen_width),
              static_cast<uint32_t>(m_screen_height)}}});
    presentCmdbuf.bindShaders(DkStageFlag_GraphicsMask,
                              {vertexShader, upscalingPassFragmentShader});
    presentCmdbuf.bindTextures(DkStage_Fragment, 0,
                               dkMakeTextureHandle(finalTextureId, 0));
    presentCmdbuf.bindUniformBuffer(DkStage_Fragment, 0,
                                    ditheringUniformBuffer.getGpuAddr(),
                                    ditheringUniformBuffer.getSize());
    presentCmdbuf.draw(DkPrimitive_Quads, QuadVertexData.size(), 1, 0, 0);

    if (m_dithering_enabled) {
        m_video_render_stats.total_dithering_time +=
            toMicroseconds(PostProcessClock::now() - ditheringStageStart);
        m_video_render_stats.dithered_frames++;
    }

    queue.submitCommands(presentCmdMemRing.end(presentCmdbuf));
    return true;
}
#endif

void DKVideoRenderer::recordStaticCommands(AVFrame* frame) {
    AVColorSpace colorSpace = AVCOL_SPC_UNSPECIFIED;
    bool colorFull = false;
    getFrameColorInfo(frame, colorSpace, colorFull);
#ifdef SUPPORT_UPSCALING
    const bool requestedDithering = shouldUseDithering();
#else
    const bool requestedDithering = false;
#endif
    const bool requestedUpscaling = shouldUseUpscaling();
#ifdef SUPPORT_UPSCALING
    const bool requestedRcas = shouldUseRcas();
#else
    const bool requestedRcas = false;
#endif
    bool useDithering = requestedDithering;
    bool useUpscaling = requestedUpscaling;
    bool useRcas = requestedRcas;
#ifdef SUPPORT_UPSCALING
    if ((useDithering || useUpscaling || useRcas) && !ensureUpscalingResources()) {
        brls::Logger::warning("{}: Falling back to direct presentation path",
                              __PRETTY_FUNCTION__);
        useDithering = false;
        useUpscaling = false;
        useRcas = false;
    }
#endif

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
    dk::DepthStencilState depthStencilState;
    dk::ColorState colorState;
    dk::ColorWriteState colorWriteState;
    const CShader* activeFragmentShader = &fragmentShader;

#ifdef SUPPORT_UPSCALING
    if (useUpscaling && upscalingFragmentShader) {
        activeFragmentShader = &upscalingFragmentShader;
    }
#endif

    cmdbuf.clear();
#ifdef SUPPORT_UPSCALING
    if (useDithering || useUpscaling || useRcas) {
        dk::ImageView upscalingTarget{upscalingTargetImage};
        cmdbuf.bindRenderTargets(&upscalingTarget);
        cmdbuf.setViewports(
            0, {{{0.0f, 0.0f, static_cast<float>(m_upscaling_target_width),
                  static_cast<float>(m_upscaling_target_height), 0.0f, 1.0f}}});
        cmdbuf.setScissors(
            0, {{{0, 0, static_cast<uint32_t>(m_upscaling_target_width),
                  static_cast<uint32_t>(m_upscaling_target_height)}}});
    }
#endif
    cmdbuf.clearColor(0, DkColorMask_RGBA, 0.0f, 0.0f, 0.0f, 0.0f);
    cmdbuf.bindShaders(DkStageFlag_GraphicsMask,
                       {vertexShader, *activeFragmentShader});
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
    cmdbuf.bindDepthStencilState(
        depthStencilState.setDepthTestEnable(false)
            .setDepthWriteEnable(false)
            .setStencilTestEnable(false));
    cmdbuf.bindColorState(colorState);
    cmdbuf.bindColorWriteState(colorWriteState);
    cmdbuf.bindVtxBuffer(0, vertexBuffer.getGpuAddr(), vertexBuffer.getSize());
    cmdbuf.bindVtxAttribState(VertexAttribState);
    cmdbuf.bindVtxBufferState(VertexBufferState);
    cmdbuf.draw(DkPrimitive_Quads, QuadVertexData.size(), 1, 0, 0);
    cmdlist = cmdbuf.finishList();

    m_color_space = colorSpace;
    m_color_full = colorFull;
    m_dithering_enabled = useDithering;
    m_upscaling_enabled = useUpscaling;
    m_dithering_requested = requestedDithering;
    m_upscaling_requested = requestedUpscaling;
    m_rcas_requested = requestedRcas;
#ifdef SUPPORT_UPSCALING
    m_rcas_enabled = useRcas && rcasTargetHandle && rcasFragmentShader &&
                     rcasUniformBuffer;
#endif
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
#ifdef SUPPORT_UPSCALING
    const bool requestedDithering =
        Settings::instance().dithering() && frame->width > 0 && frame->height > 0 &&
        width > 0 && height > 0;
#else
    const bool requestedDithering = false;
#endif
    const bool requestedUpscaling =
        Settings::instance().upscaling() && frame->width > 0 && frame->height > 0 &&
        width > 0 && height > 0 &&
        (width > frame->width || height > frame->height);
    const bool requestedRcas =
        Settings::instance().rcas() && Settings::instance().rcas_strength() > 0.0f &&
        frame->width > 0 && frame->height > 0 &&
        width > 0 && height > 0;
    const bool ditheringChanged = m_dithering_requested != requestedDithering;
    const bool upscalingChanged = m_upscaling_requested != requestedUpscaling;
    const bool rcasChanged = m_rcas_requested != requestedRcas;

    if (!frameSizeChanged && !screenSizeChanged && !colorChanged &&
        !ditheringChanged && !upscalingChanged && !rcasChanged) {
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

#ifdef SUPPORT_UPSCALING
    if (upscalingTextureId >= 0) {
        vctx->freeImageIndex(upscalingTextureId);
        upscalingTextureId = -1;
    }

    if (rcasTextureId >= 0) {
        vctx->freeImageIndex(rcasTextureId);
        rcasTextureId = -1;
    }
#endif
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
#ifdef SUPPORT_UPSCALING
        if (m_dithering_enabled || m_upscaling_enabled || m_rcas_enabled) {
            const auto postProcessStart = PostProcessClock::now();
            const auto upscalingStageStart = PostProcessClock::now();
            queue.submitCommands(cmdlist);
            vctx->queueSignalFence(&upscalingFence);
            vctx->queueWaitFence(&upscalingFence);

            if (m_upscaling_enabled) {
                m_video_render_stats.total_upscaling_time +=
                    toMicroseconds(PostProcessClock::now() - upscalingStageStart);
                m_video_render_stats.upscaled_frames++;
            }

            const bool submittedPostProcess = submitUpscalingPresentPass();
            vctx->queueFlush();

            if (submittedPostProcess) {
                m_video_render_stats.total_post_process_time +=
                    toMicroseconds(PostProcessClock::now() - postProcessStart);
                m_video_render_stats.post_processed_frames++;
            }
        } else {
            queue.submitCommands(cmdlist);
        }
#else
        queue.submitCommands(cmdlist);
#endif
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
        m_video_render_stats.post_processing_time = 0.0f;
        m_video_render_stats.dithering_time = 0.0f;
        m_video_render_stats.upscaling_time = 0.0f;
        m_video_render_stats.sharpening_time = 0.0f;
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

    m_video_render_stats.post_processing_time =
        m_video_render_stats.post_processed_frames
            ? ((float)m_video_render_stats.total_post_process_time /
               (float)m_video_render_stats.post_processed_frames) /
                  1000.0f
            : 0.0f;

    m_video_render_stats.dithering_time =
        m_video_render_stats.dithered_frames
            ? ((float)m_video_render_stats.total_dithering_time /
               (float)m_video_render_stats.dithered_frames) /
                  1000.0f
            : 0.0f;

    m_video_render_stats.upscaling_time =
        m_video_render_stats.upscaled_frames
            ? ((float)m_video_render_stats.total_upscaling_time /
               (float)m_video_render_stats.upscaled_frames) /
                  1000.0f
            : 0.0f;

    m_video_render_stats.sharpening_time =
        m_video_render_stats.sharpened_frames
            ? ((float)m_video_render_stats.total_sharpening_time /
               (float)m_video_render_stats.sharpened_frames) /
                  1000.0f
            : 0.0f;

    return &m_video_render_stats;
}

#endif
