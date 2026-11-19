//
//  onscreen_gamepad.cpp
//  Moonlight
//

#include "onscreen_gamepad.hpp"
#include <Limelight.h>
#include <algorithm>
#include <cmath>
#include <nanovg.h>

using namespace brls;

namespace {
enum class Kind { Button, Trigger, Stick, Dpad, View, Menu };

// In content pixels from the top left, or from the top right when `right` is
// set. Circles have a radius; rectangles a width and height.
struct Control {
    Kind kind;
    int flag;
    bool right;
    float x, y, radius, w, h;
    const char* label;
};

constexpr float STICK_RADIUS = 100;
constexpr float KNOB_RADIUS = 40;
constexpr float STICK_TRAVEL = STICK_RADIUS - KNOB_RADIUS;
constexpr float STICK_DEADZONE = 0.08f;
constexpr float AXIS_MAX = 32767;
constexpr float BUTTON_SLACK = 1.4f;
constexpr float PAD_SLACK = 1.25f;
constexpr float DPAD_CENTER = 18;
constexpr float DPAD_ARM = 28;
constexpr float DPAD_REACH = 84;
constexpr int DPAD_PRESSED_SHIFT = 16;

// Laid out like the Joy-Con: the left stick above the D-pad, the face buttons
// above the right stick and the shoulders along the top edge. The face buttons
// sit where an Xbox pad has them, since that is what the host sees.
constexpr Control CONTROLS[] = {
    {Kind::Trigger, 0, false, 40, 40, 0, 150, 54, "LT"},
    {Kind::Button, LB_FLAG, false, 40, 104, 0, 150, 54, "LB"},
    {Kind::Trigger, 1, true, 190, 40, 0, 150, 54, "RT"},
    {Kind::Button, RB_FLAG, true, 190, 104, 0, 150, 54, "RB"},
    {Kind::View, BACK_FLAG, false, 290, 72, 26, 0, 0, nullptr},
    {Kind::Menu, PLAY_FLAG, true, 290, 72, 26, 0, 0, nullptr},
    {Kind::Button, LS_CLK_FLAG, false, 300, 236, 28, 0, 0, "LS"},
    {Kind::Button, RS_CLK_FLAG, true, 300, 236, 28, 0, 0, "RS"},
    {Kind::Stick, 0, false, 150, 340, STICK_RADIUS, 0, 0, nullptr},
    {Kind::Dpad, 0, false, 270, 560, 96, 0, 0, nullptr},
    {Kind::Stick, 1, true, 270, 560, STICK_RADIUS, 0, 0, nullptr},
    {Kind::Button, Y_FLAG, true, 150, 278, 34, 0, 0, "Y"},
    {Kind::Button, A_FLAG, true, 150, 402, 34, 0, 0, "A"},
    {Kind::Button, X_FLAG, true, 212, 340, 34, 0, 0, "X"},
    {Kind::Button, B_FLAG, true, 88, 340, 34, 0, 0, "B"},
};
constexpr int CONTROL_COUNT = sizeof(CONTROLS) / sizeof(CONTROLS[0]);

Point center(const Control& control, float width) {
    float x = control.right ? width - control.x : control.x;
    if (control.radius > 0)
        return {x, control.y};
    return {x + control.w / 2, control.y + control.h / 2};
}

// 0 at the control's centre and 1 on the edge of the area it takes touches
// in, which is a little larger than what is drawn.
float reach(const Control& control, Point at, float width) {
    Point mid = center(control, width);
    float dx = at.x - mid.x;
    float dy = at.y - mid.y;
    if (control.radius > 0) {
        bool wide = control.kind == Kind::Stick || control.kind == Kind::Dpad;
        return std::hypot(dx, dy) /
               (control.radius * (wide ? PAD_SLACK : BUTTON_SLACK));
    }
    return std::max(std::fabs(dx) / (control.w / 2 + 20),
                    std::fabs(dy) / (control.h / 2 + 6));
}

int controlAt(Point at, float width) {
    int best = -1;
    float bestReach = 1;
    for (int i = 0; i < CONTROL_COUNT; i++) {
        float value = reach(CONTROLS[i], at, width);
        if (value <= bestReach) {
            best = i;
            bestReach = value;
        }
    }
    return best;
}

int dpadFlags(float dx, float dy) {
    if (std::hypot(dx, dy) < DPAD_CENTER)
        return 0;

    constexpr float TAN_22_5 = 0.41421357f;
    int horizontal = dx > 0 ? RIGHT_FLAG : LEFT_FLAG;
    int vertical = dy > 0 ? DOWN_FLAG : UP_FLAG;
    if (std::fabs(dy) < std::fabs(dx) * TAN_22_5)
        return horizontal;
    if (std::fabs(dx) < std::fabs(dy) * TAN_22_5)
        return vertical;
    return horizontal | vertical;
}

uint32_t dpadPressed(int flags) {
    uint32_t bits = 0;
    if (flags & LEFT_FLAG) bits |= 1;
    if (flags & UP_FLAG) bits |= 2;
    if (flags & RIGHT_FLAG) bits |= 4;
    if (flags & DOWN_FLAG) bits |= 8;
    return bits << DPAD_PRESSED_SHIFT;
}

NVGcolor faceColor(const char* label, unsigned char alpha) {
    switch (label ? label[0] : 0) {
        case 'A': return nvgRGBA(16, 124, 16, alpha);
        case 'B': return nvgRGBA(200, 30, 40, alpha);
        case 'X': return nvgRGBA(0, 100, 200, alpha);
        case 'Y': return nvgRGBA(220, 160, 0, alpha);
        default: return nvgRGBA(0, 0, 0, alpha);
    }
}

void shapePath(NVGcontext* vg, const Control& control, Point mid) {
    nvgBeginPath(vg);
    if (control.radius > 0)
        nvgCircle(vg, mid.x, mid.y, control.radius);
    else
        nvgRoundedRect(vg, mid.x - control.w / 2, mid.y - control.h / 2,
                       control.w, control.h, 12);
}

void fillAndStroke(NVGcontext* vg, NVGcolor fill, NVGcolor line) {
    nvgFillColor(vg, fill);
    nvgFill(vg);
    nvgStrokeColor(vg, line);
    nvgStrokeWidth(vg, 2);
    nvgStroke(vg);
}
}

