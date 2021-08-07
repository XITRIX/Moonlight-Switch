
#ifdef __SWITCH__
#include <switch.h>
#endif

#include "InputManager.hpp"
#include "Settings.hpp"
#include "Limelight.h"
#include <chrono>

MoonlightInputManager::MoonlightInputManager()
{
    
}

void MoonlightInputManager::updateTouchScreenPanDelta(brls::PanGestureStatus panStatus)
{
    this->panStatus = panStatus;
}

void MoonlightInputManager::handleRumble(unsigned short controller, unsigned short lowFreqMotor, unsigned short highFreqMotor)
{
    brls::Application::getPlatform()->getInputManager()->sendRumble(controller, lowFreqMotor, highFreqMotor);
}

void MoonlightInputManager::handleInput() 
{
    static brls::ControllerState controller;
    static brls::RawMouseState mouse;
    brls::Application::getPlatform()->getInputManager()->updateControllerState(&controller);
    brls::Application::getPlatform()->getInputManager()->updateMouseStates(&mouse);

#ifdef __SWITCH__
    static HidTouchScreenState hidState = { 0 };
    hidGetTouchScreenStates(&hidState, 1);
    bool specialKey = hidState.count > 0;
#else
    bool specialKey = false;
#endif
    
    static GamepadState lastGamepadState;
    GamepadState gamepadState
    {
        .buttonFlags = 0,
        .leftTrigger = static_cast<unsigned char>(0xFFFF * (!specialKey && controller.buttons[brls::BUTTON_LT] ? 1 : 0)),
        .rightTrigger = static_cast<unsigned char>(0xFFFF * (!specialKey && controller.buttons[brls::BUTTON_RT] ? 1 : 0)),
        .leftStickX = static_cast<short>(0x7FFF * controller.axes[brls::LEFT_X]),
        .leftStickY = static_cast<short>(-0x7FFF * (!specialKey ? controller.axes[brls::LEFT_Y] : 0)),
        .rightStickX = static_cast<short>(0x7FFF * controller.axes[brls::RIGHT_X]),
        .rightStickY = static_cast<short>(-0x7FFF * (!specialKey ? controller.axes[brls::RIGHT_Y] : 0)),
    };
    
    brls::ControllerButton a;
    brls::ControllerButton b;
    brls::ControllerButton x;
    brls::ControllerButton y;
    
    if (Settings::instance().swap_game_keys())
    {
        a = brls::BUTTON_B;
        b = brls::BUTTON_A;
        x = brls::BUTTON_Y;
        y = brls::BUTTON_X;
    }
    else
    {
        a = brls::BUTTON_A;
        b = brls::BUTTON_B;
        x = brls::BUTTON_X;
        y = brls::BUTTON_Y;
    }
    
#define SET_GAME_PAD_STATE(LIMELIGHT_KEY, GAMEPAD_BUTTON) \
    controller.buttons[GAMEPAD_BUTTON] ? (gamepadState.buttonFlags |= LIMELIGHT_KEY) : (gamepadState.buttonFlags &= ~LIMELIGHT_KEY);
    
    SET_GAME_PAD_STATE(UP_FLAG, brls::BUTTON_UP);
    SET_GAME_PAD_STATE(DOWN_FLAG, brls::BUTTON_DOWN);
    SET_GAME_PAD_STATE(LEFT_FLAG, brls::BUTTON_LEFT);
    SET_GAME_PAD_STATE(RIGHT_FLAG, brls::BUTTON_RIGHT);

#ifdef __SWITCH__
    SET_GAME_PAD_STATE(A_FLAG, b);
    SET_GAME_PAD_STATE(B_FLAG, a);
    SET_GAME_PAD_STATE(X_FLAG, y);
    SET_GAME_PAD_STATE(Y_FLAG, x);
#else
    SET_GAME_PAD_STATE(A_FLAG, a);
    SET_GAME_PAD_STATE(B_FLAG, b);
    SET_GAME_PAD_STATE(X_FLAG, x);
    SET_GAME_PAD_STATE(Y_FLAG, y);
#endif

    SET_GAME_PAD_STATE(BACK_FLAG, brls::BUTTON_BACK);
    SET_GAME_PAD_STATE(PLAY_FLAG, brls::BUTTON_START);

    SET_GAME_PAD_STATE(LB_FLAG, brls::BUTTON_LB);
    SET_GAME_PAD_STATE(RB_FLAG, brls::BUTTON_RB);

    SET_GAME_PAD_STATE(LS_CLK_FLAG, brls::BUTTON_LSB);
    SET_GAME_PAD_STATE(RS_CLK_FLAG, brls::BUTTON_RSB);
    
    auto guideKeys = Settings::instance().guide_key_options().buttons;
    bool guideCombo = guideKeys.size() > 0;
    for (auto key: guideKeys)
        guideCombo &= controller.buttons[key];
    guideCombo ? (gamepadState.buttonFlags |= SPECIAL_FLAG) : (gamepadState.buttonFlags &= ~SPECIAL_FLAG);
    
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
            brls::Logger::info("StreamingView: error sending input data");
    }
    
    float stickScrolling = specialKey ? (controller.axes[brls::LEFT_Y] + controller.axes[brls::RIGHT_Y]) : 0;
    
    static MouseStateS lastMouseState;
    MouseStateS mouseState
    {
        .position = mouse.position,
        .scroll_y = stickScrolling + mouse.scroll.y,
        .l_pressed = (specialKey && controller.buttons[brls::BUTTON_RT]) || mouse.leftButton,
        .m_pressed = mouse.middleButton,
        .r_pressed = (specialKey && controller.buttons[brls::BUTTON_LT]) || mouse.rightButton
    };
    
    if (Settings::instance().swap_mouse_scroll())
        mouseState.scroll_y *= -1;
    
    if (mouseState.l_pressed != lastMouseState.l_pressed)
    {
        lastMouseState.l_pressed = mouseState.l_pressed;
        auto lb = Settings::instance().swap_mouse_keys() ? BUTTON_MOUSE_RIGHT : BUTTON_MOUSE_LEFT;
        LiSendMouseButtonEvent(mouseState.l_pressed ? BUTTON_ACTION_PRESS : BUTTON_ACTION_RELEASE, lb);
    }
    
    if (mouseState.r_pressed != lastMouseState.r_pressed)
    {
        lastMouseState.r_pressed = mouseState.r_pressed;
        auto rb = Settings::instance().swap_mouse_keys() ? BUTTON_MOUSE_LEFT : BUTTON_MOUSE_RIGHT;
        LiSendMouseButtonEvent(mouseState.r_pressed ? BUTTON_ACTION_PRESS : BUTTON_ACTION_RELEASE, rb);
    }
    
//    if (mouseState.position != lastMouseState.position)
//    {
//        lastMouseState.position = mouseState.position;
//        LiSendMousePositionEvent(mouseState.position.x, mouseState.position.y, Application::contentWidth, Application::contentHeight);
//    }
    
    std::chrono::high_resolution_clock::time_point timeNow = std::chrono::high_resolution_clock::now();
    static std::chrono::high_resolution_clock::time_point timeStamp = timeNow;
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(timeNow - timeStamp).count();
    if (mouseState.scroll_y != 0 && duration > 550 - abs(mouseState.scroll_y) * 500)
    {
        timeStamp = timeNow;
        brls::Logger::info("Scroll sended: {}", mouseState.scroll_y);
        lastMouseState.scroll_y = mouseState.scroll_y;
        LiSendHighResScrollEvent(mouseState.scroll_y > 0 ? 1 : -1);
    }
    
    if (panStatus.has_value())
    {
        LiSendMouseMoveEvent(-panStatus->delta.x, -panStatus->delta.y);
        panStatus.reset();
    }
}
