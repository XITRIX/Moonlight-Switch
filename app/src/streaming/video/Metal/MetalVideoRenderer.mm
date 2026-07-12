#if defined(USE_METAL_RENDERER)

#define AVMediaType AVMediaType_FFmpeg
//#include <libavutil/pixdesc.h>
#undef AVMediaType

extern "C" {
    #include <libavutil/pixdesc.h>
}

#if !defined(__SDL3__)
#include <SDL2/SDL_syswm.h>
#endif

#include <borealis.hpp>
#include <borealis/platforms/sdl/sdl_video.hpp>
#include "MTShaders.hpp"
#include "streamutils.hpp"
#include "MetalVideoRenderer.hpp"
#include "Settings.hpp"
#include "UpscalingSupport.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>

#import <CoreVideo/CoreVideo.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <TargetConditionals.h>

#if defined(SUPPORT_UPSCALING) && !TARGET_OS_TV && !TARGET_OS_VISION
#define MOONLIGHT_ENABLE_METALFX_UPSCALING 1
#else
#define MOONLIGHT_ENABLE_METALFX_UPSCALING 0
#endif

#if MOONLIGHT_ENABLE_METALFX_UPSCALING
#import <MetalFX/MetalFX.h>
#endif

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif

#if TARGET_OS_OSX
#define AppView NSView
#else
#define AppView UIView
#endif

#define MAX_VIDEO_PLANES 3

struct MetalVideoRenderer::MetalRendererState {
    MTKView* metalView = nil;
    CAMetalLayer* metalLayer = nil;
    CVMetalTextureCacheRef textureCache = nullptr;
    id<MTLBuffer> cscParamsBuffer = nil;
    id<MTLBuffer> videoVertexBuffer = nil;
    id<MTLRenderPipelineState> videoPipelineState = nil;
    id<MTLRenderPipelineState> overlayPipelineState = nil;
    id<MTLLibrary> shaderLibrary = nil;
    id<MTLCommandQueue> commandQueue = nil;
#if defined(SUPPORT_UPSCALING)
    id<MTLBuffer> fullFrameVertexBuffer = nil;
    id<MTLBuffer> easuParamsBuffer = nil;
    id<MTLBuffer> postProcessParamsBuffer = nil;
    id<MTLBuffer> rcasParamsBuffer = nil;
    id<MTLRenderPipelineState> easuPipelineState = nil;
    id<MTLRenderPipelineState> postProcessPipelineState = nil;
    id<MTLRenderPipelineState> rcasPipelineState = nil;
    id<MTLTexture> upscalingInputTexture = nil;
    id<MTLTexture> upscalingOutputTexture = nil;
    id<MTLTexture> upscalingMotionTexture = nil;
    id<MTLTexture> upscalingDepthTexture = nil;
    id<MTLTexture> rcasOutputTexture = nil;
#if MOONLIGHT_ENABLE_METALFX_UPSCALING
    id<MTLFXTemporalScaler> temporalScaler = nil;
#endif
    bool metalFxSupported = false;
    bool loggedMetalFxUnsupported = false;
    UpscalingMode upscalingResourcesMode = UPSCALING_OFF;
    int upscalingInputWidth = 0;
    int upscalingInputHeight = 0;
    int upscalingOutputWidth = 0;
    int upscalingOutputHeight = 0;
    MTLPixelFormat upscalingPixelFormat = MTLPixelFormatInvalid;
#endif
    SDL_mutex* presentationMutex = SDL_CreateMutex();
    SDL_cond* presentationCond = SDL_CreateCond();
    int pendingPresentationCount = 0;

    ~MetalRendererState()
    {
        if (textureCache != nullptr) {
            CFRelease(textureCache);
        }
        if (presentationCond != nullptr) {
            SDL_DestroyCond(presentationCond);
        }
        if (presentationMutex != nullptr) {
            SDL_DestroyMutex(presentationMutex);
        }
    }
};

#define m_MetalView m_State->metalView
#define m_MetalLayer m_State->metalLayer
#define m_TextureCache m_State->textureCache
#define m_CscParamsBuffer m_State->cscParamsBuffer
#define m_VideoVertexBuffer m_State->videoVertexBuffer
#define m_VideoPipelineState m_State->videoPipelineState
#define m_OverlayPipelineState m_State->overlayPipelineState
#define m_ShaderLibrary m_State->shaderLibrary
#define m_CommandQueue m_State->commandQueue
#if defined(SUPPORT_UPSCALING)
#define m_FullFrameVertexBuffer m_State->fullFrameVertexBuffer
#define m_EasuParamsBuffer m_State->easuParamsBuffer
#define m_PostProcessParamsBuffer m_State->postProcessParamsBuffer
#define m_RcasParamsBuffer m_State->rcasParamsBuffer
#define m_EasuPipelineState m_State->easuPipelineState
#define m_PostProcessPipelineState m_State->postProcessPipelineState
#define m_RcasPipelineState m_State->rcasPipelineState
#define m_UpscalingInputTexture m_State->upscalingInputTexture
#define m_UpscalingOutputTexture m_State->upscalingOutputTexture
#define m_UpscalingMotionTexture m_State->upscalingMotionTexture
#define m_UpscalingDepthTexture m_State->upscalingDepthTexture
#define m_RcasOutputTexture m_State->rcasOutputTexture
#if MOONLIGHT_ENABLE_METALFX_UPSCALING
#define m_TemporalScaler m_State->temporalScaler
#endif
#define m_MetalFxSupported m_State->metalFxSupported
#define m_LoggedMetalFxUnsupported m_State->loggedMetalFxUnsupported
#define m_UpscalingResourcesMode m_State->upscalingResourcesMode
#define m_UpscalingInputWidth m_State->upscalingInputWidth
#define m_UpscalingInputHeight m_State->upscalingInputHeight
#define m_UpscalingOutputWidth m_State->upscalingOutputWidth
#define m_UpscalingOutputHeight m_State->upscalingOutputHeight
#define m_UpscalingPixelFormat m_State->upscalingPixelFormat
#endif
#define m_PresentationMutex m_State->presentationMutex
#define m_PresentationCond m_State->presentationCond
#define m_PendingPresentationCount m_State->pendingPresentationCount

#if defined(SUPPORT_UPSCALING)
#if __has_feature(objc_arc)
#define releaseObjCReference(obj) do { (obj) = nil; } while (0)
#else
#define releaseObjCReference(obj) do { [(obj) release]; (obj) = nil; } while (0)
#endif
#endif

static CGColorRef clearLayerColor()
{
#if TARGET_OS_OSX
    return NSColor.clearColor.CGColor;
#else
    return UIColor.clearColor.CGColor;
#endif
}

static void prepareOriginalMetalView(AppView* view)
{
#if TARGET_OS_OSX
    [view setWantsLayer:YES];
#else
    view.opaque = NO;
    view.backgroundColor = UIColor.clearColor;
#endif

    view.layer.backgroundColor = clearLayerColor();

    if ([view.layer isKindOfClass:[CAMetalLayer class]]) {
        CAMetalLayer* metalLayer = (CAMetalLayer*)view.layer;
        metalLayer.opaque = NO;
        metalLayer.backgroundColor = clearLayerColor();
    }
}

static void prepareMetalView(MTKView* metalView)
{
    metalView.clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);
    metalView.autoResizeDrawable = NO;
    metalView.paused = YES;
    metalView.enableSetNeedsDisplay = NO;
    metalView.preferredFramesPerSecond = 120;

#if TARGET_OS_OSX
    metalView.layer.backgroundColor = clearLayerColor();
    metalView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
#else
    metalView.opaque = NO;
    metalView.backgroundColor = UIColor.clearColor;
    metalView.userInteractionEnabled = NO;
    metalView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
#endif
}

static CGSize drawableSizeForMetalView(MTKView* metalView)
{
#if TARGET_OS_OSX
    if (metalView.window != nil) {
        NSRect backingRect = [metalView convertRectToBacking:metalView.bounds];
        return backingRect.size;
    }
#else
    if (metalView.window != nil) {
        CGFloat scale = metalView.layer.contentsScale;
        if (scale <= 0.0) {
            scale = metalView.contentScaleFactor;
        }
        return CGSizeMake(metalView.bounds.size.width * scale,
                          metalView.bounds.size.height * scale);
    }
#endif

    return metalView.bounds.size;
}