void OnscreenGamepad::update(const std::vector<TouchState>& touches,
                             float width) {
    std::map<int, int> live;
    int buttons = 0;
    GamepadState state;
    Knob newKnobs[2];
    uint32_t held = 0;
    ownedTouches.clear();

    for (const auto& touch : touches) {
        auto owner = fingers.find(touch.fingerId);
        int index;
        if (owner != fingers.end())
            index = owner->second;
        else if (touch.phase == TouchPhase::START)
            index = controlAt(touch.position, width);
        else
            continue;

        if (index < 0)
            continue;

        ownedTouches.insert(touch.fingerId);
        if (touch.phase == TouchPhase::END || touch.phase == TouchPhase::NONE)
            continue;

        live[touch.fingerId] = index;
        const Control& control = CONTROLS[index];
        Point mid = center(control, width);
        float dx = touch.position.x - mid.x;
        float dy = touch.position.y - mid.y;

        switch (control.kind) {
            case Kind::Button:
            case Kind::View:
            case Kind::Menu:
            case Kind::Trigger:
                if (reach(control, touch.position, width) > 1)
                    break;
                held |= 1u << index;
                if (control.kind != Kind::Trigger)
                    buttons |= control.flag;
                else if (control.flag == 0)
                    state.leftTrigger = 0xFF;
                else
                    state.rightTrigger = 0xFF;
                break;
            case Kind::Dpad: {
                int flags = dpadFlags(dx, dy);
                buttons |= flags;
                held |= dpadPressed(flags);
                break;
            }
            case Kind::Stick: {
                held |= 1u << index;
                float length = std::hypot(dx, dy);
                if (length <= 0)
                    break;

                float distance = std::min(length, STICK_TRAVEL);
                float ux = dx / length;
                float uy = dy / length;
                newKnobs[control.flag] = {ux * distance, uy * distance};

                float tilt = distance / STICK_TRAVEL;
                if (tilt < STICK_DEADZONE)
                    break;
                tilt = (tilt - STICK_DEADZONE) / (1 - STICK_DEADZONE);
                short x = (short) std::lround(ux * tilt * AXIS_MAX);
                short y = (short) std::lround(-uy * tilt * AXIS_MAX);
                if (control.flag == 0) {
                    state.leftStickX = x;
                    state.leftStickY = y;
                } else {
                    state.rightStickX = x;
                    state.rightStickY = y;
                }
                break;
            }
        }
    }

    state.buttonFlags = (short) buttons;
    fingers = live;
    current = state;
    knobs[0] = newKnobs[0];
    knobs[1] = newKnobs[1];
    pressed = held;
}

