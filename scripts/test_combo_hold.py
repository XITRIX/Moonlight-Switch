#!/usr/bin/env python3
"""Check that local combos like the overlay one stay off the host."""

import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

PRELUDE = r"""
#include <cassert>
#include <chrono>
#include <cstdint>
"""

SCENARIOS = r"""
using namespace std::chrono;
int main() {
    constexpr uint32_t minus = 1, plus = 2, a = 4, combo = minus | plus;
    ComboHold hold;
    steady_clock::time_point t;

    // Other buttons pass straight through.
    assert(hold.update(a, combo, t) == a);

    // Both combo buttons inside the window: the host sees neither.
    assert(hold.update(minus, combo, t) == 0);
    assert(hold.update(minus | plus, combo, t += 20ms) == 0);
    assert(hold.update(minus | plus | a, combo, t += 500ms) == a);
    assert(hold.update(minus, combo, t += 20ms) == 0); // still held back
    assert(hold.update(minus, combo, t += 500ms) == 0);
    assert(hold.update(0, combo, t += 20ms) == 0);

    // A lone press reaches the host after the window and stays held.
    assert(hold.update(plus, combo, t += 1s) == 0);
    assert(hold.update(plus, combo, t += 60ms) == plus);
    assert(hold.update(plus | minus, combo, t += 20ms) == 0); // combo after all
    assert(hold.update(0, combo, t += 20ms) == 0);

    // A quick tap is sent as a press, then released on the next frame.
    assert(hold.update(minus, combo, t += 1s) == 0);
    assert(hold.update(0, combo, t += 20ms) == minus);
    assert(hold.update(0, combo, t += 20ms) == 0);
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


def main():
    cxx = shutil.which("c++") or shutil.which("g++")
    if not cxx:
        raise SystemExit("A host C++ compiler is required for combo tests")
    header = (ROOT / "app/src/streaming/InputManager.hpp").read_text()
    source = (ROOT / "app/src/streaming/InputManager.cpp").read_text()
    declaration = method(header, "class ComboHold") + ";\n"
    update = method(source, "uint32_t ComboHold::update(")
    with tempfile.TemporaryDirectory(prefix="moonlight-combo-") as name:
        tmp = Path(name)
        test = tmp / "test.cpp"
        test.write_text(PRELUDE + declaration + update + SCENARIOS)
        binary = tmp / "test.exe"
        subprocess.run(
            [cxx, "-std=c++17", "-Wall", "-Wextra", "-Werror", str(test), "-o", str(binary)],
            check=True,
        )
        subprocess.run([str(binary)], check=True)
    print("Local combos: held back from the host, lone presses after 60 ms, taps passed")


if __name__ == "__main__":
    main()
