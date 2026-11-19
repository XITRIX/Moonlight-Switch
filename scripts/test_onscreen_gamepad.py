#!/usr/bin/env python3
"""Run the on-screen gamepad's touch handling with host stubs for borealis and nanovg."""

import re
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

PRELUDE = r"""
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <vector>
namespace brls {
struct Point {float x=0,y=0;};
enum class TouchPhase {START,STAY,END,NONE};
struct TouchState {int fingerId=0;TouchPhase phase=TouchPhase::NONE;Point position;};
enum {FONT_REGULAR};
struct Application {static int getFont(int){return 0;}};
}
struct NVGcontext {};
struct NVGcolor {};
enum {NVG_ALIGN_CENTER=1,NVG_ALIGN_MIDDLE=2};
NVGcolor nvgRGBA(int,int,int,int){return {};}
template<class... T> void nvgBeginPath(T...){}
template<class... T> void nvgCircle(T...){}
template<class... T> void nvgRoundedRect(T...){}
template<class... T> void nvgRect(T...){}
template<class... T> void nvgMoveTo(T...){}
template<class... T> void nvgLineTo(T...){}
template<class... T> void nvgFillColor(T...){}
template<class... T> void nvgFill(T...){}
template<class... T> void nvgStrokeColor(T...){}
template<class... T> void nvgStrokeWidth(T...){}
template<class... T> void nvgStroke(T...){}
template<class... T> void nvgFontFaceId(T...){}
template<class... T> void nvgFontSize(T...){}
template<class... T> void nvgFontBlur(T...){}
template<class... T> void nvgTextAlign(T...){}
template<class... T> void nvgText(T...){}
"""

SCENARIOS = r"""
using brls::TouchPhase;
static std::vector<brls::TouchState> touches;
static void finger(int id,TouchPhase phase,float x,float y){
    touches.push_back({id,phase,{x,y}});
}
static GamepadState frame(OnscreenGamepad& pad){
    pad.update(touches,1280);touches.clear();return pad.state();
}
int main(){
    OnscreenGamepad pad;
    const float faceX=1280-150;

    // A tap on A presses it, lifts on release and keeps the finger off the host meanwhile.
    finger(1,TouchPhase::START,faceX,402);
    assert(frame(pad).buttonFlags==(short)A_FLAG&&pad.touches().count(1));
    finger(1,TouchPhase::END,faceX,402);
    assert(frame(pad).buttonFlags==0&&pad.touches().count(1)&&!pad.holding());
    frame(pad);assert(pad.touches().empty());

    // A finger that starts off the controls is not the pad's, even if it moves onto one.
    finger(2,TouchPhase::START,640,360);
    assert(frame(pad).buttonFlags==0&&pad.touches().empty());
    finger(2,TouchPhase::STAY,faceX,402);
    assert(frame(pad).buttonFlags==0&&pad.touches().empty());

    // A stick follows its finger past the ring and reads y up.
    finger(3,TouchPhase::START,150,340);
    assert(frame(pad).leftStickX==0);
    finger(3,TouchPhase::STAY,210,340);
    auto state=frame(pad);assert(state.leftStickX==32767&&state.leftStickY==0);
    finger(3,TouchPhase::STAY,150,100);
    state=frame(pad);assert(state.leftStickX==0&&state.leftStickY==32767);
    finger(3,TouchPhase::STAY,153,340);
    assert(frame(pad).leftStickX==0); // inside the deadzone
    finger(3,TouchPhase::END,153,340);frame(pad);

    // Sliding off a button lets go of it, but the finger stays with the pad.
    finger(4,TouchPhase::START,1280-88,340);
    assert(frame(pad).buttonFlags==(short)B_FLAG);
    finger(4,TouchPhase::STAY,640,360);
    assert(frame(pad).buttonFlags==0&&pad.holding());
    finger(4,TouchPhase::END,640,360);frame(pad);

    // The D-pad has eight directions and a dead centre.
    finger(5,TouchPhase::START,270,560);
    assert(frame(pad).buttonFlags==0);
    finger(5,TouchPhase::STAY,330,560);
    assert(frame(pad).buttonFlags==(short)RIGHT_FLAG);
    finger(5,TouchPhase::STAY,320,610);
    assert(frame(pad).buttonFlags==(short)(RIGHT_FLAG|DOWN_FLAG));

    // Triggers are full or nothing, and several fingers combine.
    finger(5,TouchPhase::STAY,320,610);
    finger(6,TouchPhase::START,1280-115,67);
    state=frame(pad);
    assert(state.rightTrigger==0xFF&&state.buttonFlags==(short)(RIGHT_FLAG|DOWN_FLAG));

    pad.release();
    assert(!pad.holding()&&pad.touches().empty()&&pad.state().buttonFlags==0);
    assert(pad.hit({faceX,402},1280)&&!pad.hit({640,360},1280));
    pad.draw(nullptr,1280);
}
"""


def method(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def without_includes(text):
    return re.sub(r"^#(?:include|pragma)[^\n]*\n", "", text, flags=re.M)


def main():
    cxx = shutil.which("c++") or shutil.which("g++")
    if not cxx:
        raise SystemExit("A host C++ compiler is required for on-screen gamepad tests")
    limelight = (ROOT / "extern/moonlight-common-c/src/Limelight.h").read_text()
    flags = "\n".join(re.findall(r"^#define \w+_FLAG\s+0x[0-9A-Fa-f]+$", limelight, flags=re.M))
    input_header = (ROOT / "app/src/streaming/InputManager.hpp").read_text()
    code = PRELUDE + flags + "\n"
    code += method(input_header, "struct GamepadState") + ";\n"
    code += without_includes((ROOT / "app/include/onscreen_gamepad.hpp").read_text())
    code += without_includes((ROOT / "app/src/onscreen_gamepad.cpp").read_text())
    code += SCENARIOS
    with tempfile.TemporaryDirectory(prefix="moonlight-onscreen-pad-") as name:
        tmp = Path(name)
        test = tmp / "test.cpp"
        test.write_text(code)
        binary = tmp / "test.exe"
        subprocess.run(
            [cxx, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter",
             str(test), "-o", str(binary)],
            check=True,
        )
        subprocess.run([str(binary)], check=True)
    print("On-screen gamepad: fingers keep their control, sticks, D-pad, triggers and taps")


if __name__ == "__main__":
    main()
