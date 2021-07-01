#include "InputManager.hpp"
#include "Limelight.h"

MoonlightInputManager::MoonlightInputManager()
{
    
}

void MoonlightInputManager::updateTouchScreenPanDelta(brls::PanGestureStatus panStatus)
{
    this->panStatus = panStatus;
}

void MoonlightInputManager::handleRumble(unsigned short controller, unsigned short low_freq_motor, unsigned short high_freq_motor)
{
    brls::Application::getPlatform()->getInputManager()->sendRumble(controller, low_freq_motor, high_freq_motor);
}

void MoonlightInputManager::handleInput() 
{
    static brls::ControllerState controller;
    static brls::RawMouseState mouse;
    brls::Application::getPlatform()->getInputManager()->updateControllerState(&controller);
    brls::Application::getPlatform()->getInputManager()->updateMouseStates(&mouse);
    
    static GamepadState lastGamepadState;
    GamepadState gamepadState
    {
        .buttonFlags = 0,
        .leftTrigger = static_cast<unsigned char>(0xFFFF * (!panStatus.has_value() && controller.buttons[brls::BUTTON_LT] ? 1 : 0)),
        .rightTrigger = static_cast<unsigned char>(0xFFFF * (!panStatus.has_value() && controller.buttons[brls::BUTTON_RT] ? 1 : 0)),
        .leftStickX = static_cast<short>(controller.axes[brls::LEFT_X] * 0x7FFF),
        .leftStickY = static_cast<short>(controller.axes[brls::LEFT_Y] * -0x7FFF),
        .rightStickX = static_cast<short>(controller.axes[brls::RIGHT_X] * 0x7FFF),
        .rightStickY = static_cast<short>(controller.axes[brls::RIGHT_Y] * -0x7FFF),
    };
    
#define SET_GAME_PAD_STATE(LIMELIGHT_KEY, GAMEPAD_BUTTON) \
    controller.buttons[GAMEPAD_BUTTON] ? (gamepadState.buttonFlags |= LIMELIGHT_KEY) : (gamepadState.buttonFlags &= ~LIMELIGHT_KEY);
    
    SET_GAME_PAD_STATE(UP_FLAG, brls::BUTTON_UP);
    SET_GAME_PAD_STATE(DOWN_FLAG, brls::BUTTON_DOWN);
    SET_GAME_PAD_STATE(LEFT_FLAG, brls::BUTTON_LEFT);
    SET_GAME_PAD_STATE(RIGHT_FLAG, brls::BUTTON_RIGHT);

#ifdef __SWITCH__
    SET_GAME_PAD_STATE(A_FLAG, brls::BUTTON_B);
    SET_GAME_PAD_STATE(B_FLAG, brls::BUTTON_A);
    SET_GAME_PAD_STATE(X_FLAG, brls::BUTTON_Y);
    SET_GAME_PAD_STATE(Y_FLAG, brls::BUTTON_X);
#else
    SET_GAME_PAD_STATE(A_FLAG, brls::BUTTON_A);
    SET_GAME_PAD_STATE(B_FLAG, brls::BUTTON_B);
    SET_GAME_PAD_STATE(X_FLAG, brls::BUTTON_X);
    SET_GAME_PAD_STATE(Y_FLAG, brls::BUTTON_Y);
#endif

    SET_GAME_PAD_STATE(BACK_FLAG, brls::BUTTON_BACK);
    SET_GAME_PAD_STATE(PLAY_FLAG, brls::BUTTON_START);

    SET_GAME_PAD_STATE(LB_FLAG, brls::BUTTON_LB);
    SET_GAME_PAD_STATE(RB_FLAG, brls::BUTTON_RB);

    SET_GAME_PAD_STATE(LS_CLK_FLAG, brls::BUTTON_LSB);
    SET_GAME_PAD_STATE(RS_CLK_FLAG, brls::BUTTON_RSB);
    
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
    
    static MouseStateS lastMouseState;
    MouseStateS mouseState
    {
        .position = mouse.position,
        .scroll_y = mouse.scroll.y,
        .l_pressed = (panStatus.has_value() && controller.buttons[brls::BUTTON_RT]) || mouse.leftButton,
        .m_pressed = mouse.middleButton,
        .r_pressed = (panStatus.has_value() && controller.buttons[brls::BUTTON_LT]) || mouse.rightButton
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
}