static AppView* getOriginalMetalView(SDL_Window* window)
{
    if (window == nullptr) {
        return nil;
    }

#if defined(__SDL3__)
    SDL_PropertiesID properties = SDL_GetWindowProperties(window);
#if TARGET_OS_OSX
    NSWindow* nsWindow = (__bridge NSWindow*)SDL_GetPointerProperty(
        properties, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
    NSView* contentView = nsWindow.contentView;
    if (contentView == nil || contentView.subviews.count == 0) {
        return nil;
    }

    return contentView.subviews[0];
#else
    UIWindow* uiWindow = (__bridge UIWindow*)SDL_GetPointerProperty(
        properties, SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, nullptr);
    if (uiWindow == nil || uiWindow.rootViewController == nil) {
        return nil;
    }

    return uiWindow.rootViewController.view;
#endif
#else
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);

    if (!SDL_GetWindowWMInfo(window, &info)) {
        return nil;
    }

#if TARGET_OS_OSX
    if (info.subsystem != SDL_SYSWM_COCOA) {
        return nil;
    }

    NSView* contentView = info.info.cocoa.window.contentView;
    if (contentView.subviews.count == 0) {
        return nil;
    }

    return contentView.subviews[0];
#else
    if (info.subsystem != SDL_SYSWM_UIKIT) {
        return nil;
    }

    UIWindow* uiWindow = info.info.uikit.window;
    if (uiWindow == nil || uiWindow.rootViewController == nil) {
        return nil;
    }

    return uiWindow.rootViewController.view;
#endif
#endif
}

struct CscParams
{
    vector_float3 matrix[3];
    vector_float3 offsets;
};

struct ParamBuffer
{
    CscParams cscParams;
    float bitnessScaleFactor;
};

struct PostProcessParams
{
    float control[4];
};

struct EasuParams
{
    float con0[4];
    float con1[4];
    float con2[4];
    float con3[4];
};

struct RcasParams
{
    float control[4];
};

static_assert(sizeof(EasuParams) == 64,
              "EasuParams must match the Metal shader layout");
static_assert(sizeof(PostProcessParams) == 16,
              "PostProcessParams must match the Metal shader layout");
static_assert(sizeof(RcasParams) == 16,
              "RcasParams must match the Metal shader layout");

static const CscParams k_CscParams_Bt601Lim = {
    // CSC Matrix
    {
        { 1.1644f, 0.0f, 1.5960f },
        { 1.1644f, -0.3917f, -0.8129f },
        { 1.1644f, 2.0172f, 0.0f }
    },

    // Offsets
    { 16.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f },
};
static const CscParams k_CscParams_Bt601Full = {
    // CSC Matrix
    {
        { 1.0f, 0.0f, 1.4020f },
        { 1.0f, -0.3441f, -0.7141f },
        { 1.0f, 1.7720f, 0.0f },
    },

    // Offsets
    { 0.0f, 128.0f / 255.0f, 128.0f / 255.0f },
};
static const CscParams k_CscParams_Bt709Lim = {
    // CSC Matrix
    {
        { 1.1644f, 0.0f, 1.7927f },
        { 1.1644f, -0.2132f, -0.5329f },
        { 1.1644f, 2.1124f, 0.0f },
    },

    // Offsets
    { 16.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f },
};
static const CscParams k_CscParams_Bt709Full = {
    // CSC Matrix
    {
        { 1.0f, 0.0f, 1.5748f },
        { 1.0f, -0.1873f, -0.4681f },
        { 1.0f, 1.8556f, 0.0f },
    },

    // Offsets
    { 0.0f, 128.0f / 255.0f, 128.0f / 255.0f },
};
static const CscParams k_CscParams_Bt2020Lim = {
    // CSC Matrix
    {
        { 1.1644f, 0.0f, 1.6781f },
        { 1.1644f, -0.1874f, -0.6505f },
        { 1.1644f, 2.1418f, 0.0f },
    },

    // Offsets
    { 16.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f },
};
static const CscParams k_CscParams_Bt2020Full = {
    // CSC Matrix
    {
        { 1.0f, 0.0f, 1.4746f },
        { 1.0f, -0.1646f, -0.5714f },
        { 1.0f, 1.8814f, 0.0f },
    },

    // Offsets
    { 0.0f, 128.0f / 255.0f, 128.0f / 255.0f },
};

struct Vertex
{
    vector_float4 position;
    vector_float2 texCoord;
};

static const std::array<Vertex, 4> FullFrameVertexData = {{
    {{-1.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    {{-1.0f, +1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
    {{+1.0f, -1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{+1.0f, +1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
}};

int getFramePlaneCount(AVFrame* frame)
{
    return CVPixelBufferGetPlaneCount((CVPixelBufferRef)frame->data[3]);
}

int getBitnessScaleFactor(AVFrame* frame) {
    // VideoToolbox frames never require scaling
    return 1;
}

bool MetalVideoRenderer::updateColorSpaceForFrame(AVFrame* frame) {
    int colorspace = getFrameColorspace(frame);
    bool fullRange = isFrameFullRange(frame);
    if (colorspace != m_LastColorSpace || fullRange != m_LastFullRange) {
        CGColorSpaceRef newColorSpace;
        ParamBuffer paramBuffer;

        switch (colorspace) {
        case COLORSPACE_REC_709:
            m_MetalLayer.colorspace = newColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceITUR_709);
            m_MetalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
            paramBuffer.cscParams = (fullRange ? k_CscParams_Bt709Full : k_CscParams_Bt709Lim);
            break;
        case COLORSPACE_REC_2020:
            // https://developer.apple.com/documentation/metal/hdr_content/using_color_spaces_to_display_hdr_content
            if (frame->color_trc == AVCOL_TRC_SMPTE2084) {
                m_MetalLayer.colorspace = newColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceITUR_2100_PQ);
                m_MetalLayer.pixelFormat = MTLPixelFormatBGR10A2Unorm;
            }
            else {
                m_MetalLayer.colorspace = newColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceITUR_2020);
                m_MetalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
            }
            paramBuffer.cscParams = (fullRange ? k_CscParams_Bt2020Full : k_CscParams_Bt2020Lim);
            break;
        default:
        case COLORSPACE_REC_601:
            m_MetalLayer.colorspace = newColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
            m_MetalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
            paramBuffer.cscParams = (fullRange ? k_CscParams_Bt601Full : k_CscParams_Bt601Lim);
            break;
        }

        paramBuffer.bitnessScaleFactor = getBitnessScaleFactor(frame);

        // The CAMetalLayer retains the CGColorSpace
        CGColorSpaceRelease(newColorSpace);

        // Create the new colorspace parameter buffer for our fragment shader
//        [m_CscParamsBuffer release];
        auto bufferOptions = MTLCPUCacheModeWriteCombined | MTLResourceStorageModeShared; //MTLResourceStorageModeManaged;
        m_CscParamsBuffer = [m_MetalLayer.device newBufferWithBytes:(void*)&paramBuffer length:sizeof(paramBuffer) options:bufferOptions];
        if (!m_CscParamsBuffer) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to create CSC parameters buffer");
            return false;
        }

        int planes = getFramePlaneCount(frame);
        SDL_assert(planes == 2 || planes == 3);

        MTLRenderPipelineDescriptor *pipelineDesc = [MTLRenderPipelineDescriptor new];
        pipelineDesc.vertexFunction = [m_ShaderLibrary newFunctionWithName:@"vs_draw"];
        pipelineDesc.fragmentFunction = [m_ShaderLibrary newFunctionWithName:planes == 2 ? @"ps_draw_biplanar" : @"ps_draw_triplanar"];
        pipelineDesc.colorAttachments[0].pixelFormat = m_MetalLayer.pixelFormat;
//        [m_VideoPipelineState release];
        m_VideoPipelineState = [m_MetalLayer.device newRenderPipelineStateWithDescriptor:pipelineDesc error:nullptr];
        if (!m_VideoPipelineState) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to create video pipeline state");
            return false;
        }

        pipelineDesc = [MTLRenderPipelineDescriptor new];
        pipelineDesc.vertexFunction = [m_ShaderLibrary newFunctionWithName:@"vs_draw"];
        pipelineDesc.fragmentFunction = [m_ShaderLibrary newFunctionWithName:@"ps_draw_rgb"];
        pipelineDesc.colorAttachments[0].pixelFormat = m_MetalLayer.pixelFormat;
        pipelineDesc.colorAttachments[0].blendingEnabled = YES;
        pipelineDesc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
        pipelineDesc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
        pipelineDesc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        pipelineDesc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
        pipelineDesc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        pipelineDesc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
//        [m_OverlayPipelineState release];
        m_OverlayPipelineState = [m_MetalLayer.device newRenderPipelineStateWithDescriptor:pipelineDesc error:nullptr];
        if (!m_OverlayPipelineState) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to create overlay pipeline state");
            return false;
        }

#if defined(SUPPORT_UPSCALING)
        pipelineDesc = [MTLRenderPipelineDescriptor new];
        pipelineDesc.vertexFunction = [m_ShaderLibrary newFunctionWithName:@"vs_draw"];
        pipelineDesc.fragmentFunction = [m_ShaderLibrary newFunctionWithName:@"ps_draw_easu"];
        pipelineDesc.colorAttachments[0].pixelFormat = m_MetalLayer.pixelFormat;
        m_EasuPipelineState =
            [m_MetalLayer.device newRenderPipelineStateWithDescriptor:pipelineDesc
                                                                error:nullptr];
        if (!m_EasuPipelineState) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to create FSR1 EASU pipeline state");
            return false;
        }

        pipelineDesc = [MTLRenderPipelineDescriptor new];
        pipelineDesc.vertexFunction = [m_ShaderLibrary newFunctionWithName:@"vs_draw"];
        pipelineDesc.fragmentFunction = [m_ShaderLibrary newFunctionWithName:@"ps_draw_postprocess"];
        pipelineDesc.colorAttachments[0].pixelFormat = m_MetalLayer.pixelFormat;
        m_PostProcessPipelineState =
            [m_MetalLayer.device newRenderPipelineStateWithDescriptor:pipelineDesc
                                                                error:nullptr];
        if (!m_PostProcessPipelineState) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to create post-process pipeline state");
            return false;
        }

        pipelineDesc = [MTLRenderPipelineDescriptor new];
        pipelineDesc.vertexFunction = [m_ShaderLibrary newFunctionWithName:@"vs_draw"];
        pipelineDesc.fragmentFunction = [m_ShaderLibrary newFunctionWithName:@"ps_draw_rcas"];
        pipelineDesc.colorAttachments[0].pixelFormat = m_MetalLayer.pixelFormat;
        m_RcasPipelineState =
            [m_MetalLayer.device newRenderPipelineStateWithDescriptor:pipelineDesc
                                                                error:nullptr];
        if (!m_RcasPipelineState) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to create RCAS pipeline state");
            return false;
        }

        releaseUpscalingResources();
