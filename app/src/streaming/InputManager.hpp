//
//  InputManager.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 27.05.2021.
//

#pragma once

#include "Singleton.hpp"
#include "keyboard_view.hpp"
#include <borealis.hpp>
#include <optional>

// Moonlight ready gamepad
struct GamepadState {
    short buttonFlags = 0;
    unsigned char leftTrigger = 0;
    unsigned char rightTrigger = 0;
    short leftStickX = 0;
    short leftStickY = 0;
    short rightStickX = 0;
    short rightStickY = 0;

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
    float scroll_y = 0;
    bool l_pressed = 0;
    bool m_pressed = 0;
    bool r_pressed = 0;
};

struct RumbleValues {
    unsigned short lowFreqMotor;
    unsigned short highFreqMotor;
    uint16_t leftTriggerMotor;
    uint16_t rightTriggerMotor;
};

class MoonlightInputManager : public Singleton<MoonlightInputManager> {
  public:
    MoonlightInputManager();
    void dropInput();
    void handleInput();
    void handleRumble(unsigned short controller, unsigned short lowFreqMotor, unsigned short highFreqMotor);
    void handleRumbleTriggers(unsigned short controller, unsigned short lowFreqMotor, unsigned short highFreqMotor);
    void updateTouchScreenPanDelta(brls::PanGestureStatus panStatus);
    void reloadButtonMappingLayout();
    static void leftMouseClick();
    static void rightMouseClick();

  private:
    RumbleValues rumbleCache[GAMEPADS_MAX];
    GamepadState lastGamepadStates[GAMEPADS_MAX];
    brls::ControllerButton mappingButtons[brls::_BUTTON_MAX];
    std::optional<brls::PanGestureStatus> panStatus;
    std::map<uint32_t, bool> activeTouchIDs;
    bool inputDropped = false;

    brls::ControllerState mapController(brls::ControllerState controller);
    static short glfwKeyToVKKey(brls::BrlsKeyboardScancode key);

    GamepadState getControllerState(int controllerNum, bool specialKey);
    void handleControllers(bool specialKey);

    static short controllersToMap();
};
