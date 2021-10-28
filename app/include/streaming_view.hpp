//
//  streaming_view.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 27.05.2021.
//

#pragma once

#include <borealis.hpp>
#include <Settings.hpp>
#include "GameStreamClient.hpp"
#include "MoonlightSession.hpp"
#include "loading_overlay.hpp"
#include "keyboard_view.hpp"
#include "gestures/fingers_gesture_recognizer.hpp"
#include <optional>

class StreamingView : public brls::Box
{
public:
    StreamingView(Host host, AppInfo app);
    ~StreamingView();
    
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override;
    void onFocusGained() override;
    void onFocusLost() override;
    void onLayout() override;
    
    void terminate(bool terminateApp);
    
    bool draw_stats = false;

    Host getHost()
    {
        return host;
    }

    AppInfo getApp()
    {
        return app;
    }
private:
    Host host;
    AppInfo app;
    MoonlightSession* session;
    LoadingOverlay* loader = nullptr;
    Box* keyboardHolder = nullptr;
    KeyboardView* keyboard = nullptr;
    bool blocked = false;
    bool terminated = false;
    bool tempInputLock = false;
    brls::Event<brls::KeyState>::Subscription keysSubscription;
    
    void handleInput();
    void handleOverlayCombo();
    void handleMouseInputCombo();
    void addKeyboard();
    void removeKeyboard();
};