#endif

        m_LastColorSpace = colorspace;
        m_LastFullRange = fullRange;
    }

    return true;
}

bool MetalVideoRenderer::updateVideoRegionSizeForFrame(AVFrame* frame) {
    CGSize drawableSize = drawableSizeForMetalView(m_MetalView);
    int drawableWidth = (int)drawableSize.width;
    int drawableHeight = (int)drawableSize.height;

    if (drawableWidth <= 0 || drawableHeight <= 0) {
        return false;
    }

    // Check if anything has changed since the last vertex buffer upload
    if (m_VideoVertexBuffer &&
            frame->width == m_LastFrameWidth && frame->height == m_LastFrameHeight &&
            drawableWidth == m_LastDrawableWidth && drawableHeight == m_LastDrawableHeight) {
        // Nothing to do
        return true;
    }

    m_MetalView.drawableSize = drawableSize;

    // Determine the correct scaled size for the video region
    SDL_Rect src, dst;
    src.x = src.y = 0;
    src.w = frame->width;
    src.h = frame->height;
    dst.x = dst.y = 0;
    dst.w = drawableWidth;
    dst.h = drawableHeight;
    StreamUtils::scaleSourceToDestinationSurface(&src, &dst);

    // Convert screen space to normalized device coordinates
    SDL_FRect renderRect;
    StreamUtils::screenSpaceToNormalizedDeviceCoords(&dst, &renderRect, drawableWidth, drawableHeight);

    Vertex verts[] =
    {
        { { renderRect.x, renderRect.y, 0.0f, 1.0f }, { 0.0f, 1.0f } },
        { { renderRect.x, renderRect.y+renderRect.h, 0.0f, 1.0f }, { 0.0f, 0} },
        { { renderRect.x+renderRect.w, renderRect.y, 0.0f, 1.0f }, { 1.0f, 1.0f} },
        { { renderRect.x+renderRect.w, renderRect.y+renderRect.h, 0.0f, 1.0f }, { 1.0f, 0} },
    };

//    [m_VideoVertexBuffer release];
    auto bufferOptions = MTLCPUCacheModeWriteCombined | MTLResourceStorageModeShared; //MTLResourceStorageModeManaged;
    m_VideoVertexBuffer = [m_MetalLayer.device newBufferWithBytes:verts length:sizeof(verts) options:bufferOptions];
    if (!m_VideoVertexBuffer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to create video vertex buffer");
        return false;
    }

    m_LastFrameWidth = frame->width;
    m_LastFrameHeight = frame->height;
    m_LastDrawableWidth = drawableWidth;
    m_LastDrawableHeight = drawableHeight;
    m_LastVideoRegionWidth = dst.w;
    m_LastVideoRegionHeight = dst.h;

    return true;
}

#if defined(SUPPORT_UPSCALING)
static const char* metalPixelFormatName(MTLPixelFormat pixelFormat) {
    switch (pixelFormat) {
    case MTLPixelFormatBGRA8Unorm:
        return "BGRA8Unorm";
    case MTLPixelFormatBGR10A2Unorm:
        return "BGR10A2Unorm";
    case MTLPixelFormatRGBA16Float:
        return "RGBA16Float";
    default:
        return "unknown";
    }
}

