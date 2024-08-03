//
//  streaming_view.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 27.05.2021.
//

#include "streaming_view.hpp"
#include "InputManager.hpp"
#include "click_gesture_recognizer.hpp"
#include "helper.hpp"
#include "ingame_overlay_view.hpp"
#include "streaming_input_overlay.hpp"
#include "two_finger_scroll_recognizer.hpp"
#include <Limelight.h>
#include <chrono>
#include <nanovg.h>

using namespace brls;

StreamingView::StreamingView(const Host& host, const AppInfo& app) : host(host), app(app) {
    Application::getPlatform()->disableScreenDimming(true);

    setFocusable(true);
    setHideHighlight(true);
    loader = new LoadingOverlay(this);

    keyboardHolder = new Box(Axis::COLUMN);
    keyboardHolder->detach();
    keyboardHolder->setJustifyContent(JustifyContent::FLEX_END);
    keyboardHolder->setAlignItems(AlignItems::STRETCH);
    addView(keyboardHolder);

    session = new MoonlightSession(host.address, app.app_id);

    ASYNC_RETAIN
    GameStreamClient::instance().connect(
        host.address, [ASYNC_TOKEN](GSResult<SERVER_DATA> result) {
            ASYNC_RELEASE
            if (!result.isSuccess()) {
                showError(result.error(), [this]() { terminate(false); });
                return;
            }

            ASYNC_RETAIN
            session->start([ASYNC_TOKEN](GSResult<bool> result) {
                ASYNC_RELEASE

                loader->setHidden(true);
                if (!result.isSuccess()) {
                    showError(result.error(), [this]() { terminate(false); });
                }
            });
        });

    MoonlightInputManager::instance().reloadButtonMappingLayout();

    static bool lMouseKeyGate = false;
    static bool lMouseKeyUsed = false;
    addGestureRecognizer(
        new FingersGestureRecognizer(3, [this] { addKeyboard(); }));

    addGestureRecognizer(
        new ClickGestureRecognizer(1, [](TapGestureStatus status) {
            if (Settings::instance().touchscreen_mouse_mode()) return;

            if (status.state == brls::GestureState::END) {
                Logger::debug("Left mouse click");
                MoonlightInputManager::instance().leftMouseClick();
                lMouseKeyGate = true;
                delay(200, [] { lMouseKeyGate = false; });
            }
        }));

    addGestureRecognizer(
        new ClickGestureRecognizer(2, [](TapGestureStatus status) {
            if (Settings::instance().touchscreen_mouse_mode()) return;

            if (status.state == brls::GestureState::END) {
                Logger::debug("Right mouse click");
                MoonlightInputManager::instance().rightMouseClick();
            }
        }));

    addGestureRecognizer(new PanGestureRecognizer(
        [this](PanGestureStatus status, Sound* sound) {
            static bool overlayTriggered = false;

            // Close keyboard by swiping outside of it
            if (status.state == brls::GestureState::START) {
                removeKeyboard();
                overlayTriggered = false;
            }

            // Open overlay by swipe from left screen corner
            bool hasControllers = Application::getPlatform()->getInputManager()->getControllersConnectedCount() > 0;
            if (!hasControllers && !overlayTriggered && status.state == brls::GestureState::STAY && status.startPosition.x < 100 && status.position.x > 200) {
                overlayTriggered = true;
                auto overlay = new IngameOverlay(this);
                Application::pushActivity(new Activity(overlay));
            }

            if (Settings::instance().touchscreen_mouse_mode()) return;

            if (status.state == brls::GestureState::UNSURE && lMouseKeyGate) {
                lMouseKeyGate = false;
                lMouseKeyUsed = true;
            } else if (status.state == brls::GestureState::START) {
                if (lMouseKeyUsed) {
//                    Logger::debug("Pressed key at {}", status.state);
                    LiSendMouseButtonEvent(BUTTON_ACTION_PRESS,
                                           BUTTON_MOUSE_LEFT);
                }
            } else if (status.state == brls::GestureState::STAY) {
                brls::RawMouseState mouseState;
                Application::getPlatform()->getInputManager()->updateMouseStates(&mouseState);
                // Dirty hack to not update pan if mouse left button is pressed, because pan gesture recognizer will append its speed with raw mouse value
                // Need to improve gesture recognizers to determine the input source and ignore it for mouse
                if (!mouseState.leftButton) {
                    MoonlightInputManager::instance().updateTouchScreenPanDelta(
                            status);
                }
            } else if (lMouseKeyUsed) {
//                Logger::debug("Release key at {}", status.state);
                LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE,
                                       BUTTON_MOUSE_LEFT);
                lMouseKeyUsed = false;
            }
        },
        PanAxis::ANY));

    addGestureRecognizer(new TwoFingerScrollGestureRecognizer(
        [this](TwoFingerScrollState state) {
            if (Settings::instance().touchscreen_mouse_mode()) return;

            if (state.state == brls::GestureState::START)
                this->touchScrollCounter = 0;

            int threashhold = state.delta.y / 25;
            if (threashhold != this->touchScrollCounter) {
                Logger::debug("Scroll on: {}",
                              threashhold - this->touchScrollCounter);
                int invert = Settings::instance().swap_mouse_scroll() ? -1 : 1;
                char scrollCount = threashhold - this->touchScrollCounter;
                LiSendScrollEvent(scrollCount * invert);
                this->touchScrollCounter = threashhold;
            }
        }));

    keysSubscription =
        Application::getPlatform()
            ->getInputManager()
            ->getKeyboardKeyStateChanged()
            ->subscribe([this, host, app](brls::KeyState state) {
                if (state.key == BRLS_KBD_KEY_ESCAPE) {
                    static std::chrono::high_resolution_clock::time_point
                        clock_counter;
                    static bool buttonState = false;
                    static bool used = false;

                    auto duration =
                        std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::high_resolution_clock::now() -
                            clock_counter);

                    if (!buttonState && state.pressed) {
                        buttonState = true;
                        clock_counter =
                            std::chrono::high_resolution_clock::now();
                    } else if (buttonState && !state.pressed) {
                        buttonState = false;
                        used = false;
                    } else if (buttonState && duration.count() >= 2 && !used) {
                        used = true;

                        auto overlay = new IngameOverlay(this);
                        Application::pushActivity(new Activity(overlay));
                    }
                }
            });
}

