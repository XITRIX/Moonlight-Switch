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
#include <optional>
#include "GameStreamClient.hpp"
#include "MoonlightSession.hpp"
#include "two_finger_scroll_recognizer.hpp"

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

  private:
    /**
     * Ends the stream when the console suspends, matching Moonlight on
     * Android.
     *
     * There, Game.onStop() calls stopConnection() and then finish(), so
     * putting the phone to sleep mid stream ends the session and returns you
     * to the host list. This does the same on losing applet focus, which on
     * Switch means the console sleeping or the HOME menu taking over.
     *
     * The alternative is keeping the session alive across the suspend, which
     * means preserving decoder and GPU state through a window where the OS
     * tears those services down underneath a process that keeps running. That
     * is where issue #306 lives. Ending the stream removes the state instead
     * of trying to repair it: there is nothing stale left to get wrong.
     *
     * The cost is one reconnect after waking, which is exactly the Android
     * behaviour.
     */
    void onWindowFocusChanged(bool focused);

    brls::Event<bool>::Subscription windowFocusSubscription;

    /**
     * Set by onWindowFocusChanged, acted on in draw().
     *
     * The focus callback runs inside Event<bool>::fire, which iterates its
     * callback list by value:
     *
     *     for (Callback cb : this->callbacks)
     *         cb(args...);
     *
     * terminate() calls dismiss(), which pops this view and runs its
     * destructor, and the destructor unsubscribes. Doing that from inside the
     * callback mutates the list fire() is walking, and the iteration then
     * runs off a destroyed object.
     *
     * So the callback only records the intent and draw() performs it, where
     * the view is known to be alive and nothing is iterating the event.
     */
    bool pendingSuspendTerminate = false;

    /**
     * How many focus subscriptions this class currently holds.
     *
     * One streaming view exists at a time, so this is 1 while streaming and 0
     * otherwise. Anything else is a leak. It is logged on both sides so the
     * value shows up in whatever is collecting logs, which is the only way a
     * bug of this shape announces itself: the previous version subscribed in
     * onFocusGained, gained a callback per focus cycle, and said nothing at
     * all until the leftovers fired on a destroyed object.
     */
    static inline int focusSubscriptionCount = 0;

  public:

    bool draw_stats = false;

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

    void handleInput();
    void handleOverlayCombo();
    void handleMouseInputCombo();
    void addKeyboard();
    void removeKeyboard();
};
