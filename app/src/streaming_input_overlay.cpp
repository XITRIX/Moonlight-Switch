//
//  streaming_input_overlay.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 25.08.2021.
//

#include "streaming_input_overlay.hpp"
#include <Limelight.h>

using namespace brls;

StreamingInputOverlay::StreamingInputOverlay(StreamingView* streamView)
    : streamView(streamView) {
    this->inflateFromXMLRes("xml/views/stream_input_overlay.xml");

    inner->setHideHighlightBackground(true);
    inner->setHideHighlightBorder(true);

    //    hintBar->setAlpha(0.3f);
    NVGcolor color = Application::getTheme()["brls/background"];
    color.a = 0.3f;
    hintBar->setBackgroundColor(color);

    addGestureRecognizer(
        new TapGestureRecognizer([this](TapGestureStatus status, Sound* sound) {
            if (status.state == GestureState::END)
                this->dismiss();
        }));

    applet->addGestureRecognizer(new TapGestureRecognizer(
        [this](TapGestureStatus status, Sound* sound) {}));

    actionsToFree.push_back(registerAction("mouse_input/mouse"_i18n,
                                           ControllerButton::BUTTON_LSB,
                                           [](View* view) { return true; }));
    actionsToFree.push_back(registerAction("mouse_input/scroll"_i18n,
                                           ControllerButton::BUTTON_RSB,
                                           [](View* view) { return true; }));
    actionsToFree.push_back(registerAction("mouse_input/keyboard"_i18n,
                                           ControllerButton::BUTTON_X,
                                           [this](View* view) {
                                               this->toggleKeyboard();
                                               return true;
                                           }));
    inner->registerAction("hints/back"_i18n, ControllerButton::BUTTON_B,
                          [this](View* view) {
                              if (this->isKeyboardOpen) {
                                  this->toggleKeyboard();
                                  return true;
                              }
                              return false;
                          });
}

void StreamingInputOverlay::onFocusGained() {
    View::onFocusGained();
    Application::giveFocus(this);
}

void StreamingInputOverlay::draw(NVGcontext* vg, float x, float y, float width,
                                 float height, Style style, FrameContext* ctx) {
    Box::draw(vg, x, y, width, height, style, ctx);

    // Keyboard
    if (keyboard) {
        static KeyboardState oldKeyboardState;
        KeyboardState keyboardState = keyboard->getKeyboardState();

        for (int i = 0; i < _VK_KEY_MAX; i++) {
            if (keyboardState.keys[i] != oldKeyboardState.keys[i]) {
                oldKeyboardState.keys[i] = keyboardState.keys[i];
                LiSendKeyboardEvent(
                    keyboard->getKeyCode((KeyboardKeys)i),
                    keyboardState.keys[i] ? KEY_ACTION_DOWN : KEY_ACTION_UP, 0);
            }
        }
    }

    if (!isKeyboardOpen) {

        ControllerState controller;
        Application::getPlatform()
            ->getInputManager()
            ->updateUnifiedControllerState(&controller);

        // Add setting with range 10 - 30
        float mouseSpeed = 15;

        if (controller.buttons[BUTTON_RB])
            mouseSpeed += 10;
        if (controller.buttons[BUTTON_LB])
            mouseSpeed -= 10;

        float x = controller.axes[LEFT_X] * mouseSpeed;
        float y = controller.axes[LEFT_Y] * mouseSpeed;

        // Dead zone prevents from mouse drifting
        if (x < 2 && x > -2)
            x = 0;
        if (y < 2 && y > -2)
            y = 0;

        if (x != 0 || y != 0) {
            float multiplier =
                Settings::instance().get_mouse_speed_multiplier() / 100.f *
                    1.5f +
                0.5f;
            LiSendMouseMoveEvent(x * multiplier, y * multiplier);
        }

        static bool old_l_pressed;
        static bool old_r_pressed;

        float scroll_y = controller.axes[brls::RIGHT_Y];
        bool l_pressed = controller.buttons[brls::BUTTON_RT];
        bool r_pressed = controller.buttons[brls::BUTTON_LT];

        // Dead zone prevents from mouse drifting
        if (scroll_y < 0.2f && scroll_y > -0.2f)
            scroll_y = 0;

        // Left mouse button
        if (l_pressed != old_l_pressed) {
            old_l_pressed = l_pressed;
            auto lb = Settings::instance().swap_mouse_keys()
                          ? BUTTON_MOUSE_RIGHT
                          : BUTTON_MOUSE_LEFT;
            LiSendMouseButtonEvent(
                l_pressed ? BUTTON_ACTION_PRESS : BUTTON_ACTION_RELEASE, lb);
        }

        // Right mouse button
        if (r_pressed != old_r_pressed) {
            old_r_pressed = r_pressed;
            auto rb = Settings::instance().swap_mouse_keys()
                          ? BUTTON_MOUSE_LEFT
                          : BUTTON_MOUSE_RIGHT;
            LiSendMouseButtonEvent(
                r_pressed ? BUTTON_ACTION_PRESS : BUTTON_ACTION_RELEASE, rb);
        }

        // Scroll
        if (Settings::instance().swap_mouse_scroll())
            scroll_y *= -1;

        std::chrono::high_resolution_clock::time_point timeNow =
            std::chrono::high_resolution_clock::now();
        static std::chrono::high_resolution_clock::time_point timeStamp =
            timeNow;

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                            timeNow - timeStamp)
                            .count();
        if (scroll_y != 0 && duration > 550 - abs(scroll_y) * 500) {
            timeStamp = timeNow;
            brls::Logger::info("Scroll sended: {}", scroll_y);
            LiSendScrollEvent(scroll_y > 0 ? 1 : -1);
        }
    }
}

void StreamingInputOverlay::toggleKeyboard() {
    isKeyboardOpen = !isKeyboardOpen;
    if (!isKeyboardOpen) {
        inner->removeView(keyboard);
        keyboard = nullptr;
        Application::giveFocus(this);
    } else {
        keyboard = new KeyboardView(true);
        inner->addView(keyboard);
    }

    if (!isKeyboardOpen) {
        Application::giveFocus(this);
        actionsToFree.push_back(registerAction(
            "mouse_input/mouse"_i18n, ControllerButton::BUTTON_LSB,
            [](View* view) { return true; }));
        actionsToFree.push_back(registerAction(
            "mouse_input/scroll"_i18n, ControllerButton::BUTTON_RSB,
            [](View* view) { return true; }));
        actionsToFree.push_back(registerAction("mouse_input/keyboard"_i18n,
                                               ControllerButton::BUTTON_X,
                                               [this](View* view) {
                                                   this->toggleKeyboard();
                                                   return true;
                                               }));
    } else {
        Application::giveFocus(keyboard);
        for (auto action : actionsToFree) {
            unregisterAction(action);
        }
        actionsToFree.clear();
    }
    Application::getGlobalHintsUpdateEvent()->fire();
}

brls::AppletFrame* StreamingInputOverlay::getAppletFrame() { return applet; }
