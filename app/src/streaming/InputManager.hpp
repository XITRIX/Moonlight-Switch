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
    brls::Point position;
    float scroll_y;
    bool l_pressed;
    bool m_pressed;
    bool r_pressed;
};

class MoonlightInputManager : public Singleton<MoonlightInputManager>
{
public:
    MoonlightInputManager();
    void handleInput();
    void handleRumble(unsigned short controller, unsigned short low_freq_motor, unsigned short high_freq_motor);
    void updateTouchScreenPanDelta(brls::PanGestureStatus panStatus);
    
private:
    std::optional<brls::PanGestureStatus> panStatus;
};

