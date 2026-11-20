#!/usr/bin/env python3
"""Run Moonlight's scroll sender against a recording host transport."""

import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    cxx = shutil.which("c++") or shutil.which("g++")
    if not cxx:
        raise SystemExit("A host C++ compiler is required for scroll tests")
    source = (ROOT / "app/src/streaming/InputManager.cpp").read_text()
    sender = source[
        source.index("void MoonlightInputManager::sendMouseScroll(") : source.index(
            "void MoonlightInputManager::sendRelativeMouseMove("
        )
    ]
    model = r"""
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <limits>
#include <vector>
namespace brls {
struct Point {float x=0,y=0;};
struct Logger {
    template<class... T> static void warning(T...){}
    template<class... T> static void info(T...){}
};
}
struct Settings {
    float sensitivity=1;
    static Settings& instance(){static Settings value;return value;}
    float get_mouse_scroll_multiplier(){return sensitivity;}
};
constexpr float MOONLIGHT_WHEEL_DELTA=120;
static std::vector<short> horizontal,vertical;
int LiSendHighResHScrollEvent(short value){horizontal.push_back(value);return 0;}
int LiSendHighResScrollEvent(short value){vertical.push_back(value);return 0;}
struct MoonlightInputManager {
    brls::Point desktopScrollRemainder;
    brls::Point scrollLogInput;
    int64_t scrollLogSentX=0,scrollLogSentY=0;
    unsigned scrollLogEvents=0;
    float loggedScrollSensitivity=-1;
    std::chrono::steady_clock::time_point lastScrollLog;
    void sendMouseScroll(brls::Point);
};
"""
    scenarios = r"""
int main(){
    MoonlightInputManager input;
    input.sendMouseScroll({1,-1});
    assert(horizontal.back()==120&&vertical.back()==-120);
    Settings::instance().sensitivity=0.5f;
    input.sendMouseScroll({-1,1});
#ifdef __SWITCH2__
    assert(horizontal.back()==120&&vertical.back()==-120); // Half a click is held.
    input.sendMouseScroll({-1,1});
    assert(horizontal.back()==-120&&vertical.back()==120);
#else
    assert(horizontal.back()==-60&&vertical.back()==60);
#endif
    Settings::instance().sensitivity=2;
    input.sendMouseScroll({0,1});
    assert(vertical.back()==240);
    Settings::instance().sensitivity=0.1f;
    vertical.clear();
    for(int i=0;i<10;++i)input.sendMouseScroll({0,0.01f});
    int sum=0;for(short value:vertical)sum+=value;
#ifdef __SWITCH2__
    assert(sum==0&&input.desktopScrollRemainder.y>1); // Hold sub-click motion.
#else
    assert(sum==1); // Tiny low-sensitivity samples must accumulate, not vanish.
#endif
    input.sendMouseScroll({0,-0.1f});
    sum=0;for(short value:vertical)sum+=value;
    assert(sum==0); // Reversing a gesture also cancels its fractional remainder.
    input.desktopScrollRemainder={};
    vertical.clear();
    for(int i=0;i<120;++i)input.sendMouseScroll({0,-6.0f/120.0f});
    sum=0;for(short value:vertical)sum+=value;
#ifdef __SWITCH2__
    assert(sum==0&&std::abs(input.desktopScrollRemainder.y+72)<0.01f);
    for(int i=0;i<120;++i)input.sendMouseScroll({0,-6.0f/120.0f});
    sum=0;for(short value:vertical)sum+=value;
    assert(sum==-120&&vertical.size()==1); // One full step, never a flood of tiny packets.
    input.desktopScrollRemainder={};vertical.clear();
    Settings::instance().sensitivity=1;
    for(int i=0;i<240;++i)input.sendMouseScroll({0,-6.0f/120.0f});
    sum=0;for(short value:vertical)sum+=value;
    assert(sum==-1440&&vertical.size()==12); // x1.00 sends ten times the amount.
#else
    assert(std::abs(sum+72)<=1); // At x0.10, 720 native units queue 72, not 8640.
#endif
    input.desktopScrollRemainder={};
    Settings::instance().sensitivity=2;
    input.sendMouseScroll({10000,-10000});
#ifdef __SWITCH2__
    assert(horizontal.back()==32760&&vertical.back()==-32760);
#else
    assert(horizontal.back()==std::numeric_limits<short>::max());
    assert(vertical.back()==std::numeric_limits<short>::min());
#endif
}
"""
    with tempfile.TemporaryDirectory(prefix="moonlight-scroll-") as name:
        tmp = Path(name)
        harness = tmp / "test.cpp"
        harness.write_text(model + sender + scenarios)
        for platform in ("desktop", "switch2"):
            binary = tmp / (platform + ".exe")
            subprocess.run(
                [
                    cxx,
                    "-std=c++17",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    *(["-D__SWITCH2__"] if platform == "switch2" else []),
                    str(harness),
                    "-o",
                    str(binary),
                ],
                check=True,
            )
            subprocess.run([str(binary)], check=True)
    print(
        "Scroll sensitivity: default, live changes, fractions, reversal and bounds passed"
    )


if __name__ == "__main__":
    main()
