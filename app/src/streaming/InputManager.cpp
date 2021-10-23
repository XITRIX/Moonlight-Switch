
#ifdef __SWITCH__
#include <switch.h>
#endif

#include "InputManager.hpp"
#include "Settings.hpp"
#include "Limelight.h"
#include <borealis.hpp>
#include <chrono>

using namespace brls;

MoonlightInputManager::MoonlightInputManager()
{
    brls::Application::getPlatform()->getInputManager()->getMouseCusorOffsetChanged()->subscribe([](brls::Point offset) {
        if (offset.x != 0 || offset.y != 0)
        {
            float multiplier = Settings::instance().get_mouse_speed_multiplier() / 100.f * 1.5f + 0.5f;
            LiSendMouseMoveEvent(offset.x * multiplier, offset.y * multiplier);
        }
    });
    
    brls::Application::getPlatform()->getInputManager()->getMouseScrollOffsetChanged()->subscribe([](brls::Point scroll) {
        if (scroll.y != 0)
        {
//            signed char count = scroll.y > 0 ? 1 : -1;
//            LiSendScrollEvent(count);
            LiSendHighResScrollEvent(scroll.y);
        }
    });
    
    brls::Application::getPlatform()->getInputManager()->getKeyboardKeyStateChanged()->subscribe([this](brls::KeyState state) {
        int vkKey = this->glfwKeyToVKKey(state.key);
        char modifiers = state.mods;
        LiSendKeyboardEvent(vkKey, state.pressed ? KEY_ACTION_DOWN : KEY_ACTION_UP, modifiers);
    });
}

void MoonlightInputManager::reloadButtonMappingLayout()
{
    KeyMappingLayout layout = (*Settings::instance().get_mapping_laouts())[Settings::instance().get_current_mapping_layout()];
    for (int i = 0; i < _BUTTON_MAX; i++) {
        if (layout.mapping.count(i) == 1) {
            mappingButtons[i] = (brls::ControllerButton)layout.mapping.at(i);
        } else {
            mappingButtons[i] = (brls::ControllerButton)i;
        }
    }
}

void MoonlightInputManager::updateTouchScreenPanDelta(brls::PanGestureStatus panStatus)
{
    this->panStatus = panStatus;
}

void MoonlightInputManager::handleRumble(unsigned short controller, unsigned short lowFreqMotor, unsigned short highFreqMotor)
{
    brls::Application::getPlatform()->getInputManager()->sendRumble(controller, lowFreqMotor, highFreqMotor);
}

void MoonlightInputManager::dropInput()
{
    if (inputDropped) return;
    
    GamepadState gamepadState;
    if (LiSendControllerEvent(
          gamepadState.buttonFlags,
          gamepadState.leftTrigger,
          gamepadState.rightTrigger,
          gamepadState.leftStickX,
          gamepadState.leftStickY,
          gamepadState.rightStickX,
          gamepadState.rightStickY) == 0)
        inputDropped = true;
}