void OnscreenGamepad::release() {
    fingers.clear();
    ownedTouches.clear();
    current = {};
    knobs[0] = knobs[1] = {};
    pressed = 0;
}

bool OnscreenGamepad::hit(Point point, float width) const {
    return controlAt(point, width) >= 0;
}

void OnscreenGamepad::draw(NVGcontext* vg, float width) {
    nvgFontFaceId(vg, Application::getFont(FONT_REGULAR));
    nvgFontSize(vg, 22);
    nvgFontBlur(vg, 0);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

    for (int i = 0; i < CONTROL_COUNT; i++) {
        const Control& control = CONTROLS[i];
        const bool down = pressed & (1u << i);
        const Point mid = center(control, width);
        const NVGcolor line = nvgRGBA(255, 255, 255, down ? 230 : 140);
        const NVGcolor fill = faceColor(control.label, down ? 190 : 100);

        switch (control.kind) {
            case Kind::Stick: {
                shapePath(vg, control, mid);
                fillAndStroke(vg, nvgRGBA(0, 0, 0, 70), line);
                const Knob& knob = knobs[control.flag];
                nvgBeginPath(vg);
                nvgCircle(vg, mid.x + knob.x, mid.y + knob.y, KNOB_RADIUS);
                fillAndStroke(vg, nvgRGBA(255, 255, 255, down ? 150 : 90), line);
                break;
            }
            case Kind::Dpad: {
                const float arms[4][2] = {{-DPAD_REACH, -DPAD_ARM},
                                          {-DPAD_ARM, -DPAD_REACH},
                                          {DPAD_ARM, -DPAD_ARM},
                                          {-DPAD_ARM, DPAD_ARM}};
                for (int arm = 0; arm < 4; arm++) {
                    bool armDown = pressed & (1u << (DPAD_PRESSED_SHIFT + arm));
                    nvgBeginPath(vg);
                    nvgRoundedRect(vg, mid.x + arms[arm][0], mid.y + arms[arm][1],
                                   DPAD_REACH - DPAD_ARM, DPAD_ARM * 2, 8);
                    fillAndStroke(vg, nvgRGBA(0, 0, 0, armDown ? 190 : 100),
                                  nvgRGBA(255, 255, 255, armDown ? 230 : 140));
                }
                break;
            }
            case Kind::View:
                shapePath(vg, control, mid);
                fillAndStroke(vg, fill, line);
                nvgBeginPath(vg);
                nvgRect(vg, mid.x - 9, mid.y - 9, 12, 12);
                nvgRect(vg, mid.x - 3, mid.y - 3, 12, 12);
                nvgStrokeColor(vg, line);
                nvgStroke(vg);
                break;
            case Kind::Menu:
                shapePath(vg, control, mid);
                fillAndStroke(vg, fill, line);
                nvgBeginPath(vg);
                for (int row = -1; row <= 1; row++) {
                    nvgMoveTo(vg, mid.x - 9, mid.y + row * 6);
                    nvgLineTo(vg, mid.x + 9, mid.y + row * 6);
                }
                nvgStrokeColor(vg, line);
                nvgStroke(vg);
                break;
            case Kind::Button:
            case Kind::Trigger:
                shapePath(vg, control, mid);
                fillAndStroke(vg, fill, line);
                nvgFillColor(vg, line);
                nvgText(vg, mid.x, mid.y, control.label, nullptr);
                break;
        }
    }
}
