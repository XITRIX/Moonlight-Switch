#if defined(USE_METAL_RENDERER)

#define AVMediaType AVMediaType_FFmpeg
//#include <libavutil/pixdesc.h>
#undef AVMediaType

extern "C" {
    #include <libavutil/pixdesc.h>
}

#include <SDL2/SDL_syswm.h>

#include <borealis.hpp>
#include <borealis/platforms/sdl/sdl_video.hpp>
#include "MTShaders.hpp"
#include "streamutils.hpp"
#include "MetalVideoRenderer.hpp"
#include "Settings.hpp"
#include "UpscalingSupport.hpp"
#include <array>

#import <CoreVideo/CoreVideo.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <TargetConditionals.h>
#if defined(SUPPORT_UPSCALING)
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
    id<MTLTexture> upscalingInputTexture = nil;
    id<MTLTexture> upscalingOutputTexture = nil;
    id<MTLTexture> upscalingMotionTexture = nil;
    id<MTLTexture> upscalingDepthTexture = nil;
#if !TARGET_OS_VISION
    id<MTLFXTemporalScaler> temporalScaler = nil;
#endif
    bool metalFxSupported = false;
    bool loggedMetalFxUnsupported = false;
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
#define m_UpscalingInputTexture m_State->upscalingInputTexture
#define m_UpscalingOutputTexture m_State->upscalingOutputTexture
#define m_UpscalingMotionTexture m_State->upscalingMotionTexture
#define m_UpscalingDepthTexture m_State->upscalingDepthTexture
#if !TARGET_OS_VISION
#define m_TemporalScaler m_State->temporalScaler
#endif
#define m_MetalFxSupported m_State->metalFxSupported
#define m_LoggedMetalFxUnsupported m_State->loggedMetalFxUnsupported
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
        if (!m_VideoPipelineState) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to create overlay pipeline state");
            return false;
        }

#if defined(SUPPORT_UPSCALING)
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

#if TARGET_OS_VISION
    return false;
#else
#if TARGET_OS_OSX
    if (@available(macOS 13.0, *)) {
#else
    if (@available(iOS 16.0, tvOS 16.0, visionOS 1.0, *)) {
#endif
        return [MTLFXTemporalScalerDescriptor supportsDevice:device];
    }

    return false;
#endif
}

bool MetalVideoRenderer::shouldUseUpscaling() const {
    return Settings::instance().upscaling() && m_MetalFxSupported &&
           m_LastFrameWidth > 0 && m_LastFrameHeight > 0 &&
           m_LastVideoRegionWidth > 0 && m_LastVideoRegionHeight > 0 &&
           m_LastVideoRegionWidth >= m_LastFrameWidth &&
           m_LastVideoRegionHeight >= m_LastFrameHeight &&
           (m_LastVideoRegionWidth > m_LastFrameWidth ||
            m_LastVideoRegionHeight > m_LastFrameHeight);
}

void MetalVideoRenderer::releaseUpscalingResources() {
#if !TARGET_OS_VISION
    releaseObjCReference(m_TemporalScaler);
#endif
    releaseObjCReference(m_UpscalingInputTexture);
    releaseObjCReference(m_UpscalingOutputTexture);
    releaseObjCReference(m_UpscalingMotionTexture);
    releaseObjCReference(m_UpscalingDepthTexture);
    m_UpscalingInputWidth = 0;
    m_UpscalingInputHeight = 0;
    m_UpscalingOutputWidth = 0;
    m_UpscalingOutputHeight = 0;
    m_UpscalingPixelFormat = MTLPixelFormatInvalid;
}

bool MetalVideoRenderer::ensureUpscalingResources(AVFrame* frame) {
    if (!shouldUseUpscaling() || frame == nullptr || m_MetalLayer.device == nil) {
        return false;
    }

    const int inputWidth = frame->width;
    const int inputHeight = frame->height;
    const int outputWidth = m_LastVideoRegionWidth;
    const int outputHeight = m_LastVideoRegionHeight;
    const MTLPixelFormat pixelFormat = m_MetalLayer.pixelFormat;

    if (
#if !TARGET_OS_VISION
        m_TemporalScaler &&
#endif
        m_UpscalingInputTexture && m_UpscalingOutputTexture &&
        m_UpscalingMotionTexture && m_UpscalingDepthTexture &&
        m_UpscalingInputWidth == inputWidth &&
        m_UpscalingInputHeight == inputHeight &&
        m_UpscalingOutputWidth == outputWidth &&
        m_UpscalingOutputHeight == outputHeight &&
        m_UpscalingPixelFormat == pixelFormat) {
        return true;
    }

    releaseUpscalingResources();

#if TARGET_OS_VISION
    return false;
#else
#if TARGET_OS_OSX
    if (@available(macOS 13.0, *)) {
#else
    if (@available(iOS 16.0, tvOS 16.0, visionOS 1.0, *)) {
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

    MTLTextureDescriptor* outputDesc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pixelFormat
                                                           width:outputWidth
                                                          height:outputHeight
                                                       mipmapped:NO];
    outputDesc.storageMode = MTLStorageModePrivate;
    outputDesc.usage = MTLTextureUsageShaderRead |
                       [m_TemporalScaler outputTextureUsage];

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

    m_UpscalingInputTexture = [m_MetalLayer.device newTextureWithDescriptor:inputDesc];
    m_UpscalingOutputTexture = [m_MetalLayer.device newTextureWithDescriptor:outputDesc];
    m_UpscalingMotionTexture = [m_MetalLayer.device newTextureWithDescriptor:motionDesc];
    m_UpscalingDepthTexture = [m_MetalLayer.device newTextureWithDescriptor:depthDesc];

    if (!m_UpscalingInputTexture || !m_UpscalingOutputTexture ||
        !m_UpscalingMotionTexture || !m_UpscalingDepthTexture) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Failed to allocate MetalFX upscaling textures");
        releaseUpscalingResources();
        return false;
    }

    m_UpscalingInputWidth = inputWidth;
    m_UpscalingInputHeight = inputHeight;
    m_UpscalingOutputWidth = outputWidth;
    m_UpscalingOutputHeight = outputHeight;
    m_UpscalingPixelFormat = pixelFormat;
    m_LoggedMetalFxUnsupported = false;

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Using MetalFX temporal scaler: %s %dx%d -> %dx%d format %s colorUsage=0x%lx depthUsage=0x%lx motionUsage=0x%lx outputUsage=0x%lx",
                NSStringFromClass([m_TemporalScaler class]).UTF8String,
                inputWidth, inputHeight, outputWidth, outputHeight,
                metalPixelFormatName(pixelFormat),
                static_cast<unsigned long>([m_TemporalScaler colorTextureUsage]),
                static_cast<unsigned long>([m_TemporalScaler depthTextureUsage]),
                static_cast<unsigned long>([m_TemporalScaler motionTextureUsage]),
                static_cast<unsigned long>([m_TemporalScaler outputTextureUsage]));
    return true;