void StreamingView::onFocusGained() {
    View::onFocusGained();
    if (!blocked) {
        blocked = true;
        Application::blockInputs(true);
    }

    tempInputLock = true;
    ASYNC_RETAIN
    delay(300, [ASYNC_TOKEN]() {
        ASYNC_RELEASE
        this->tempInputLock = false;
    });

    Application::getPlatform()->getInputManager()->setPointerLock(true);
}

void StreamingView::onFocusLost() {
    View::onFocusLost();
    if (blocked) {
        blocked = false;
        Application::unblockInputs();
    }

    removeKeyboard();
    Application::getPlatform()->getInputManager()->setPointerLock(false);
}

void StreamingView::draw(NVGcontext* vg, float x, float y, float width,
                         float height, Style style, FrameContext* ctx) {
    if (session->is_terminated()) {
        terminate(false);
        return;
    }

    session->draw(vg, width, height);

    if (!tempInputLock && session->is_active())
        handleInput();
    handleOverlayCombo();
    handleMouseInputCombo();

    if (session->connection_status_is_poor()) {
        nvgFontSize(vg, 20);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

        nvgFontBlur(vg, 3);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
        nvgFontFaceId(vg, Application::getFont(FONT_REGULAR));
        nvgText(vg, 50, height - 28, "\uE140 Bad connection...", nullptr);

        nvgFontBlur(vg, 0);
        nvgFillColor(vg, nvgRGBA(255, 255, 255, 255));
        nvgFontFaceId(vg, Application::getFont(FONT_REGULAR));
        nvgText(vg, 50, height - 28, "\uE140 Bad connection...", nullptr);
    }

    if (draw_stats) {
        static char output[1024];
        int offset = 0;
        auto stats = session->session_stats();

        offset += sprintf(&output[offset],
                          "Estimated host PC frame rate: %.2f FPS\n"
                          "Incoming frame rate from network: %.2f FPS\n"
                          "Decoding frame rate: %.2f FPS\n"
                          "Rendering frame rate: %.2f FPS\n",
                          stats->video_decode_stats.total_fps,
                          stats->video_decode_stats.received_fps,
                          stats->video_decode_stats.decoded_fps,
                          stats->video_render_stats.rendered_fps);

        offset += sprintf(
            &output[offset],
            "Frames dropped by your network connection: %.2f%% (Total: %u)\n"
            "Average receive time: %.2f ms\n"
            "Average decoding time: %.2f ms\n"
            "Average rendering time: %.2f ms\n",
            (float)stats->video_decode_stats.network_dropped_frames /
                stats->video_decode_stats.total_frames * 100,
            stats->video_decode_stats.network_dropped_frames,
            (float)stats->video_decode_stats.total_reassembly_time /
                stats->video_decode_stats.received_frames,
            (float)stats->video_decode_stats.total_decode_time /
                stats->video_decode_stats.decoded_frames,
            (float)stats->video_render_stats.total_render_time /
                stats->video_render_stats.rendered_frames);

        nvgFontFaceId(vg, Application::getFont(FONT_REGULAR));
        nvgFontSize(vg, 20);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);

        nvgFontBlur(vg, 1);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
        nvgTextBox(vg, 20, 30, width, output, NULL);

        nvgFontBlur(vg, 0);
        nvgFillColor(vg, nvgRGBA(0, 255, 0, 255));
        nvgTextBox(vg, 20, 30, width, output, NULL);
    }

    Box::draw(vg, x, y, width, height, style, ctx);
}

