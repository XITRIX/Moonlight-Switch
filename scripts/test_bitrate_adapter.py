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
using Second = BitrateAdapter::Second;

static Second clean() {
    Second second;
    second.bitrate = 20000;
    second.rtt_ms = 5;
    return second;
}

static Second poor() {
    Second second = clean();
    second.poor = true;
    return second;
}

static Change run(BitrateAdapter& adapter, int seconds, const Second& second) {
    Change last = Change::NONE;
    for (int i = 0; i < seconds && last == Change::NONE; i++)
        last = adapter.update(second);
    return last;
}

// Six of ten judged seconds must be poor, after the 5 s warm-up.
static void lowers_on(void (*bad)(Second&, int)) {
    BitrateAdapter adapter;
    Change last = Change::NONE;
    for (int i = 0; i < 11 && last == Change::NONE; i++) {
        Second second = clean();
        bad(second, i);
        last = adapter.update(second);
        assert(i == 10 || last == Change::NONE);
    }
    assert(last == Change::LOWER);
}

int main() {
    {
        BitrateAdapter adapter;
        assert(run(adapter, 5, poor()) == Change::NONE); // warm-up isn't judged
        assert(run(adapter, 5, poor()) == Change::NONE);
        assert(adapter.update(poor()) == Change::LOWER);
        assert(adapter.scale() == 0.75f);
        // A new connection warms up again and can't lower twice.
        assert(run(adapter, 60, poor()) == Change::NONE);
        assert(run(adapter, 119, clean()) == Change::NONE);
        assert(adapter.update(clean()) == Change::RESTORE);
        assert(adapter.scale() == 1.0f);
        assert(run(adapter, 300, poor()) == Change::NONE);
        assert(run(adapter, 300, clean()) == Change::NONE);
    }
    {
        BitrateAdapter adapter;
        // Five poor seconds out of every ten never lower.
        for (int i = 0; i < 600; i++)
            assert(adapter.update(i % 2 == 0 ? poor() : clean()) == Change::NONE);
    }
    {
        BitrateAdapter adapter;
        assert(run(adapter, 300, clean()) == Change::NONE); // nothing to restore
    }

    // Each signal alone makes a second poor.
    lowers_on([](Second& s, int i) { s.dropped_frames = i * 5; });
    lowers_on([](Second& s, int i) { s.idr_requests = i; });
    lowers_on([](Second& s, int) { s.rtt_ms = 81; });
    lowers_on([](Second& s, int) { s.pending_frames = 4; });
    {
        // Below every threshold the connection is clean.
        BitrateAdapter adapter;
        for (int i = 0; i < 300; i++) {
            Second second = clean();
            second.dropped_frames = i * 4;
            second.rtt_ms = 80;
            second.pending_frames = 3;
            assert(adapter.update(second) == Change::NONE);
        }
    }
    {
        // Counters that restart with a new decoder aren't a burst of loss.
        BitrateAdapter adapter;
        Second second = clean();
        second.dropped_frames = 1000;
        second.idr_requests = 10;
        assert(run(adapter, 20, second) == Change::NONE);
        second.dropped_frames = 2;
        second.idr_requests = 0;
        assert(run(adapter, 300, second) == Change::NONE);
    }
    {
        // A poor second resets the clean stretch before a restore.
        BitrateAdapter adapter;
        assert(run(adapter, 11, poor()) == Change::LOWER);
        assert(run(adapter, 100, clean()) == Change::NONE);
        assert(adapter.update(poor()) == Change::NONE);
        assert(run(adapter, 119, clean()) == Change::NONE);
        assert(adapter.update(clean()) == Change::RESTORE);
    }
    {
        // Lowering at the floor would reconnect at the same rate.
        BitrateAdapter adapter;
        Second second = poor();
        second.bitrate = 5000;
        assert(run(adapter, 300, second) == Change::NONE);
        second.bitrate = 5100;
        assert(adapter.update(second) == Change::LOWER);
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
    print("Bitrate step-down: warm-up, loss signals, one restore and floor passed")


if __name__ == "__main__":
    main()
