//
//  streaming_view.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 27.05.2021.
//

#pragma once

#include "gestures/fingers_gesture_recognizer.hpp"
#include "keyboard_view.hpp"
#include "loading_overlay.hpp"
#include <Settings.hpp>
#include <borealis.hpp>
#include <chrono>
#include <optional>
#include "GameStreamClient.hpp"
#include "MoonlightSession.hpp"
#include "two_finger_scroll_recognizer.hpp"
#ifdef __SWITCH2__
#include <memory>

class Switch2Clocks;
#endif

class StreamingView : public brls::Box {
  public:
    StreamingView(const Host& host, const AppInfo& app);
    ~StreamingView();

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;
    void onFocusGained() override;
    void onFocusLost() override;
    void onLayout() override;

    void terminate(bool terminateApp);

    Host getHost() { return host; }

    AppInfo getApp() { return app; }

  private:
    Host host;
    AppInfo app;
    MoonlightSession* session = nullptr;
    LoadingOverlay* loader = nullptr;
    Box* keyboardHolder = nullptr;
    KeyboardView* keyboard = nullptr;
    bool blocked = false;
    bool terminated = false;
    bool tempInputLock = false;
    brls::Event<brls::KeyState>::Subscription keysSubscription;
    int touchScrollCounter = 0;
    size_t bottombarDelayTask = -1;
    bool m_use_hdr = false;
    TwoFingerScrollGestureRecognizer* scrollTouchRecognizer = nullptr;
    brls::Event<bool>::Subscription windowFocusSubscription;
    bool windowFocused = true;

    void onWindowFocusChanged(bool focused);

    std::string buildStatsText();
    std::string buildCompactStatsText();
    std::string buildFrameCounterText();
    uint64_t frameCounterSecond = 0;
    uint32_t frameCounterLostBase = 0;
    uint32_t frameCounterLost = 0;
#ifdef __SWITCH2__
    std::unique_ptr<Switch2Clocks> clocks;
    void updateClocks();
#endif
    std::string statsText;
    std::chrono::steady_clock::time_point statsTextTime{};
    float statsBounds[4] = {0, 0, 0, 0};

    void handleInput();
    void handleOverlayCombo();
    void handleMouseInputCombo();
    void addKeyboard();
    void removeKeyboard();
};
