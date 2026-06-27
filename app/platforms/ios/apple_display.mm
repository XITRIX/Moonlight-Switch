#import <UIKit/UIKit.h>

static UIWindow* AppleSelectWindow(NSArray<UIWindow*>* windows) {
    UIWindow* fallbackWindow = nil;

    for (UIWindow* window in windows) {
        if (window.hidden) {
            continue;
        }

        if (window.isKeyWindow) {
            return window;
        }

        if (fallbackWindow == nil) {
            fallbackWindow = window;
        }
    }

    if (fallbackWindow != nil) {
        return fallbackWindow;
    }

    return windows.firstObject;
}

static UIWindow* AppleGetActiveWindow() {
    UIApplication* application = [UIApplication sharedApplication];

    if (@available(iOS 13.0, tvOS 13.0, visionOS 1.0, *)) {
        UIWindow* inactiveWindow = nil;
        UIWindow* fallbackWindow = nil;

        for (UIScene* scene in application.connectedScenes) {
            if (![scene isKindOfClass:[UIWindowScene class]]) {
                continue;
            }

            UIWindowScene* windowScene = (UIWindowScene*)scene;
            UIWindow* window = AppleSelectWindow(windowScene.windows);
            if (window == nil) {
                continue;
            }

            if (windowScene.activationState == UISceneActivationStateForegroundActive) {
                return window;
            }

            if (windowScene.activationState == UISceneActivationStateForegroundInactive && inactiveWindow == nil) {
                inactiveWindow = window;
            } else if (fallbackWindow == nil) {
                fallbackWindow = window;
            }
        }

        if (inactiveWindow != nil) {
            return inactiveWindow;
        }

        if (fallbackWindow != nil) {
            return fallbackWindow;
        }
    }

#if !defined(PLATFORM_VISIONOS)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    UIWindow* keyWindow = application.keyWindow;
#pragma clang diagnostic pop
    if (keyWindow != nil) {
        return keyWindow;
    }

    id<UIApplicationDelegate> delegate = application.delegate;
    if ([delegate respondsToSelector:@selector(window)]) {
        return delegate.window;
    }
#endif

    return nil;
}

#ifdef PLATFORM_TVOS

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
    UIWindow* window = AppleGetActiveWindow();
    if (window == nil) {
        return;
    }

    AVDisplayManager* displayManager = [window avDisplayManager];
    if (displayManager == nil) {
        return;
    }

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

void getWindowSize(int* w, int* h) {
    UIWindow* window = AppleGetActiveWindow();
    CGSize logicalSize = CGSizeZero;
    CGFloat scale = 0.0;

    if (window != nil) {
        logicalSize = window.bounds.size;
        scale = window.traitCollection.displayScale;
#if !defined(PLATFORM_VISIONOS)
        if (scale <= 0.0) {
            scale = window.screen.scale;
        }
#endif
    }

#if !defined(PLATFORM_VISIONOS)
    if (logicalSize.width <= 0.0 || logicalSize.height <= 0.0 || scale <= 0.0) {
        UIScreen* screen = [UIScreen mainScreen];
        logicalSize = screen.bounds.size;
        scale = screen.scale;
    }
#endif

    if (scale <= 0.0) {
        scale = 1.0;
    }

    if (w != nullptr) {
        *w = (int)(logicalSize.width * scale + 0.5);
    }
    if (h != nullptr) {
        *h = (int)(logicalSize.height * scale + 0.5);
    }
}