GamepadState MoonlightInputManager::getControllerState(int controllerNum, bool specialKey)
{
    brls::ControllerState rawController;
    brls::ControllerState controller;

    brls::Application::getPlatform()->getInputManager()->updateControllerState(&rawController, controllerNum);
    controller = mapController(rawController);

    GamepadState gamepadState
    {
        .buttonFlags = 0,
        .leftTrigger = static_cast<unsigned char>(0xFFFF * (!specialKey && controller.buttons[brls::BUTTON_LT] ? 1 : 0)),
        .rightTrigger = static_cast<unsigned char>(0xFFFF * (!specialKey && controller.buttons[brls::BUTTON_RT] ? 1 : 0)),
        .leftStickX = static_cast<short>(0x7FFF * (!specialKey ? controller.axes[brls::LEFT_X] : 0)),
        .leftStickY = static_cast<short>(-0x7FFF * (!specialKey ? controller.axes[brls::LEFT_Y] : 0)),
        .rightStickX = static_cast<short>(0x7FFF * (!specialKey ? controller.axes[brls::RIGHT_X] : 0)),
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

    if (guideCombo || lastGamepadStates[controllerNum].buttonFlags & SPECIAL_FLAG) gamepadState.buttonFlags = 0;
    guideCombo ? (gamepadState.buttonFlags |= SPECIAL_FLAG) : (gamepadState.buttonFlags &= ~SPECIAL_FLAG);

    return gamepadState;
}

void MoonlightInputManager::handleControllers(bool specialKey)
{
    for (int i = 0; i < brls::Application::getPlatform()->getInputManager()->getControllersConnectedCount(); i++) {
        GamepadState gamepadState = getControllerState(i, specialKey);

        static short lastControllersCount = -1;
        short controllersCount = controllersToMap();

        if (!gamepadState.is_equal(lastGamepadStates[i]))
        {
            lastGamepadStates[i] = gamepadState;
            if (LiSendMultiControllerEvent(
                  i,
                  controllersCount,
                  gamepadState.buttonFlags,
                  gamepadState.leftTrigger,
                  gamepadState.rightTrigger,
                  gamepadState.leftStickX,
                  gamepadState.leftStickY,
                  gamepadState.rightStickX,
                  gamepadState.rightStickY) != 0)
                brls::Logger::info("StreamingView: error sending input data");
        }
    }
}

void MoonlightInputManager::handleInput() 
{
    inputDropped = false;
    static brls::ControllerState rawController;
    static brls::ControllerState controller;
    static brls::RawMouseState mouse;

    brls::Application::getPlatform()->getInputManager()->updateUnifiedControllerState(&rawController);
    brls::Application::getPlatform()->getInputManager()->updateMouseStates(&mouse);
    controller = mapController(rawController);

#ifdef __SWITCH__
    static HidTouchScreenState hidState = { 0 };
    hidGetTouchScreenStates(&hidState, 1);
    bool specialKey = hidState.count > 0;
#else
    bool specialKey = false;
#endif

    handleControllers(specialKey);
    
    float stickScrolling = specialKey ? (controller.axes[brls::LEFT_Y] + controller.axes[brls::RIGHT_Y]) : 0;
    
    static MouseStateS lastMouseState;
    MouseStateS mouseState
    {
        .scroll_y = stickScrolling,// + mouse.scroll.y,
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
    
    if (mouseState.m_pressed != lastMouseState.m_pressed)
    {
        lastMouseState.m_pressed = mouseState.m_pressed;
        LiSendMouseButtonEvent(mouseState.m_pressed ? BUTTON_ACTION_PRESS : BUTTON_ACTION_RELEASE, BUTTON_MOUSE_MIDDLE);
    }
    
    if (mouseState.r_pressed != lastMouseState.r_pressed)
    {
        lastMouseState.r_pressed = mouseState.r_pressed;
        auto rb = Settings::instance().swap_mouse_keys() ? BUTTON_MOUSE_LEFT : BUTTON_MOUSE_RIGHT;
        LiSendMouseButtonEvent(mouseState.r_pressed ? BUTTON_ACTION_PRESS : BUTTON_ACTION_RELEASE, rb);
    }
    
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
        float multiplier = Settings::instance().get_mouse_speed_multiplier() / 100.f * 1.5f + 0.5f;
        LiSendMouseMoveEvent(-panStatus->delta.x * multiplier, -panStatus->delta.y * multiplier);
        panStatus.reset();
    }
}

short MoonlightInputManager::controllersToMap()
{
    switch (brls::Application::getPlatform()->getInputManager()->getControllersConnectedCount()) {
        case 0:
            return 0x0;
        case 1:
            return 0x1;
        case 2:
            return 0x3;
        case 3:
            return 0x7;
        default:
            return 0xF;
    }
}

brls::ControllerState MoonlightInputManager::mapController(brls::ControllerState controller)
{
    brls::ControllerState result;

    std::fill(result.buttons, result.buttons + sizeof(result.buttons), false);

    for (int i = 0; i < _AXES_MAX; i++)
        result.axes[i] = controller.axes[i];


    for (int i = 0; i < _BUTTON_MAX; i++) {
        result.buttons[mappingButtons[i]] |= controller.buttons[i];
    }

    return result;
}

int MoonlightInputManager::glfwKeyToVKKey(BrlsKeyboardScancode key)
{
    if (BRLS_KBD_KEY_F1 <= key && key <= BRLS_KBD_KEY_F12)
        return key - BRLS_KBD_KEY_F1 + 0x70;

    if (BRLS_KBD_KEY_KP_0 <= key && key <= BRLS_KBD_KEY_KP_9)
        return key - BRLS_KBD_KEY_KP_0 + 0x60;

    switch (key) {
        case BRLS_KBD_KEY_BACKSPACE:
            return 0x08;
        case BRLS_KBD_KEY_PERIOD:
            return 0xBE;
        case BRLS_KBD_KEY_GRAVE_ACCENT:
            return 0xC0;
        case BRLS_KBD_KEY_LEFT_BRACKET:
            return 0xDB;
        case BRLS_KBD_KEY_BACKSLASH:
            return 0xDC;
        case BRLS_KBD_KEY_APOSTROPHE:
            return 0xDE;
        case BRLS_KBD_KEY_TAB:
            return 0x09;
        case BRLS_KBD_KEY_CAPS_LOCK:
            return 0x14;
        case BRLS_KBD_KEY_LEFT_SHIFT:
            return 0xA0;
        case BRLS_KBD_KEY_RIGHT_SHIFT:
            return 0xA1;
        case BRLS_KBD_KEY_LEFT_CONTROL:
            return 0xA2;
        case BRLS_KBD_KEY_RIGHT_CONTROL:
            return 0xA3;
        case BRLS_KBD_KEY_LEFT_ALT:
            return 0xA4;
        case BRLS_KBD_KEY_RIGHT_ALT:
            return 0xA5;
        case BRLS_KBD_KEY_DELETE:
            return 0x2E;
        case BRLS_KBD_KEY_ENTER:
            return 0x0D;
        case BRLS_KBD_KEY_LEFT_SUPER:
            return 0x5B;
        case BRLS_KBD_KEY_RIGHT_SUPER:
            return 0x5C;
        case BRLS_KBD_KEY_LEFT:
            return 0x25;
        case BRLS_KBD_KEY_UP:
            return 0x26;
        case BRLS_KBD_KEY_RIGHT:
            return 0x27;
        case BRLS_KBD_KEY_DOWN:
            return 0x28;
        case BRLS_KBD_KEY_ESCAPE:
            return 0x1B;
        case BRLS_KBD_KEY_KP_ADD:
            return 0x6B;
        case BRLS_KBD_KEY_KP_DECIMAL:
            return 0x6E;
        case BRLS_KBD_KEY_KP_DIVIDE:
            return 0x6F;
        case BRLS_KBD_KEY_KP_MULTIPLY:
            return 0x6A;
        case BRLS_KBD_KEY_KP_ENTER:
            return 0x0D;
        case BRLS_KBD_KEY_NUM_LOCK:
            return 0x90;
        case BRLS_KBD_KEY_SCROLL_LOCK:
            return 0x91;

        default:
            return key;
    }
}
