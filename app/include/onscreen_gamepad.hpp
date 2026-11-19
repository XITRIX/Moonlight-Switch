//
//  onscreen_gamepad.hpp
//  Moonlight
//

#pragma once

#include "InputManager.hpp"
#include <borealis.hpp>
#include <map>
#include <set>

// Touch controls drawn over the stream, sent to the host as an extra Xbox
// controller. A finger belongs to the control it went down on until it lifts;
// fingers that start anywhere else are left to the stream.
class OnscreenGamepad {
  public:
    void update(const std::vector<brls::TouchState>& touches, float width);
    void release();
    void draw(NVGcontext* vg, float width);

    bool hit(brls::Point point, float width) const;
    bool holding() const { return !fingers.empty(); }
    const std::set<int>& touches() const { return ownedTouches; }
    GamepadState state() const { return current; }

  private:
    struct Knob {
        float x = 0;
        float y = 0;
    };

    std::map<int, int> fingers;
    std::set<int> ownedTouches;
    GamepadState current;
    Knob knobs[2];
    uint32_t pressed = 0;
};
