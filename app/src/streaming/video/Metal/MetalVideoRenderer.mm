#if defined(PLATFORM_IOS)

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
#include <array>

#import <CoreVideo/CoreVideo.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#define MAX_VIDEO_PLANES 3

CAMetalLayer* m_MetalLayer;
CVMetalTextureCacheRef m_TextureCache;
id<MTLBuffer> m_CscParamsBuffer;
id<MTLBuffer> m_VideoVertexBuffer;
id<MTLRenderPipelineState> m_VideoPipelineState;
id<MTLRenderPipelineState> m_OverlayPipelineState;
id<MTLLibrary> m_ShaderLibrary;
id<MTLCommandQueue> m_CommandQueue;
id<CAMetalDrawable> m_NextDrawable;
SDL_mutex* m_PresentationMutex = SDL_CreateMutex();
SDL_cond* m_PresentationCond = SDL_CreateCond();
int m_PendingPresentationCount = 0;

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

int getFramePlaneCount(AVFrame* frame)
{
    return CVPixelBufferGetPlaneCount((CVPixelBufferRef)frame->data[3]);
}

void MetalVideoRenderer::discardNextDrawable()
{ @autoreleasepool {
    if (!m_NextDrawable) {
        return;
    }

//    [m_NextDrawable release];
    m_NextDrawable = nullptr;
}}

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

        // Free any unpresented drawable since we're changing pixel formats
        discardNextDrawable();

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

        m_LastColorSpace = colorspace;
        m_LastFullRange = fullRange;
    }

    return true;
}

bool MetalVideoRenderer::updateVideoRegionSizeForFrame(AVFrame* frame) {
    int drawableWidth, drawableHeight;
    SDL_Metal_GetDrawableSize(m_Window, &drawableWidth, &drawableHeight);

    // Check if anything has changed since the last vertex buffer upload
    if (m_VideoVertexBuffer &&
            frame->width == m_LastFrameWidth && frame->height == m_LastFrameHeight &&
            drawableWidth == m_LastDrawableWidth && drawableHeight == m_LastDrawableHeight) {
        // Nothing to do
        return true;
    }

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

    return true;
}

MetalVideoRenderer::MetalVideoRenderer() {
    initialize();
}

MetalVideoRenderer::~MetalVideoRenderer()
{@autoreleasepool {
    if (m_TextureCache != nullptr) {
        CFRelease(m_TextureCache);
    }

//    if (m_MetalView != nullptr) {
//        SDL_Metal_DestroyView(m_MetalView);
//    }
}}

void MetalVideoRenderer::waitToRender()
{ @autoreleasepool {
    if (!m_NextDrawable) {
        // Wait for the next available drawable before latching the frame to render
        m_NextDrawable = [m_MetalLayer nextDrawable];
        if (m_NextDrawable == nullptr) {
            return;
        }

//        if (m_MetalLayer.displaySyncEnabled) {
        // Pace ourselves by waiting if too many frames are pending presentation
        SDL_LockMutex(m_PresentationMutex);
        if (m_PendingPresentationCount > 2) {
            if (SDL_CondWaitTimeout(m_PresentationCond, m_PresentationMutex, 100) == SDL_MUTEX_TIMEDOUT) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Presentation wait timed out after 100 ms");
            }
        }
        SDL_UnlockMutex(m_PresentationMutex);
//        }
    }
}}

