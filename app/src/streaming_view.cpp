//
//  streaming_view.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 27.05.2021.
//

#include "streaming_view.hpp"
#include "ingame_overlay_view.hpp"
#include <nanovg.h>
#include <Limelight.h>
#include <chrono>
#include "helper.hpp"

// Moonlight ready gamepad
struct GamepadState {
    short buttonFlags;
    unsigned char leftTrigger;
    unsigned char rightTrigger;
    short leftStickX;
    short leftStickY;
    short rightStickX;
    short rightStickY;
    
    bool is_equal(GamepadState other) {
        return buttonFlags == other.buttonFlags &&
        leftTrigger == other.leftTrigger &&
        rightTrigger == other.rightTrigger &&
        leftStickX == other.leftStickX &&
        leftStickY == other.leftStickY &&
        rightStickX == other.rightStickX &&
        rightStickY == other.rightStickY;
    }
};

struct MouseStateS {
    Point position;
    float scroll_y;
    bool l_pressed;
    bool m_pressed;
    bool r_pressed;
};

FingersGestureRecognizer::FingersGestureRecognizer(int fingers, FingersGestureEvent::Callback respond)
    : fingers(fingers)
{
    event.subscribe(respond);
}

GestureState FingersGestureRecognizer::recognitionLoop(std::array<TouchState, TOUCHES_MAX> touches, MouseState mouse, View* view, Sound* soundToPlay)
{
    if (touches[fingers - 1].phase == brls::TouchPhase::START)
    {
        event.fire();
        return brls::GestureState::END;
    }
    return brls::GestureState::UNSURE;
}


StreamingView::StreamingView(Host host, AppInfo app) :
    host(host), app(app)
{
    setFocusable(true);
    setHideHighlight(true);
    loader = new LoadingOverlay(this);
    
    keyboard = new KeyboardView();
    keyboard->detach();
    keyboard->setDetachedPosition(0, 400);
    keyboard->setWidth(Application::contentWidth);
    keyboard->setVisibility(brls::Visibility::GONE);
    addView(keyboard);
    
    addGestureRecognizer(new FingersGestureRecognizer(3, [this] {
        keyboard->setVisibility(brls::Visibility::VISIBLE);
    }));
    
    session = new MoonlightSession(host.address, app.app_id);
    
    ASYNC_RETAIN
    session->start([ASYNC_TOKEN](GSResult<bool> result) {
        ASYNC_RELEASE

        loader->setHidden(true);
        if (!result.isSuccess())
        {
            showError(result.error(), [this]() {
                terminate(false);
            });
        }
    });
    
    addGestureRecognizer(new PanGestureRecognizer([this](PanGestureStatus status, Sound* sound) {
        if (status.state == brls::GestureState::START)
            keyboard->setVisibility(brls::Visibility::GONE);
        
        if (status.state == brls::GestureState::STAY)
            panStatus = status;
    }, PanAxis::ANY));
}

void StreamingView::onFocusGained()
{
    View::onFocusGained();
    Application::blockInputs(true);
}

void StreamingView::onFocusLost()
{
    View::onFocusLost();
    Application::unblockInputs();
}