#endif
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
        if (SDL_CondWaitTimeout(m_PresentationCond, m_PresentationMutex, 100) == SDL_MUTEX_TIMEDOUT) {
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
    if (!m_video_render_stats.rendered_frames) {
        m_video_render_stats.measurement_start_timestamp = before_render;
    }

    auto commandBuffer = [m_CommandQueue commandBuffer];
    [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer>) {
        // Free textures after completion of rendering per CVMetalTextureCache requirements
        for (size_t i = 0; i < planes; i++) {
            CFRelease(cvMetalTextures[i]);
        }
    }];

#if defined(SUPPORT_UPSCALING) && !TARGET_OS_VISION
    const bool useUpscaling = ensureUpscalingResources(frame);
    const uint64_t postProcessStart = LiGetMillis();

    if (useUpscaling) {
        auto sourcePassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
        MTLRenderPassColorAttachmentDescriptor* sourceAttachment =
            [sourcePassDescriptor.colorAttachments objectAtIndexedSubscript:0];
        if (sourceAttachment == nil) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to acquire MetalFX source attachment descriptor");
            return;
        }
        [sourceAttachment setTexture:m_UpscalingInputTexture];
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

        auto motionPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
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

        auto depthPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
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

        auto presentPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
        MTLRenderPassColorAttachmentDescriptor* presentAttachment =
            [presentPassDescriptor.colorAttachments objectAtIndexedSubscript:0];
        if (presentAttachment == nil) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to acquire MetalFX present attachment descriptor");
            return;
        }
        [presentAttachment setTexture:drawableTexture];
        [presentAttachment setLoadAction:MTLLoadActionClear];
        [presentAttachment setClearColor:MTLClearColorMake(0.0, 0.0, 0.0, 0.0)];
        [presentAttachment setStoreAction:MTLStoreActionStore];

        auto presentEncoder =
            [commandBuffer renderCommandEncoderWithDescriptor:presentPassDescriptor];
        [presentEncoder setRenderPipelineState:m_OverlayPipelineState];
        [presentEncoder setFragmentTexture:m_UpscalingOutputTexture atIndex:0];
        [presentEncoder setVertexBuffer:m_VideoVertexBuffer offset:0 atIndex:0];
        [presentEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
                            vertexStart:0
                            vertexCount:4];
        [presentEncoder endEncoding];

        m_video_render_stats.total_upscaling_time +=
            LiGetMillis() - postProcessStart;
        m_video_render_stats.upscaled_frames++;
        m_video_render_stats.total_post_process_time +=
            LiGetMillis() - postProcessStart;
        m_video_render_stats.post_processed_frames++;
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
    [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer>) {
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

    m_video_render_stats.total_render_time += LiGetMillis() - before_render;
    m_video_render_stats.rendered_frames++;
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
    if (!m_MetalFxSupported) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "MetalFX temporal upscaling is unavailable on this OS or device");
    }
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
                     "Failed to create MetalFX source vertex buffer");
        return false;
    }
#endif

    initialized = true;
    return true;
}}

VideoRenderStats* MetalVideoRenderer::video_render_stats() {
    if (!m_video_render_stats.rendered_frames) {
        m_video_render_stats.rendered_fps = 0.0f;
        m_video_render_stats.rendering_time = 0.0f;
        m_video_render_stats.post_processing_time = 0.0f;
        m_video_render_stats.upscaling_time = 0.0f;
        return (VideoRenderStats*)&m_video_render_stats;
    }

    m_video_render_stats.rendered_fps = (float)m_video_render_stats.rendered_frames /
            ((float) (LiGetMillis() - m_video_render_stats.measurement_start_timestamp) / 1000);

    m_video_render_stats.rendering_time = (float)m_video_render_stats.total_render_time /
            (float) m_video_render_stats.rendered_frames;

    m_video_render_stats.post_processing_time =
        m_video_render_stats.post_processed_frames
            ? (float)m_video_render_stats.total_post_process_time /
                  (float)m_video_render_stats.post_processed_frames
            : 0.0f;

    m_video_render_stats.upscaling_time =
        m_video_render_stats.upscaled_frames
            ? (float)m_video_render_stats.total_upscaling_time /
                  (float)m_video_render_stats.upscaled_frames
            : 0.0f;

    return (VideoRenderStats*)&m_video_render_stats;
}

#endif // USE_METAL_RENDERER