static bool metalFxSupportsDevice(id<MTLDevice> device) {
    if (device == nil) {
        return false;
    }

#if !MOONLIGHT_ENABLE_METALFX_UPSCALING
    return false;
#else
#if TARGET_OS_OSX
    if (@available(macOS 13.0, *)) {
#else
    if (@available(iOS 16.0, *)) {
#endif
        return [MTLFXTemporalScalerDescriptor supportsDevice:device];
    }

    return false;
#endif
}

bool MetalVideoRenderer::shouldUseUpscaling() const {
    return shouldUseMetalFxUpscaling() || shouldUseFsrUpscaling();
}

bool MetalVideoRenderer::shouldUseMetalFxUpscaling() const {
#if !MOONLIGHT_ENABLE_METALFX_UPSCALING
    return false;
#else
    return Settings::instance().upscaling_mode() == UPSCALING_METALFX &&
           m_MetalFxSupported &&
           m_LastFrameWidth > 0 && m_LastFrameHeight > 0 &&
           m_LastVideoRegionWidth > 0 && m_LastVideoRegionHeight > 0 &&
           m_LastVideoRegionWidth >= m_LastFrameWidth &&
           m_LastVideoRegionHeight >= m_LastFrameHeight &&
           (m_LastVideoRegionWidth > m_LastFrameWidth ||
            m_LastVideoRegionHeight > m_LastFrameHeight);
#endif
}

bool MetalVideoRenderer::shouldUseFsrUpscaling() const {
    return Settings::instance().upscaling_mode() == UPSCALING_FSR1 &&
           m_LastFrameWidth > 0 && m_LastFrameHeight > 0 &&
           m_LastVideoRegionWidth > 0 && m_LastVideoRegionHeight > 0 &&
           m_LastVideoRegionWidth >= m_LastFrameWidth &&
           m_LastVideoRegionHeight >= m_LastFrameHeight &&
           (m_LastVideoRegionWidth > m_LastFrameWidth ||
            m_LastVideoRegionHeight > m_LastFrameHeight);
}

bool MetalVideoRenderer::shouldUseDithering() const {
    return Settings::instance().dithering() &&
           Settings::instance().dithering_strength() > 0.0f &&
           m_LastFrameWidth > 0 && m_LastFrameHeight > 0 &&
           m_LastVideoRegionWidth > 0 && m_LastVideoRegionHeight > 0;
}

bool MetalVideoRenderer::shouldUseRcas() const {
    return Settings::instance().rcas() &&
           Settings::instance().rcas_strength() > 0.0f &&
           m_LastFrameWidth > 0 && m_LastFrameHeight > 0 &&
           m_LastVideoRegionWidth > 0 && m_LastVideoRegionHeight > 0;
}

void MetalVideoRenderer::updateEasuParams(int inputWidth,
                                          int inputHeight,
                                          int outputWidth,
                                          int outputHeight) {
    if (!m_EasuParamsBuffer || inputWidth <= 0 || inputHeight <= 0 ||
        outputWidth <= 0 || outputHeight <= 0) {
        return;
    }

    const float inputWidthRcp = 1.0f / static_cast<float>(inputWidth);
    const float inputHeightRcp = 1.0f / static_cast<float>(inputHeight);
    const float outputWidthRcp = 1.0f / static_cast<float>(outputWidth);
    const float outputHeightRcp = 1.0f / static_cast<float>(outputHeight);

    EasuParams params = {};
    params.con0[0] = static_cast<float>(inputWidth) * outputWidthRcp;
    params.con0[1] = static_cast<float>(inputHeight) * outputHeightRcp;
    params.con0[2] =
        0.5f * static_cast<float>(inputWidth) * outputWidthRcp - 0.5f;
    params.con0[3] =
        0.5f * static_cast<float>(inputHeight) * outputHeightRcp - 0.5f;

    params.con1[0] = inputWidthRcp;
    params.con1[1] = inputHeightRcp;
    params.con1[2] = inputWidthRcp;
    params.con1[3] = -inputHeightRcp;

    params.con2[0] = -inputWidthRcp;
    params.con2[1] = 2.0f * inputHeightRcp;
    params.con2[2] = inputWidthRcp;
    params.con2[3] = 2.0f * inputHeightRcp;

    params.con3[1] = 4.0f * inputHeightRcp;

    memcpy([m_EasuParamsBuffer contents], &params, sizeof(params));
}

void MetalVideoRenderer::updatePostProcessParams(bool ditheringEnabled) {
    if (!m_PostProcessParamsBuffer) {
        return;
    }

    PostProcessParams params = {};
    params.control[0] = ditheringEnabled ? 1.0f : 0.0f;
    params.control[1] =
        std::clamp(Settings::instance().dithering_strength(), 1.0f, 10.0f);
    memcpy([m_PostProcessParamsBuffer contents], &params, sizeof(params));
}

void MetalVideoRenderer::updateRcasParams() {
    if (!m_RcasParamsBuffer) {
        return;
    }

    const float strength =
        std::clamp(Settings::instance().rcas_strength(), 0.0f, 1.0f);
    const float sharpnessStops = 2.0f * (1.0f - strength);
    const float sharpnessLinear =
        static_cast<float>(std::exp2(-sharpnessStops));

    RcasParams params = {};
    params.control[0] = sharpnessLinear;
    memcpy([m_RcasParamsBuffer contents], &params, sizeof(params));
}

void MetalVideoRenderer::releaseUpscalingResources() {
#if MOONLIGHT_ENABLE_METALFX_UPSCALING
    releaseObjCReference(m_TemporalScaler);
#endif
    releaseObjCReference(m_UpscalingInputTexture);
    releaseObjCReference(m_UpscalingOutputTexture);
    releaseObjCReference(m_UpscalingMotionTexture);
    releaseObjCReference(m_UpscalingDepthTexture);
    releaseObjCReference(m_RcasOutputTexture);
    m_UpscalingResourcesMode = UPSCALING_OFF;
    m_UpscalingInputWidth = 0;
    m_UpscalingInputHeight = 0;
    m_UpscalingOutputWidth = 0;
    m_UpscalingOutputHeight = 0;
    m_UpscalingPixelFormat = MTLPixelFormatInvalid;
}

bool MetalVideoRenderer::ensureUpscalingResources(AVFrame* frame) {
    const bool wantMetalFxUpscaling = shouldUseMetalFxUpscaling();
    const bool wantFsrUpscaling = shouldUseFsrUpscaling();
    const bool wantUpscaling = wantMetalFxUpscaling || wantFsrUpscaling;
    const bool wantDithering = shouldUseDithering();
    const bool wantRcas = shouldUseRcas();
    const UpscalingMode requestedMode =
        wantMetalFxUpscaling ? UPSCALING_METALFX :
        wantFsrUpscaling ? UPSCALING_FSR1 : UPSCALING_OFF;

    if ((!wantUpscaling && !wantDithering && !wantRcas) || frame == nullptr ||
        m_MetalLayer.device == nil || !m_PostProcessPipelineState ||
        !m_PostProcessParamsBuffer) {
        return false;
    }
    if (wantFsrUpscaling && (!m_EasuPipelineState || !m_EasuParamsBuffer)) {
        return false;
    }

    const int inputWidth = frame->width;
    const int inputHeight = frame->height;
    const int outputWidth = m_LastVideoRegionWidth;
    const int outputHeight = m_LastVideoRegionHeight;
    const MTLPixelFormat pixelFormat = m_MetalLayer.pixelFormat;

    if (m_UpscalingOutputTexture &&
        (!wantMetalFxUpscaling ||
#if MOONLIGHT_ENABLE_METALFX_UPSCALING
         (m_TemporalScaler &&
#else
         (false &&
#endif
         m_UpscalingInputTexture &&
         m_UpscalingMotionTexture && m_UpscalingDepthTexture)) &&
        (!wantFsrUpscaling || m_UpscalingInputTexture) &&
        (!wantRcas || m_RcasOutputTexture) &&
        m_UpscalingResourcesMode == requestedMode &&
        m_UpscalingInputWidth == inputWidth &&
        m_UpscalingInputHeight == inputHeight &&
        m_UpscalingOutputWidth == outputWidth &&
        m_UpscalingOutputHeight == outputHeight &&
        m_UpscalingPixelFormat == pixelFormat) {
        return true;
    }

    releaseUpscalingResources();

    if (wantMetalFxUpscaling) {
#if !MOONLIGHT_ENABLE_METALFX_UPSCALING
        return false;
#else
#if TARGET_OS_OSX
        if (@available(macOS 13.0, *)) {
#else
        if (@available(iOS 16.0, *)) {
#endif
            MTLFXTemporalScalerDescriptor* descriptor =
                [[MTLFXTemporalScalerDescriptor alloc] init];
            descriptor.colorTextureFormat = pixelFormat;
            descriptor.depthTextureFormat = MTLPixelFormatDepth32Float;
            descriptor.motionTextureFormat = MTLPixelFormatRG16Float;
            descriptor.outputTextureFormat = pixelFormat;
            descriptor.inputWidth = inputWidth;
            descriptor.inputHeight = inputHeight;
            descriptor.outputWidth = outputWidth;
            descriptor.outputHeight = outputHeight;
            descriptor.autoExposureEnabled = YES;
            descriptor.requiresSynchronousInitialization = YES;

            m_TemporalScaler =
                [descriptor newTemporalScalerWithDevice:m_MetalLayer.device];
            releaseObjCReference(descriptor);
        } else {
            return false;
        }

        if (!m_TemporalScaler) {
            if (!m_LoggedMetalFxUnsupported) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "MetalFX temporal scaler is unavailable for %dx%d -> %dx%d format %lu",
                            inputWidth, inputHeight, outputWidth, outputHeight,
                            static_cast<unsigned long>(pixelFormat));
                m_LoggedMetalFxUnsupported = true;
            }
            releaseUpscalingResources();
            return false;
        }

        if (![m_TemporalScaler conformsToProtocol:@protocol(MTLFXTemporalScaler)]) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "MetalFX returned non-temporal scaler class: %s",
                         NSStringFromClass([m_TemporalScaler class]).UTF8String);
            releaseUpscalingResources();
            return false;
        }

        MTLTextureDescriptor* inputDesc =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pixelFormat
                                                               width:inputWidth
                                                              height:inputHeight
                                                           mipmapped:NO];
        inputDesc.storageMode = MTLStorageModePrivate;
        inputDesc.usage = MTLTextureUsageRenderTarget |
                          MTLTextureUsageShaderRead |
                          [m_TemporalScaler colorTextureUsage];

        MTLTextureDescriptor* motionDesc =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRG16Float
                                                               width:inputWidth
                                                              height:inputHeight
                                                           mipmapped:NO];
        motionDesc.storageMode = MTLStorageModePrivate;
        motionDesc.usage = MTLTextureUsageRenderTarget |
                           [m_TemporalScaler motionTextureUsage];

        MTLTextureDescriptor* depthDesc =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                               width:inputWidth
                                                              height:inputHeight
                                                           mipmapped:NO];
        depthDesc.storageMode = MTLStorageModePrivate;
        depthDesc.usage = MTLTextureUsageRenderTarget |
                          [m_TemporalScaler depthTextureUsage];

        m_UpscalingInputTexture =
            [m_MetalLayer.device newTextureWithDescriptor:inputDesc];
        m_UpscalingMotionTexture =
            [m_MetalLayer.device newTextureWithDescriptor:motionDesc];
        m_UpscalingDepthTexture =
            [m_MetalLayer.device newTextureWithDescriptor:depthDesc];

        if (!m_UpscalingInputTexture || !m_UpscalingMotionTexture ||
            !m_UpscalingDepthTexture) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Failed to allocate MetalFX upscaling textures");
            releaseUpscalingResources();
            return false;
        }
