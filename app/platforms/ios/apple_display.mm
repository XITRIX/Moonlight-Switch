#ifdef PLATFORM_TVOS

#import <UIKit/UIKit.h>
#import <AVFoundation/AVDisplayCriteria.h>
#import <AVKit/AVDisplayManager.h>
#import <AVKit/UIWindow.h>
#include "Limelight.h"
#include "Settings.hpp"

@interface AVDisplayCriteria()
@property(readonly) int videoDynamicRange;
@property(readonly, nonatomic) float refreshRate;
- (id)initWithRefreshRate:(float)arg1 videoDynamicRange:(int)arg2;
@end

void updatePreferredDisplayMode(bool streamActive) {
    UIWindow* window = [[[UIApplication sharedApplication] delegate] window];
    AVDisplayManager* displayManager = [window avDisplayManager];

    // This logic comes from Kodi and MrMC
    if (streamActive) {
        int dynamicRange;

        if (LiGetCurrentHostDisplayHdrMode()) {
            dynamicRange = 2; // HDR10
        }
        else {
            dynamicRange = 0; // SDR
        }

        AVDisplayCriteria* displayCriteria = [[AVDisplayCriteria alloc] initWithRefreshRate:Settings::instance().fps()
                                                                          videoDynamicRange:dynamicRange];
        displayManager.preferredDisplayCriteria = displayCriteria;
    }
    else {
        // Switch back to the default display mode
        displayManager.preferredDisplayCriteria = nil;
    }
}

#endif