void MetalVideoRenderer::draw(NVGcontext* vg, int width, int height, AVFrame* frame) {
    waitToRender();

    if (frame->format != AV_PIX_FMT_VIDEOTOOLBOX) { return; }

    // Handle changes to the frame's colorspace from last time we rendered
    if (!updateColorSpaceForFrame(frame)) {
        // Trigger the main thread to recreate the decoder
        SDL_Event event;
        event.type = SDL_RENDER_DEVICE_RESET;
        SDL_PushEvent(&event);
        return;
    }

    // Handle changes to the video size or drawable size
    if (!updateVideoRegionSizeForFrame(frame)) {
        // Trigger the main thread to recreate the decoder
        SDL_Event event;
        event.type = SDL_RENDER_DEVICE_RESET;
        SDL_PushEvent(&event);
        return;
    }

    // Don't proceed with rendering if we don't have a drawable
    if (m_NextDrawable == nullptr) {
        return;
    }

    std::array<CVMetalTextureRef, MAX_VIDEO_PLANES> cvMetalTextures;
    size_t planes = getFramePlaneCount(frame);
//    SDL_assert(planes <= MAX_VIDEO_PLANES);

    CVPixelBufferRef pixBuf = reinterpret_cast<CVPixelBufferRef>(frame->data[3]);

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

    // Prepare a render pass to render into the next drawable
    auto renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
    renderPassDescriptor.colorAttachments[0].texture = m_NextDrawable.texture;
    renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);
    renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
    auto commandBuffer = [m_CommandQueue commandBuffer];
    auto renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];

    // Bind textures and buffers then draw the video region
    [renderEncoder setRenderPipelineState:m_VideoPipelineState];
    for (size_t i = 0; i < planes; i++) {
        [renderEncoder setFragmentTexture:CVMetalTextureGetTexture(cvMetalTextures[i]) atIndex:i];
    }
    [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer>) {
        // Free textures after completion of rendering per CVMetalTextureCache requirements
        for (size_t i = 0; i < planes; i++) {
            CFRelease(cvMetalTextures[i]);
        }
    }];

    [renderEncoder setFragmentBuffer:m_CscParamsBuffer offset:0 atIndex:0];
    [renderEncoder setVertexBuffer:m_VideoVertexBuffer offset:0 atIndex:0];
    [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];

    // Now draw any overlays that are enabled
    //

    [renderEncoder endEncoding];

//    if (m_MetalLayer.displaySyncEnabled) {
    // Queue a completion callback on the drawable to pace our rendering
    SDL_LockMutex(m_PresentationMutex);
    m_PendingPresentationCount++;
    SDL_UnlockMutex(m_PresentationMutex);
    [m_NextDrawable addPresentedHandler:^(id<MTLDrawable>) {
        SDL_LockMutex(m_PresentationMutex);
        m_PendingPresentationCount--;
        SDL_CondSignal(m_PresentationCond);
        SDL_UnlockMutex(m_PresentationMutex);
    }];
//    }

    // Flip to the newly rendered buffer
    [commandBuffer presentDrawable:m_NextDrawable];
    [commandBuffer commit];

    // Wait for the command buffer to complete and free our CVMetalTextureCache references
    [commandBuffer waitUntilCompleted];

//    [m_NextDrawable release];
    m_NextDrawable = nullptr;
}

id<MTLDevice> getMetalDevice() {
    if (@available(iOS 18.0, *)) {
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
}

bool MetalVideoRenderer::initialize()
{ @autoreleasepool {
    int err;

    auto videoContext = (brls::SDLVideoContext*) brls::Application::getPlatform()->getVideoContext();
    m_Window = videoContext->getSDLWindow();

    id<MTLDevice> device = getMetalDevice();
    if (!device) {
        return false;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Selected Metal device: %s",
                device.name.UTF8String);

//    if (!checkDecoderCapabilities(device, params)) {
//        return false;
//    }

//    err = av_hwdevice_ctx_create(&m_HwContext,
//                                 AV_HWDEVICE_TYPE_VIDEOTOOLBOX,
//                                 nullptr,
//                                 nullptr,
//                                 0);
    if (err < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "av_hwdevice_ctx_create() failed for VT decoder: %d",
                    err);
        return false;
    }
    SDL_SysWMinfo info;
    UIView *view = NULL;
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(m_Window, &info)) {
        if (info.subsystem == SDL_SYSWM_UIKIT) {
            view = info.info.uikit.window.rootViewController.view;
//            if (view.tag == SDL_METALVIEW_TAG) {
//                m_MetalView = view;
//            }
        }
    }

    if (!view) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_Metal_CreateView() failed: %s",
                     SDL_GetError());
        return false;
    }

    m_MetalLayer = (CAMetalLayer*) view.layer.sublayers[0];

    // Choose a device
    m_MetalLayer.device = device;

    // Allow EDR content if we're streaming in a 10-bit format
    m_MetalLayer.wantsExtendedDynamicRangeContent = true;// !!(params->videoFormat & VIDEO_FORMAT_MASK_10BIT);

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
    err = CVMetalTextureCacheCreate(kCFAllocatorDefault, cacheAttributes, m_MetalLayer.device, nullptr, &m_TextureCache);
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
    return true;
}}

VideoRenderStats* MetalVideoRenderer::video_render_stats() {
    m_video_render_stats.rendered_fps =
            (float)m_video_render_stats.rendered_frames /
            ((float)(LiGetMillis() -
                     m_video_render_stats.measurement_start_timestamp) /
             1000);

    return (VideoRenderStats*)&m_video_render_stats;
}

#endif
