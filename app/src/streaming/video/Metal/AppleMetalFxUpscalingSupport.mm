#if defined(USE_METAL_RENDERER) && defined(PLATFORM_APPLE) && defined(SUPPORT_UPSCALING)

#include "UpscalingSupport.hpp"

#import <Metal/Metal.h>
#import <MetalFX/MetalFX.h>
#import <TargetConditionals.h>

bool isVideoUpscalingSupported() {
    static bool didCheck = false;
    static bool supported = false;

    if (didCheck) {
        return supported;
    }

    didCheck = true;

#if TARGET_OS_VISION
    supported = false;
#else
#if TARGET_OS_OSX
    if (@available(macOS 13.0, *)) {
#else
    if (@available(iOS 16.0, tvOS 16.0, visionOS 1.0, *)) {
#endif
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        supported = device != nil &&
                    [MTLFXTemporalScalerDescriptor supportsDevice:device];
    }
#endif

    return supported;
}

#endif
