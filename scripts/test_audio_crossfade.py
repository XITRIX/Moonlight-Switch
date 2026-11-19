#!/usr/bin/env python3
"""Check the fade that hides dropped audio packets."""

import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

TEST = r"""
#include "AudioCrossfade.hpp"
#include <cassert>
#include <cstdlib>

int main() {
    {
        // Stereo, 3 frames: weights 1/4, 2/4, 3/4 from `from` to `to`.
        const int16_t from[] = {0, 1000, 0, 1000, 0, 1000};
        int16_t to[] = {400, -1000, 400, -1000, 400, -1000, 7, 7};
        crossfadeAudio(from, to, 3, 2);
        const int16_t expected[] = {100, 500, 200, 0, 300, -500, 7, 7};
        for (int i = 0; i < 8; i++)
            assert(to[i] == expected[i]);
    }
    {
        // Equal audio passes through unchanged, extremes don't wrap.
        int16_t from[144], to[144];
        for (int i = 0; i < 144; i++)
            from[i] = to[i] = i % 2 ? 32767 : -32768;
        crossfadeAudio(from, to, 72, 2);
        for (int i = 0; i < 144; i++)
            assert(to[i] == from[i]);
    }
    {
        // The last frame is almost all new audio, the first almost all old.
        int16_t from[72], to[72];
        for (int i = 0; i < 72; i++) {
            from[i] = 10000;
            to[i] = -10000;
        }
        crossfadeAudio(from, to, 72, 1);
        assert(to[0] > 9000);
        assert(to[71] < -9000);
        for (int i = 1; i < 72; i++)
            assert(to[i] < to[i - 1]);
    }
    return 0;
}
"""


def main():
    cxx = shutil.which("c++") or shutil.which("g++")
    if not cxx:
        raise SystemExit("A host C++ compiler is required for audio tests")
    with tempfile.TemporaryDirectory(prefix="moonlight-crossfade-") as directory:
        tmp = Path(directory)
        (tmp / "test.cpp").write_text(TEST)
        binary = tmp / "crossfade-test"
        subprocess.run(
            [
                cxx,
                "-std=c++17",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I" + str(ROOT / "app/src/streaming/audio"),
                str(tmp / "test.cpp"),
                "-o",
                str(binary),
            ],
            check=True,
        )
        subprocess.run([str(binary)], check=True)
    print("Audio crossfade: blend, pass-through and direction passed")


if __name__ == "__main__":
    main()