#endif
    } else if (wantFsrUpscaling) {
        MTLTextureDescriptor* inputDesc =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pixelFormat
                                                               width:inputWidth
                                                              height:inputHeight
                                                           mipmapped:NO];
        inputDesc.storageMode = MTLStorageModePrivate;
        inputDesc.usage =
            MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        m_UpscalingInputTexture =
            [m_MetalLayer.device newTextureWithDescriptor:inputDesc];
        if (!m_UpscalingInputTexture) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Failed to allocate FSR1 upscaling input texture");
            releaseUpscalingResources();
            return false;
        }
    }

    MTLTextureDescriptor* outputDesc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pixelFormat
                                                           width:outputWidth
                                                          height:outputHeight
                                                       mipmapped:NO];
    outputDesc.storageMode = MTLStorageModePrivate;
    outputDesc.usage = MTLTextureUsageShaderRead;
    if (wantMetalFxUpscaling) {
#if MOONLIGHT_ENABLE_METALFX_UPSCALING
        outputDesc.usage |= [m_TemporalScaler outputTextureUsage];
#endif
    } else {
        outputDesc.usage |= MTLTextureUsageRenderTarget;
    }
    m_UpscalingOutputTexture =
        [m_MetalLayer.device newTextureWithDescriptor:outputDesc];

    if (!m_UpscalingOutputTexture) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Failed to allocate Metal post-process texture");
        releaseUpscalingResources();
        return false;
    }

    if (wantRcas) {
        MTLTextureDescriptor* rcasDesc =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pixelFormat
                                                               width:outputWidth
                                                              height:outputHeight
                                                           mipmapped:NO];
        rcasDesc.storageMode = MTLStorageModePrivate;
        rcasDesc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        m_RcasOutputTexture =
            [m_MetalLayer.device newTextureWithDescriptor:rcasDesc];
        if (!m_RcasOutputTexture) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Failed to allocate Metal RCAS texture, skipping RCAS");
        }
    }

    m_UpscalingInputWidth = inputWidth;
    m_UpscalingInputHeight = inputHeight;
    m_UpscalingOutputWidth = outputWidth;
    m_UpscalingOutputHeight = outputHeight;
    m_UpscalingPixelFormat = pixelFormat;
    m_UpscalingResourcesMode = requestedMode;
    m_LoggedMetalFxUnsupported = false;

    if (wantMetalFxUpscaling) {
#if MOONLIGHT_ENABLE_METALFX_UPSCALING
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Using MetalFX temporal scaler: %s %dx%d -> %dx%d format %s colorUsage=0x%lx depthUsage=0x%lx motionUsage=0x%lx outputUsage=0x%lx",
                    NSStringFromClass([m_TemporalScaler class]).UTF8String,
                    inputWidth, inputHeight, outputWidth, outputHeight,
                    metalPixelFormatName(pixelFormat),
                    static_cast<unsigned long>([m_TemporalScaler colorTextureUsage]),
                    static_cast<unsigned long>([m_TemporalScaler depthTextureUsage]),
                    static_cast<unsigned long>([m_TemporalScaler motionTextureUsage]),
                    static_cast<unsigned long>([m_TemporalScaler outputTextureUsage]));
#endif
    } else if (wantFsrUpscaling) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Using FSR1 EASU scaler: %dx%d -> %dx%d format %s",
                    inputWidth, inputHeight, outputWidth, outputHeight,
                    metalPixelFormatName(pixelFormat));
    }

    return true;
}
#endif

MetalVideoRenderer::MetalVideoRenderer()
    : m_State(new MetalRendererState())
{}

MetalVideoRenderer::~MetalVideoRenderer()
{@autoreleasepool {
#if defined(SUPPORT_UPSCALING)
    releaseUpscalingResources();
    releaseObjCReference(m_FullFrameVertexBuffer);
    releaseObjCReference(m_EasuParamsBuffer);
    releaseObjCReference(m_PostProcessParamsBuffer);
    releaseObjCReference(m_RcasParamsBuffer);
    releaseObjCReference(m_EasuPipelineState);
    releaseObjCReference(m_PostProcessPipelineState);
    releaseObjCReference(m_RcasPipelineState);
#endif
    if (m_MetalView != nil) {
        [m_MetalView removeFromSuperview];
    }
    delete m_State;
    m_State = nullptr;
}}

bool MetalVideoRenderer::waitToRender()
{ @autoreleasepool {
    SDL_LockMutex(m_PresentationMutex);

#if TARGET_OS_OSX
    // On macOS, blocking the UI render loop here causes visible stutter bursts
    // when the compositor or drawable presentation falls behind.
    const bool canRender = m_PendingPresentationCount < (int)m_MetalLayer.maximumDrawableCount;
    SDL_UnlockMutex(m_PresentationMutex);
    return canRender;
#else
    // Pace ourselves by waiting if too many frames are pending presentation
    if (m_PendingPresentationCount > 2) {
#if defined(__SDL3__)
        if (!SDL_WaitConditionTimeout(m_PresentationCond, m_PresentationMutex, 100)) {
#else
        if (SDL_CondWaitTimeout(m_PresentationCond, m_PresentationMutex, 100) == SDL_MUTEX_TIMEDOUT) {
#endif
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Presentation wait timed out after 100 ms");
        }
    }
    SDL_UnlockMutex(m_PresentationMutex);
    return true;
#endif
}}

void MetalVideoRenderer::draw(NVGcontext* vg, int width, int height, AVFrame* frame, int imageFormat) {
    if (!initialize(imageFormat)) {
        return;
    }

    if (frame->format != AV_PIX_FMT_VIDEOTOOLBOX) { return; }

    CVPixelBufferRef pixBuf = reinterpret_cast<CVPixelBufferRef>(frame->data[3]);
    if (!pixBuf || !m_TextureCache) {
        return;
    }

    // Handle changes to the frame's colorspace from last time we rendered
    if (!updateColorSpaceForFrame(frame)) {
        // Trigger the main thread to recreate the decoder
//        SDL_Event event;
//        event.type = SDL_RENDER_DEVICE_RESET;
//        SDL_PushEvent(&event);
        return;
    }

    // Handle changes to the video size or drawable size
    if (!updateVideoRegionSizeForFrame(frame)) {
        // Trigger the main thread to recreate the decoder
//        SDL_Event event;
//        event.type = SDL_RENDER_DEVICE_RESET;
//        SDL_PushEvent(&event);
        return;
    }

    if (!waitToRender()) {
        return;
    }

    id<CAMetalDrawable> nextDrawable = [m_MetalLayer nextDrawable];

    if (nextDrawable != nil && ![nextDrawable respondsToSelector:@selector(texture)]) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Metal drawable has unexpected class during draw: %s",
                     NSStringFromClass([nextDrawable class]).UTF8String);
        return;
    }

    if (nextDrawable == nil) {
        return;
    }

    std::array<CVMetalTextureRef, MAX_VIDEO_PLANES> cvMetalTextures;
    size_t planes = getFramePlaneCount(frame);
