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
#include <atomic>
#include <chrono>
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

// Keeps the buttons of a local combo, like the overlay one, away from the
// host. A lone combo button is held back for a moment in case the rest of the
// combo follows; once the whole combo is down, its buttons stay held back
// until they are let go.
class ComboHold {
  public:
    uint32_t update(uint32_t buttons, uint32_t combo,
                    std::chrono::steady_clock::time_point now);

  private:
    uint32_t forwarded = 0;
    uint32_t waiting = 0;
    uint32_t suppressed = 0;
    std::chrono::steady_clock::time_point since;
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
    void resetControllerState() { resetControllers = true; }
    void handleInput(bool ignoreTouch = false);
    void sendMouseScroll(brls::Point scroll);
    void handleRumble(unsigned short controller, unsigned short lowFreqMotor, unsigned short highFreqMotor);
    void handleRumbleTriggers(unsigned short controller, unsigned short lowFreqMotor, unsigned short highFreqMotor);
    void updateTouchScreenPanDelta(brls::PanGestureStatus panStatus);
    void reloadButtonMappingLayout();
    void setInputEnabled(bool enabled) { inputEnabled = enabled; }
    static void leftMouseClick();
    static void rightMouseClick();

  private:
    enum class DesktopScrollAxis {
        None,
        Horizontal,
        Vertical,
    };

    RumbleValues rumbleCache[GAMEPADS_MAX];
    GamepadState lastGamepadStates[GAMEPADS_MAX];
    ComboHold comboHolds[GAMEPADS_MAX][3];
    brls::ControllerButton mappingButtons[brls::_BUTTON_MAX];
    std::optional<brls::PanGestureStatus> panStatus;
    std::map<uint32_t, bool> activeTouchIDs;
    brls::Point desktopMouseRemainder = {0, 0};
    brls::Point desktopScrollRemainder = {0, 0};
    brls::Point scrollLogInput = {0, 0};
    int64_t scrollLogSentX = 0;
    int64_t scrollLogSentY = 0;
    unsigned scrollLogEvents = 0;
    float loggedScrollSensitivity = -1;
    std::chrono::steady_clock::time_point lastScrollLog = {};
    float pendingHorizontalScroll = 0;
    unsigned pendingHorizontalScrollEvents = 0;
    DesktopScrollAxis desktopScrollAxis = DesktopScrollAxis::None;
    std::chrono::steady_clock::time_point lastDesktopScrollEvent;
    bool inputDropped = false;
    bool inputEnabled = true;
    std::atomic<bool> resetControllers = true;
    int announcedControllerCount = -1;

    brls::ControllerState mapController(brls::ControllerState controller);
    static short glfwKeyToVKKey(brls::BrlsKeyboardScancode key);
    void sendRelativeMouseMove(brls::Point offset);
    void handleDesktopMouseScroll(brls::Point scroll);

    GamepadState getControllerState(int controllerNum, bool specialKey);
    void holdBackLocalCombos(int controllerNum, brls::ControllerState& controller);
    void handleControllers(bool specialKey);

    static short controllersToMap();
};
