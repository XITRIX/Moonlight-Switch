#!/usr/bin/env python3
"""Host regression for HID taps reaching rendered UI buttons at 1080p."""

import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    cxx = shutil.which("c++") or shutil.which("g++")
    if not cxx:
        raise SystemExit("A host C++ compiler is required for touch regression tests")
    with tempfile.TemporaryDirectory(prefix="moonlight-touch-") as name:
        tmp = Path(name)
        source = tmp / "test.cpp"
        source.write_text("""#include <cassert>
#include <cmath>
#include <borealis/platforms/switch/switch_touch_coordinates.hpp>
using brls::switchTouchCoordinate;
bool buttonHit(float x, float y, float left, float top, float width, float height) {
    return x >= left && x <= left+width && y >= top && y <= top+height;
}
int main() {
    // This lower-right button was missed by the old 1.5x division.
    float x = switchTouchCoordinate(1000, 1280, 1920, 1.5f);
    float y = switchTouchCoordinate(640, 720, 1080, 1.5f);
    assert(buttonHit(x,y,900,600,200,80));
    assert(!buttonHit(1000/1.5f,640/1.5f,900,600,200,80));
    // A capped UI uses physical-pixel layout; do not assume logical == HID.
    x = switchTouchCoordinate(1000,1280,1920,1);
    y = switchTouchCoordinate(640,720,1080,1);
    assert(buttonHit(x,y,1350,900,300,120));
    // Center, origin, far edge and fractional taps stay aligned.
    assert(switchTouchCoordinate(640,1280,1920,1.5f)==640);
    assert(switchTouchCoordinate(360,720,1080,1.5f)==360);
    assert(switchTouchCoordinate(0,1280,1920,1.5f)==0);
    assert(switchTouchCoordinate(1279,1280,1920,1.5f)==1279);
    assert(switchTouchCoordinate(719,720,1080,1.5f)==719);
    assert(std::fabs(switchTouchCoordinate(321.25f,1280,1920,1.5f)-321.25f)<0.001f);
    // Docked framebuffer changes preserve logical button hit positions.
    assert(switchTouchCoordinate(1000,1280,2560,2)==1000);
    assert(switchTouchCoordinate(640,720,1440,2)==640);
    assert(switchTouchCoordinate(1000,1280,3840,3)==1000);
    assert(switchTouchCoordinate(640,720,2160,3)==640);
    // Original-resolution mapping remains valid.
    assert(switchTouchCoordinate(1000,1280,1280,1)==1000);
    assert(switchTouchCoordinate(640,720,720,1)==640);
}
""")
        exe = tmp / "touch-test.exe"
        subprocess.run(
            [
                cxx,
                "-std=c++17",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I",
                str(ROOT / "extern/borealis/library/include"),
                str(source),
                "-o",
                str(exe),
            ],
            check=True,
        )
        subprocess.run([str(exe)], check=True)
    print("Touch regression: button hit targets, capped scale, center and edges passed")


if __name__ == "__main__":
    main()