//    SDL_assert(planes <= MAX_VIDEO_PLANES);

    // Create Metal textures for the planes of the CVPixelBuffer
    for (size_t i = 0; i < planes; i++) {
        MTLPixelFormat fmt;

        switch (CVPixelBufferGetPixelFormatType(pixBuf)) {
        case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
        case kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange:
        case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
        case kCVPixelFormatType_444YpCbCr8BiPlanarFullRange:
            fmt = (i == 0) ? MTLPixelFormatR8Unorm : MTLPixelFormatRG8Unorm;
            break;

        case kCVPixelFormatType_420YpCbCr10BiPlanarFullRange:
        case kCVPixelFormatType_444YpCbCr10BiPlanarFullRange:
        case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:
        case kCVPixelFormatType_444YpCbCr10BiPlanarVideoRange:
            fmt = (i == 0) ? MTLPixelFormatR16Unorm : MTLPixelFormatRG16Unorm;
            break;

        default:
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Unknown pixel format: %x",
                         CVPixelBufferGetPixelFormatType(pixBuf));
            return;
        }

        CVReturn err = CVMetalTextureCacheCreateTextureFromImage(kCFAllocatorDefault, m_TextureCache, pixBuf, nullptr, fmt,
                                                                 CVPixelBufferGetWidthOfPlane(pixBuf, i),
                                                                 CVPixelBufferGetHeightOfPlane(pixBuf, i),
                                                                 i,
                                                                 &cvMetalTextures[i]);
        if (err != kCVReturnSuccess) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "CVMetalTextureCacheCreateTextureFromImage() failed: %d",
                         err);
            return;
        }
    }

    id<MTLTexture> drawableTexture = [nextDrawable texture];
    if (drawableTexture == nil) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to acquire Metal drawable texture");
        return;
    }

    uint64_t before_render = LiGetMillis();
    if (!m_video_render_stats_progress.rendered_frames) {
        m_video_render_stats_progress.measurement_start_timestamp = before_render;
    }

    auto commandBuffer = [m_CommandQueue commandBuffer];
    [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer>) {
        // Free textures after completion of rendering per CVMetalTextureCache requirements
        for (size_t i = 0; i < planes; i++) {
            CFRelease(cvMetalTextures[i]);
        }
    }];

#if defined(SUPPORT_UPSCALING)
    const bool requestedMetalFxUpscaling = shouldUseMetalFxUpscaling();
    const bool requestedFsrUpscaling = shouldUseFsrUpscaling();
    const bool requestedUpscaling =
        requestedMetalFxUpscaling || requestedFsrUpscaling;
    const bool requestedDithering = shouldUseDithering();
    const bool requestedRcas = shouldUseRcas();
    const bool postProcessReady =
        (requestedUpscaling || requestedDithering || requestedRcas) &&
        ensureUpscalingResources(frame);
    bool useMetalFxUpscaling = false;
    bool useFsrUpscaling = false;
    bool useDithering = false;
    bool useRcas = false;
    bool canRenderFsrDirectToDrawable = false;

    if (postProcessReady) {
        canRenderFsrDirectToDrawable =
            requestedFsrUpscaling && !requestedDithering && !requestedRcas &&
            drawableTexture != nil &&
            m_LastVideoRegionWidth == static_cast<int>([drawableTexture width]) &&
            m_LastVideoRegionHeight == static_cast<int>([drawableTexture height]);
#if MOONLIGHT_ENABLE_METALFX_UPSCALING
        useMetalFxUpscaling =
            requestedMetalFxUpscaling && m_TemporalScaler &&
            m_UpscalingInputTexture && m_UpscalingMotionTexture &&
            m_UpscalingDepthTexture && m_UpscalingOutputTexture;
#endif
        useFsrUpscaling = requestedFsrUpscaling && m_EasuPipelineState &&
                          m_EasuParamsBuffer && m_UpscalingInputTexture &&
                          m_UpscalingOutputTexture;
        useDithering = requestedDithering && m_PostProcessPipelineState &&
                       m_PostProcessParamsBuffer;
        useRcas = requestedRcas && m_RcasPipelineState && m_RcasParamsBuffer &&
                  m_RcasOutputTexture;
    }

    if (useMetalFxUpscaling || useFsrUpscaling || useDithering || useRcas) {
        const uint64_t postProcessStart = LiGetMillis();
        id<MTLTexture> finalTexture = m_UpscalingOutputTexture;
        bool renderedDirectToDrawable = false;

        auto renderVideoToTexture = [&](id<MTLTexture> targetTexture) -> bool {
            auto sourcePassDescriptor =
                [MTLRenderPassDescriptor renderPassDescriptor];
            MTLRenderPassColorAttachmentDescriptor* sourceAttachment =
                [sourcePassDescriptor.colorAttachments objectAtIndexedSubscript:0];
            if (sourceAttachment == nil) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "Failed to acquire Metal source attachment descriptor");
                return false;
            }
            [sourceAttachment setTexture:targetTexture];
            [sourceAttachment setLoadAction:MTLLoadActionClear];
            [sourceAttachment setClearColor:MTLClearColorMake(0.0, 0.0, 0.0, 0.0)];
            [sourceAttachment setStoreAction:MTLStoreActionStore];

            auto sourceEncoder =
                [commandBuffer renderCommandEncoderWithDescriptor:sourcePassDescriptor];
            [sourceEncoder setRenderPipelineState:m_VideoPipelineState];
            for (size_t i = 0; i < planes; i++) {
                [sourceEncoder setFragmentTexture:CVMetalTextureGetTexture(cvMetalTextures[i])
                                          atIndex:i];
            }
            [sourceEncoder setFragmentBuffer:m_CscParamsBuffer offset:0 atIndex:0];
            [sourceEncoder setVertexBuffer:m_FullFrameVertexBuffer offset:0 atIndex:0];
            [sourceEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
                              vertexStart:0
                              vertexCount:4];
            [sourceEncoder endEncoding];
            return true;
        };

        if (useMetalFxUpscaling) {
#if MOONLIGHT_ENABLE_METALFX_UPSCALING
            const uint64_t upscalingStart = LiGetMillis();
            if (!renderVideoToTexture(m_UpscalingInputTexture)) {
                return;
            }

            auto motionPassDescriptor =
                [MTLRenderPassDescriptor renderPassDescriptor];
            MTLRenderPassColorAttachmentDescriptor* motionAttachment =
                [motionPassDescriptor.colorAttachments objectAtIndexedSubscript:0];
            if (motionAttachment == nil) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "Failed to acquire MetalFX motion attachment descriptor");
                return;
            }
            [motionAttachment setTexture:m_UpscalingMotionTexture];
            [motionAttachment setLoadAction:MTLLoadActionClear];
            [motionAttachment setClearColor:MTLClearColorMake(0.0, 0.0, 0.0, 0.0)];
            [motionAttachment setStoreAction:MTLStoreActionStore];
            auto motionEncoder =
                [commandBuffer renderCommandEncoderWithDescriptor:motionPassDescriptor];
            [motionEncoder endEncoding];

            auto depthPassDescriptor =
                [MTLRenderPassDescriptor renderPassDescriptor];
            MTLRenderPassDepthAttachmentDescriptor* depthAttachment =
                [depthPassDescriptor depthAttachment];
            if (depthAttachment == nil) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "Failed to acquire MetalFX depth attachment descriptor");
                return;
            }
            [depthAttachment setTexture:m_UpscalingDepthTexture];
            [depthAttachment setLoadAction:MTLLoadActionClear];
            [depthAttachment setClearDepth:1.0];
            [depthAttachment setStoreAction:MTLStoreActionStore];
            auto depthEncoder =
                [commandBuffer renderCommandEncoderWithDescriptor:depthPassDescriptor];
            [depthEncoder endEncoding];

            [m_TemporalScaler setColorTexture:m_UpscalingInputTexture];
            [m_TemporalScaler setDepthTexture:m_UpscalingDepthTexture];
            [m_TemporalScaler setMotionTexture:m_UpscalingMotionTexture];
            [m_TemporalScaler setInputContentWidth:frame->width];
            [m_TemporalScaler setInputContentHeight:frame->height];
            [m_TemporalScaler setOutputTexture:m_UpscalingOutputTexture];
            [m_TemporalScaler setJitterOffsetX:0.0f];
            [m_TemporalScaler setJitterOffsetY:0.0f];
            [m_TemporalScaler setMotionVectorScaleX:1.0f];
            [m_TemporalScaler setMotionVectorScaleY:1.0f];
            [m_TemporalScaler setDepthReversed:NO];
            [m_TemporalScaler setPreExposure:1.0f];
            [m_TemporalScaler setReset:YES];
            [m_TemporalScaler encodeToCommandBuffer:commandBuffer];

            m_video_render_stats_progress.total_upscaling_time +=
                LiGetMillis() - upscalingStart;
            m_video_render_stats_progress.upscaled_frames++;
