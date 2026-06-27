#if defined(USE_METAL_RENDERER) && defined(PLATFORM_APPLE) && defined(SUPPORT_UPSCALING)

#include "UpscalingSupport.hpp"

#import <Metal/Metal.h>
#import <TargetConditionals.h>

bool isVideoUpscalingSupported() {
    static bool didCheck = false;
    static bool supported = false;

    if (didCheck) {
        return supported;
    }

    didCheck = true;

    supported = MTLCreateSystemDefaultDevice() != nil;

    return supported;
}

#endif
