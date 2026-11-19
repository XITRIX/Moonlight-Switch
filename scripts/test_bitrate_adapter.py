#!/usr/bin/env python3
"""Drive the opt-in bitrate step-down with a scripted connection status."""

import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

TEST = r"""
#include "BitrateAdapter.hpp"
#include <cassert>

using Change = BitrateAdapter::Change;

static Change run(BitrateAdapter& adapter, int seconds, bool poor) {
    Change last = Change::NONE;
    for (int i = 0; i < seconds && last == Change::NONE; i++)
        last = adapter.update(poor);
    return last;
}

int main() {
    {
        BitrateAdapter adapter;
        assert(run(adapter, 5, true) == Change::NONE); // warm-up isn't judged
        assert(run(adapter, 5, true) == Change::NONE);
        assert(adapter.update(true) == Change::LOWER);
        assert(adapter.scale() == 0.75f);
        // A new connection warms up again and can't lower twice.
        assert(run(adapter, 60, true) == Change::NONE);
        assert(run(adapter, 119, false) == Change::NONE);
        assert(adapter.update(false) == Change::RESTORE);
        assert(adapter.scale() == 1.0f);
        assert(run(adapter, 300, true) == Change::NONE);
        assert(run(adapter, 300, false) == Change::NONE);
    }
    {
        BitrateAdapter adapter;
        // Five poor seconds out of every ten never lower.
        for (int i = 0; i < 600; i++)
            assert(adapter.update(i % 2 == 0) == Change::NONE);
    }
    {
        BitrateAdapter adapter;
        assert(run(adapter, 300, false) == Change::NONE); // nothing to restore
    }
    assert(BitrateAdapter::scaled_bitrate(20000, 1.0f) == 20000);
    assert(BitrateAdapter::scaled_bitrate(20000, 0.75f) == 15000);
    assert(BitrateAdapter::scaled_bitrate(28000, 0.75f) == 21000);
    assert(BitrateAdapter::scaled_bitrate(6000, 0.75f) == 5000);
    assert(BitrateAdapter::scaled_bitrate(4000, 0.75f) == 4000);
}
"""


def main():
    cxx = shutil.which("c++") or shutil.which("g++")
    if not cxx:
        raise SystemExit("A host C++ compiler is required for bitrate tests")
    with tempfile.TemporaryDirectory(prefix="moonlight-bitrate-") as directory:
        tmp = Path(directory)
        (tmp / "test.cpp").write_text(TEST)
        binary = tmp / "bitrate-test"
        subprocess.run(
            [
                cxx,
                "-std=c++17",
                "-Wall",
                "-Wextra",
                "-I" + str(ROOT / "app/src/streaming"),
                str(tmp / "test.cpp"),
                str(ROOT / "app/src/streaming/BitrateAdapter.cpp"),
                "-o",
                str(binary),
            ],
            check=True,
        )
        subprocess.run([str(binary)], check=True)
    print("Bitrate step-down: warm-up, sustained loss, one restore and floor passed")


if __name__ == "__main__":
    main()