void StreamingView::draw(NVGcontext* vg, float x, float y, float width, float height, Style style, FrameContext* ctx)
{
    if (!session->is_active())
    {
        brls::Application::notify("Terminate");
        terminate(false);
        return;
    }
    
    session->draw(vg);
    handleInput();
    handleButtonHolding();

    if (session->connection_status_is_poor())
    {
        nvgFontSize(vg, 20);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

        nvgFontBlur(vg, 3);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
        // nvgFontFace(ctx, "icons");
        // nvgText(ctx, 20, height - 30, utf8(FA_EXCLAMATION_TRIANGLE).data(), NULL);
        nvgFontFaceId(vg, Application::getFont(FONT_REGULAR));
        nvgText(vg, 50, height - 28, "Bad connection...", NULL);

        nvgFontBlur(vg, 0);
        nvgFillColor(vg, nvgRGBA(255, 255, 255, 255));
        // nvgFontFace(ctx, "icons");
        // nvgText(ctx, 20, height - 30, utf8(FA_EXCLAMATION_TRIANGLE).data(), NULL);
        nvgFontFaceId(vg, Application::getFont(FONT_REGULAR));
        nvgText(vg, 50, height - 28, "Bad connection...", NULL);
    }
    
    if (draw_stats)
    {
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

        offset += sprintf(&output[offset],
            "Frames dropped by your network connection: %.2f%% (Total: %u)\n"
            "Average receive time: %.2f ms\n"
            "Average decoding time: %.2f ms\n"
            "Average rendering time: %.2f ms\n",
            (float)stats->video_decode_stats.network_dropped_frames / stats->video_decode_stats.total_frames * 100,
            stats->video_decode_stats.network_dropped_frames,
            (float)stats->video_decode_stats.total_reassembly_time / stats->video_decode_stats.received_frames,
            (float)stats->video_decode_stats.total_decode_time / stats->video_decode_stats.decoded_frames,
            (float)stats->video_render_stats.total_render_time / stats->video_render_stats.rendered_frames);

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

void StreamingView::terminate(bool terminateApp)
{
//    terminated = true;
//    InputController::controller()->stop_rumple();

    session->stop(terminateApp);
    this->dismiss();
}

void StreamingView::handleInput()
{
    if (!this->focused)
        return;
    
    static ControllerState controller;
    static RawMouseState mouse;
    Application::getPlatform()->getInputManager()->updateControllerState(&controller);
    Application::getPlatform()->getInputManager()->updateMouseStates(&mouse);
    
    static GamepadState lastGamepadState;
    GamepadState gamepadState
    {
        .buttonFlags = 0,
        .leftTrigger = static_cast<unsigned char>(0xFFFF * (!panStatus.has_value() && controller.buttons[BUTTON_LT] ? 1 : 0)),
        .rightTrigger = static_cast<unsigned char>(0xFFFF * (!panStatus.has_value() && controller.buttons[BUTTON_RT] ? 1 : 0)),
        .leftStickX = static_cast<short>(controller.axes[LEFT_X] * 0x7FFF),
        .leftStickY = static_cast<short>(controller.axes[LEFT_Y] * -0x7FFF),
        .rightStickX = static_cast<short>(controller.axes[RIGHT_X] * 0x7FFF),
        .rightStickY = static_cast<short>(controller.axes[RIGHT_Y] * -0x7FFF),
    };
    
#define SET_GAME_PAD_STATE(LIMELIGHT_KEY, GAMEPAD_BUTTON) \
    controller.buttons[GAMEPAD_BUTTON] ? (gamepadState.buttonFlags |= LIMELIGHT_KEY) : (gamepadState.buttonFlags &= ~LIMELIGHT_KEY);
    
    SET_GAME_PAD_STATE(UP_FLAG, BUTTON_UP);
    SET_GAME_PAD_STATE(DOWN_FLAG, BUTTON_DOWN);
    SET_GAME_PAD_STATE(LEFT_FLAG, BUTTON_LEFT);
    SET_GAME_PAD_STATE(RIGHT_FLAG, BUTTON_RIGHT);

#ifdef __SWITCH__
    SET_GAME_PAD_STATE(A_FLAG, BUTTON_B);
    SET_GAME_PAD_STATE(B_FLAG, BUTTON_A);
    SET_GAME_PAD_STATE(X_FLAG, BUTTON_Y);
    SET_GAME_PAD_STATE(Y_FLAG, BUTTON_X);
#else
    SET_GAME_PAD_STATE(A_FLAG, BUTTON_A);
    SET_GAME_PAD_STATE(B_FLAG, BUTTON_B);
    SET_GAME_PAD_STATE(X_FLAG, BUTTON_X);
    SET_GAME_PAD_STATE(Y_FLAG, BUTTON_Y);
#endif

    SET_GAME_PAD_STATE(BACK_FLAG, BUTTON_BACK);
    SET_GAME_PAD_STATE(PLAY_FLAG, BUTTON_START);

    SET_GAME_PAD_STATE(LB_FLAG, BUTTON_LB);
    SET_GAME_PAD_STATE(RB_FLAG, BUTTON_RB);

    SET_GAME_PAD_STATE(LS_CLK_FLAG, BUTTON_LSB);
    SET_GAME_PAD_STATE(RS_CLK_FLAG, BUTTON_RSB);
    
    if (!gamepadState.is_equal(lastGamepadState))
    {
        lastGamepadState = gamepadState;
        if (LiSendControllerEvent(
              gamepadState.buttonFlags,
              gamepadState.leftTrigger,
              gamepadState.rightTrigger,
              gamepadState.leftStickX,
              gamepadState.leftStickY,
              gamepadState.rightStickX,
              gamepadState.rightStickY) != 0)
            Logger::info("StreamingView: error sending input data");
    }
    
    static MouseStateS lastMouseState;
    MouseStateS mouseState
    {
        .position = mouse.position,
        .scroll_y = mouse.scroll.y,
        .l_pressed = (panStatus.has_value() && controller.buttons[BUTTON_RT]) || mouse.leftButton,
        .m_pressed = mouse.middleButton,
        .r_pressed = (panStatus.has_value() && controller.buttons[BUTTON_LT]) || mouse.rightButton
    };
    
    if (mouseState.l_pressed != lastMouseState.l_pressed)
    {
        lastMouseState.l_pressed = mouseState.l_pressed;
        LiSendMouseButtonEvent(mouseState.l_pressed ? BUTTON_ACTION_PRESS : BUTTON_ACTION_RELEASE, BUTTON_MOUSE_LEFT);
    }
    
    if (mouseState.r_pressed != lastMouseState.r_pressed)
    {
        lastMouseState.r_pressed = mouseState.r_pressed;
        LiSendMouseButtonEvent(mouseState.r_pressed ? BUTTON_ACTION_PRESS : BUTTON_ACTION_RELEASE, BUTTON_MOUSE_RIGHT);
    }
    
//    if (mouseState.position != lastMouseState.position)
//    {
//        lastMouseState.position = mouseState.position;
//        LiSendMousePositionEvent(mouseState.position.x, mouseState.position.y, Application::contentWidth, Application::contentHeight);
//    }
    
    if (mouseState.scroll_y != lastMouseState.scroll_y)
    {
        lastMouseState.scroll_y = mouseState.scroll_y;
        LiSendHighResScrollEvent(mouseState.scroll_y > 0 ? fmax(mouseState.scroll_y, 1) : fmin(mouseState.scroll_y, -1));
    }
    
    if (panStatus.has_value())
    {
        LiSendMouseMoveEvent(-panStatus->delta.x, -panStatus->delta.y);
        panStatus.reset();
    }
    
    static KeyboardState oldKeyboardState;
    KeyboardState keyboardState = keyboard->getKeyboardState();
    
    for (int i = 0; i < _VK_KEY_MAX; i++)
    {
        if (keyboardState.keys[i] != oldKeyboardState.keys[i])
        {
            oldKeyboardState.keys[i] = keyboardState.keys[i];
            LiSendKeyboardEvent(keyboard->getKeyCode((KeyboardKeys)i), keyboardState.keys[i] ? KEY_ACTION_DOWN : KEY_ACTION_UP, 0);
        }
    }
}

void StreamingView::handleButtonHolding()
{
    static ControllerState controller;
    Application::getPlatform()->getInputManager()->updateControllerState(&controller);
    
    static std::chrono::high_resolution_clock::time_point clock_counter;
    static bool buttonState = false;
    static bool used = false;
    
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - clock_counter);
    
    if (!buttonState && controller.buttons[BUTTON_START])
    {
        buttonState = true;
        clock_counter = std::chrono::high_resolution_clock::now();
    }
    else if (buttonState && !controller.buttons[BUTTON_START])
    {
        buttonState = false;
        used = false;
    }
    else if (buttonState && duration.count() > 2 && !used)
    {
        used = true;
        
        IngameOverlay* overlay = new IngameOverlay(this);
        overlay->setTitle(host.hostname + ": " + app.name);
        Application::pushActivity(new Activity(overlay));
    }
}

void StreamingView::onLayout()
{
    Box::onLayout();
    if (loader)
        loader->layout();
}

StreamingView::~StreamingView()
{
    session->stop(false);
    delete session;
}