#endif
        } else if (useFsrUpscaling) {
            const uint64_t upscalingStart = LiGetMillis();
            if (!renderVideoToTexture(m_UpscalingInputTexture)) {
                return;
            }

            id<MTLTexture> easuTargetTexture =
                canRenderFsrDirectToDrawable ? drawableTexture : m_UpscalingOutputTexture;
            updateEasuParams(frame->width, frame->height,
                             static_cast<int>([easuTargetTexture width]),
                             static_cast<int>([easuTargetTexture height]));

            auto easuPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
            MTLRenderPassColorAttachmentDescriptor* easuAttachment =
                [easuPassDescriptor.colorAttachments objectAtIndexedSubscript:0];
            if (easuAttachment == nil) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "Failed to acquire Metal FSR1 attachment descriptor");
                return;
            }
            [easuAttachment setTexture:easuTargetTexture];
            [easuAttachment setLoadAction:MTLLoadActionClear];
            [easuAttachment setClearColor:MTLClearColorMake(0.0, 0.0, 0.0, 0.0)];
            [easuAttachment setStoreAction:MTLStoreActionStore];

            auto easuEncoder =
                [commandBuffer renderCommandEncoderWithDescriptor:easuPassDescriptor];
            [easuEncoder setRenderPipelineState:m_EasuPipelineState];
            [easuEncoder setFragmentTexture:m_UpscalingInputTexture atIndex:0];
            [easuEncoder setFragmentBuffer:m_EasuParamsBuffer offset:0 atIndex:0];
            [easuEncoder setVertexBuffer:m_FullFrameVertexBuffer offset:0 atIndex:0];
            [easuEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
                            vertexStart:0
                            vertexCount:4];
            [easuEncoder endEncoding];

            m_video_render_stats_progress.total_upscaling_time +=
                LiGetMillis() - upscalingStart;
            m_video_render_stats_progress.upscaled_frames++;
            renderedDirectToDrawable = easuTargetTexture == drawableTexture;
        } else if (!renderVideoToTexture(m_UpscalingOutputTexture)) {
            return;
        }

        if (useRcas) {
            const uint64_t sharpeningStart = LiGetMillis();
            updateRcasParams();

            auto rcasPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
            MTLRenderPassColorAttachmentDescriptor* rcasAttachment =
                [rcasPassDescriptor.colorAttachments objectAtIndexedSubscript:0];
            if (rcasAttachment == nil) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "Failed to acquire Metal RCAS attachment descriptor");
                return;
            }
            [rcasAttachment setTexture:m_RcasOutputTexture];
            [rcasAttachment setLoadAction:MTLLoadActionClear];
            [rcasAttachment setClearColor:MTLClearColorMake(0.0, 0.0, 0.0, 0.0)];
            [rcasAttachment setStoreAction:MTLStoreActionStore];

            auto rcasEncoder =
                [commandBuffer renderCommandEncoderWithDescriptor:rcasPassDescriptor];
            [rcasEncoder setRenderPipelineState:m_RcasPipelineState];
            [rcasEncoder setFragmentTexture:finalTexture atIndex:0];
            [rcasEncoder setFragmentBuffer:m_RcasParamsBuffer offset:0 atIndex:0];
            [rcasEncoder setVertexBuffer:m_FullFrameVertexBuffer offset:0 atIndex:0];
            [rcasEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
                            vertexStart:0
                            vertexCount:4];
            [rcasEncoder endEncoding];
            finalTexture = m_RcasOutputTexture;

            m_video_render_stats_progress.total_sharpening_time +=
                LiGetMillis() - sharpeningStart;
            m_video_render_stats_progress.sharpened_frames++;
        }

        if (!renderedDirectToDrawable) {
            const uint64_t ditheringStart = LiGetMillis();
            updatePostProcessParams(useDithering);

            auto presentPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
            MTLRenderPassColorAttachmentDescriptor* presentAttachment =
                [presentPassDescriptor.colorAttachments objectAtIndexedSubscript:0];
            if (presentAttachment == nil) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "Failed to acquire Metal post-process present attachment descriptor");
                return;
            }
            [presentAttachment setTexture:drawableTexture];
            [presentAttachment setLoadAction:MTLLoadActionClear];
            [presentAttachment setClearColor:MTLClearColorMake(0.0, 0.0, 0.0, 0.0)];
            [presentAttachment setStoreAction:MTLStoreActionStore];

            auto presentEncoder =
                [commandBuffer renderCommandEncoderWithDescriptor:presentPassDescriptor];
            [presentEncoder setRenderPipelineState:m_PostProcessPipelineState];
            [presentEncoder setFragmentTexture:finalTexture atIndex:0];
            [presentEncoder setFragmentBuffer:m_PostProcessParamsBuffer offset:0 atIndex:0];
            [presentEncoder setVertexBuffer:m_VideoVertexBuffer offset:0 atIndex:0];
            [presentEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
                                vertexStart:0
                                vertexCount:4];
            [presentEncoder endEncoding];

            if (useDithering) {
                m_video_render_stats_progress.total_dithering_time +=
                    LiGetMillis() - ditheringStart;
                m_video_render_stats_progress.dithered_frames++;
            }
        }

        m_video_render_stats_progress.total_post_process_time +=
            LiGetMillis() - postProcessStart;
        m_video_render_stats_progress.post_processed_frames++;
    } else
#endif
    {
        auto renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
        MTLRenderPassColorAttachmentDescriptor* colorAttachment =
            [renderPassDescriptor.colorAttachments objectAtIndexedSubscript:0];
        if (colorAttachment == nil) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to acquire Metal color attachment descriptor");
            return;
        }

        [colorAttachment setTexture:drawableTexture];
        [colorAttachment setLoadAction:MTLLoadActionClear];
        [colorAttachment setClearColor:MTLClearColorMake(0.0, 0.0, 0.0, 0.0)];
        [colorAttachment setStoreAction:MTLStoreActionStore];

        auto renderEncoder =
            [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];

        // Bind textures and buffers then draw the video region
        [renderEncoder setRenderPipelineState:m_VideoPipelineState];
        for (size_t i = 0; i < planes; i++) {
            [renderEncoder setFragmentTexture:CVMetalTextureGetTexture(cvMetalTextures[i])
                                      atIndex:i];
        }

        [renderEncoder setFragmentBuffer:m_CscParamsBuffer offset:0 atIndex:0];
        [renderEncoder setVertexBuffer:m_VideoVertexBuffer offset:0 atIndex:0];
        [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
                           vertexStart:0
                           vertexCount:4];

        // Now draw any overlays that are enabled
        //

        [renderEncoder endEncoding];
    }

//    if (m_MetalLayer.displaySyncEnabled) {
    // Queue a completion callback on the drawable to pace our rendering
    SDL_LockMutex(m_PresentationMutex);
    m_PendingPresentationCount++;
    SDL_UnlockMutex(m_PresentationMutex);
    [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> completedCommandBuffer) {
        if (@available(macOS 10.15, iOS 10.3, tvOS 10.3, *)) {
            if (completedCommandBuffer.status == MTLCommandBufferStatusCompleted &&
                completedCommandBuffer.GPUEndTime > completedCommandBuffer.GPUStartTime) {
                const double gpuTimeUs =
                    (completedCommandBuffer.GPUEndTime -
                     completedCommandBuffer.GPUStartTime) *
                    1000000.0;
                m_gpu_render_time_total_us.fetch_add(
                    static_cast<uint64_t>(gpuTimeUs + 0.5),
                    std::memory_order_relaxed);
                m_gpu_timed_frames.fetch_add(1, std::memory_order_relaxed);
            }
        }

        SDL_LockMutex(m_PresentationMutex);
        m_PendingPresentationCount--;
        SDL_CondSignal(m_PresentationCond);
        SDL_UnlockMutex(m_PresentationMutex);
    }];
//    }

    // Flip to the newly rendered buffer
    [commandBuffer presentDrawable:nextDrawable];
    [commandBuffer commit];

    // tvOS shares this thread with the Borealis UI loop, so waiting here
    // stalls overlay animation even though presentation backpressure is already
    // handled via m_PendingPresentationCount and the completed handlers above.
#if !TARGET_OS_OSX && !TARGET_OS_TV
    [commandBuffer waitUntilCompleted];