void StreamingView::addKeyboard() {
    if (keyboard)
        return;

    keyboard = new KeyboardView(false);
    keyboardHolder->addView(keyboard);
}

void StreamingView::removeKeyboard() {
    if (!keyboard)
        return;

    keyboard->removeFromSuperView();
    keyboard = nullptr;
    Application::giveFocus(this);
}

void StreamingView::terminate(bool terminateApp) {
    if (terminated)
        return;
    terminated = true;

    session->stop(terminateApp);

    bool hasOverlays =
        Application::getActivitiesStack().back() != this->getParentActivity();
    this->dismiss([this, hasOverlays] {
        if (hasOverlays)
            this->dismiss();
    });
}

void StreamingView::handleInput() {
    if (!this->focused) {
        MoonlightInputManager::instance().dropInput();
        return;
    }

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

        // Drop input if keyboard overlay presented
        MoonlightInputManager::instance().dropInput();
    } else {
        MoonlightInputManager::instance().handleInput();
    }
}

void StreamingView::handleOverlayCombo() {
    if (!this->focused)
        return;

    KeyComboOptions options = Settings::instance().overlay_options();

    static ControllerState controller;
    Application::getPlatform()->getInputManager()->updateUnifiedControllerState(
        &controller);

    static std::chrono::high_resolution_clock::time_point clock_counter;
    static bool buttonState = false;
    static bool used = false;

    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::high_resolution_clock::now() - clock_counter);

    bool buttonsPressed = true;
    for (auto button : options.buttons) {
        buttonsPressed &= controller.buttons[button];
    }

    if (!buttonState && buttonsPressed) {
        buttonState = true;
        clock_counter = std::chrono::high_resolution_clock::now();
    } else if (buttonState && !buttonsPressed) {
        buttonState = false;
        used = false;
    } else if (buttonState && duration.count() >= options.holdTime && !used) {
        used = true;

        auto overlay = new IngameOverlay(this);
        Application::pushActivity(new Activity(overlay));
    }
}

void StreamingView::handleMouseInputCombo() {
    if (!this->focused)
        return;

    KeyComboOptions options = Settings::instance().mouse_input_options();
    if (options.buttons.empty())
        return;

    static ControllerState controller;
    Application::getPlatform()->getInputManager()->updateUnifiedControllerState(
        &controller);

    static std::chrono::high_resolution_clock::time_point clock_counter;
    static bool buttonState = false;
    static bool used = false;

    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::high_resolution_clock::now() - clock_counter);

    bool buttonsPressed = true;
    for (auto button : options.buttons) {
        buttonsPressed &= controller.buttons[button];
    }

    if (!buttonState && buttonsPressed) {
        buttonState = true;
        clock_counter = std::chrono::high_resolution_clock::now();
    } else if (buttonState && !buttonsPressed) {
        buttonState = false;
        used = false;
    } else if (buttonState && duration.count() >= options.holdTime && !used) {
        used = true;

        auto overlay = new StreamingInputOverlay(this);
        Application::pushActivity(new Activity(overlay));
    }
}

void StreamingView::onLayout() {
    Box::onLayout();
    if (loader)
        loader->layout();

    if (keyboardHolder) {
        keyboardHolder->setWidth(getWidth());
        keyboardHolder->setHeight(getHeight());
    }
}

StreamingView::~StreamingView() {
    Application::getPlatform()->disableScreenDimming(false);
    Application::getPlatform()
        ->getInputManager()
        ->getKeyboardKeyStateChanged()
        ->unsubscribe(keysSubscription);
    session->stop(false);
    delete session;
}