#endif

    const uint64_t render_time = LiGetMillis() - before_render;
    m_video_render_stats_progress.total_render_time += render_time;
    m_video_render_stats_progress.rendered_frames++;

    const uint64_t stats_interval_ms = 200;
    const uint64_t stats_now = LiGetMillis();
    if (stats_now - m_video_render_stats_progress.measurement_start_timestamp >=
        stats_interval_ms) {
        m_video_render_stats_cache = m_video_render_stats_progress;
        m_video_render_stats_progress = {};
        m_video_render_stats_cache.gpu_timed_frames =
            m_gpu_timed_frames.exchange(0, std::memory_order_relaxed);
        m_video_render_stats_cache.total_gpu_render_time_us =
            m_gpu_render_time_total_us.exchange(0, std::memory_order_relaxed);

        const uint64_t elapsed_time =
            stats_now - m_video_render_stats_cache.measurement_start_timestamp;
        m_video_render_stats_cache.rendered_fps =
            elapsed_time && m_video_render_stats_cache.rendered_frames > 1
                ? (float)(m_video_render_stats_cache.rendered_frames - 1) /
                      ((float)elapsed_time / 1000.0f)
                : 0.0f;

        m_video_render_stats_cache.rendering_time =
            m_video_render_stats_cache.rendered_frames
                ? (float)m_video_render_stats_cache.total_render_time /
                      (float)m_video_render_stats_cache.rendered_frames
                : 0.0f;

        m_video_render_stats_cache.post_processing_time =
            m_video_render_stats_cache.post_processed_frames
                ? (float)m_video_render_stats_cache.total_post_process_time /
                      (float)m_video_render_stats_cache.post_processed_frames
                : 0.0f;

        m_video_render_stats_cache.upscaling_time =
            m_video_render_stats_cache.upscaled_frames
                ? (float)m_video_render_stats_cache.total_upscaling_time /
                      (float)m_video_render_stats_cache.upscaled_frames
                : 0.0f;

        m_video_render_stats_cache.dithering_time =
            m_video_render_stats_cache.dithered_frames
                ? (float)m_video_render_stats_cache.total_dithering_time /
                      (float)m_video_render_stats_cache.dithered_frames
                : 0.0f;

        m_video_render_stats_cache.sharpening_time =
            m_video_render_stats_cache.sharpened_frames
                ? (float)m_video_render_stats_cache.total_sharpening_time /
                      (float)m_video_render_stats_cache.sharpened_frames
                : 0.0f;

        m_video_render_stats_cache.gpu_rendering_time =
            m_video_render_stats_cache.gpu_timed_frames
                ? ((float)m_video_render_stats_cache.total_gpu_render_time_us /
                   1000.0f) /
                      (float)m_video_render_stats_cache.gpu_timed_frames
                : 0.0f;
    }
}

id<MTLDevice> getMetalDevice() {
#if defined(PLATFORM_VISIONOS)
    return MTLCreateSystemDefaultDevice();
#else
    if (@available(iOS 18.0, tvOS 18.0, *)) {
        NSArray<id<MTLDevice>> *devices = MTLCopyAllDevices();
        if (devices.count == 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "No Metal device found!");
            return nullptr;
        }
        
        for (id<MTLDevice> device in devices) {
            if (device.hasUnifiedMemory) {
                return device;
            }
        }
    }
    
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Using Metal renderer due to VT_FORCE_METAL=1 override.");
    return MTLCreateSystemDefaultDevice();
#endif
}

bool MetalVideoRenderer::initialize(int imageFormat)
{ @autoreleasepool {
    if (initialized) {
        return true;
    }

    auto videoContext = (brls::SDLVideoContext*) brls::Application::getPlatform()->getVideoContext();
    m_Window = videoContext->getSDLWindow();

    id<MTLDevice> device = getMetalDevice();
    if (!device) {
        return false;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Selected Metal device: %s",
                device.name.UTF8String);

#if defined(SUPPORT_UPSCALING)
    m_MetalFxSupported = metalFxSupportsDevice(device);
#if MOONLIGHT_ENABLE_METALFX_UPSCALING
    if (!m_MetalFxSupported) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "MetalFX temporal upscaling is unavailable on this OS or device");
    }
#endif
#endif

//    if (!checkDecoderCapabilities(device, params)) {
//        return false;
//    }

    AppView* originalMetalView = getOriginalMetalView(m_Window);
    AppView* containerView = originalMetalView != nil ? originalMetalView.superview : nil;
    if (originalMetalView == nil || containerView == nil) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to locate SDL Metal host view");
        return false;
    }

    prepareOriginalMetalView(originalMetalView);

    m_MetalView = [[MTKView alloc] initWithFrame:originalMetalView.frame device:device];
    prepareMetalView(m_MetalView);

#if TARGET_OS_OSX
    [containerView addSubview:m_MetalView positioned:NSWindowBelow relativeTo:originalMetalView];
#else
    [containerView insertSubview:m_MetalView atIndex:0];
#endif

    m_MetalLayer = (CAMetalLayer*)[m_MetalView layer];
    m_MetalLayer.opaque = NO;
    m_MetalLayer.backgroundColor = clearLayerColor();

    // Choose a device
    m_MetalLayer.device = device;

    // Allow EDR content if we're streaming in a 10-bit format
#if defined(PLATFORM_IOS) || defined(PLATFORM_VISIONOS)
    if (@available(iOS 16.0, tvOS 16.0, visionOS 1.0, *)) {
        m_MetalLayer.wantsExtendedDynamicRangeContent = imageFormat & VIDEO_FORMAT_MASK_10BIT;
    }
#endif

    // Ideally, we don't actually want triple buffering due to increased
    // display latency, since our render time is very short. However, we
    // *need* 3 drawables in order to hit the offloaded "direct" display
    // path for our Metal layer.
    //
    // If we only use 2 drawables, we'll be stuck in the composited path
    // (particularly for windowed mode) and our latency will actually be
    // higher than opting for triple buffering.
    m_MetalLayer.maximumDrawableCount = 3;

    // Allow tearing if V-Sync is off (also requires direct display path)
//    m_MetalLayer.displaySyncEnabled = params->enableVsync;

    // Create the Metal texture cache for our CVPixelBuffers
    CFStringRef keys[1] = { kCVMetalTextureUsage };
    NSUInteger values[1] = { MTLTextureUsageShaderRead };
    auto cacheAttributes = CFDictionaryCreate(kCFAllocatorDefault, (const void**)keys, (const void**)values, 1, nullptr, nullptr);
    CVReturn err = CVMetalTextureCacheCreate(kCFAllocatorDefault, cacheAttributes, m_MetalLayer.device, nullptr, &m_TextureCache);
    CFRelease(cacheAttributes);

    if (err != kCVReturnSuccess) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "CVMetalTextureCacheCreate() failed: %d",
                     err);
        return false;
    }

    // Compile our shaders
    auto shader = [[NSString alloc] initWithCString: metal_shader encoding: NSUTF8StringEncoding];
    m_ShaderLibrary = [m_MetalLayer.device newLibraryWithSource: shader options:nullptr error:nullptr];
    if (!m_ShaderLibrary) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to compile shaders");
        return false;
    }

    // Create a command queue for submission
    m_CommandQueue = [m_MetalLayer.device newCommandQueue];
    if (!m_CommandQueue) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to create Metal command queue");
        return false;
    }

#if defined(SUPPORT_UPSCALING)
    MTLResourceOptions bufferOptions = static_cast<MTLResourceOptions>(
        static_cast<NSUInteger>(MTLCPUCacheModeWriteCombined) |
        static_cast<NSUInteger>(MTLResourceStorageModeShared));
    m_FullFrameVertexBuffer =
        [m_MetalLayer.device newBufferWithBytes:FullFrameVertexData.data()
                                         length:sizeof(Vertex) * FullFrameVertexData.size()
                                        options:bufferOptions];
    if (!m_FullFrameVertexBuffer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to create Metal upscaling source vertex buffer");
        return false;
    }

    EasuParams easuParams = {};
    m_EasuParamsBuffer =
        [m_MetalLayer.device newBufferWithBytes:&easuParams
                                         length:sizeof(easuParams)
                                        options:bufferOptions];
    if (!m_EasuParamsBuffer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to create Metal FSR1 EASU parameters buffer");
        return false;
    }

    PostProcessParams postProcessParams = {};
    postProcessParams.control[1] = 3.0f;
    m_PostProcessParamsBuffer =
        [m_MetalLayer.device newBufferWithBytes:&postProcessParams
                                         length:sizeof(postProcessParams)
                                        options:bufferOptions];
    if (!m_PostProcessParamsBuffer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to create Metal post-process parameters buffer");
        return false;
    }

    RcasParams rcasParams = {};
    rcasParams.control[0] =
        static_cast<float>(std::exp2(-2.0f * (1.0f - 0.2f)));
    m_RcasParamsBuffer =
        [m_MetalLayer.device newBufferWithBytes:&rcasParams
                                         length:sizeof(rcasParams)
                                        options:bufferOptions];
    if (!m_RcasParamsBuffer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to create Metal RCAS parameters buffer");
        return false;
    }
#endif

    initialized = true;
    return true;
}}

VideoRenderStats* MetalVideoRenderer::video_render_stats() {
    return (VideoRenderStats*)&m_video_render_stats_cache;
}

#endif // USE_METAL_RENDERER
