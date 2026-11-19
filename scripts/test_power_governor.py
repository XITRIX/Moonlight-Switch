#!/usr/bin/env python3
"""Run the battery saver clock plan and governor with host scenarios."""

import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SCENARIOS = r"""
#include "PowerGovernor.hpp"
#include <cassert>

static StreamSecond second(uint64_t arrived, uint64_t presented, uint64_t skipped = 0,
                           uint64_t linkLost = 0) {
    StreamSecond value;
    value.arrived = arrived;
    value.presented = presented;
    value.skipped = skipped;
    value.linkLost = linkLost;
    return value;
}

static StreamMode mode(uint32_t width, uint32_t height, uint32_t fps, bool docked) {
    StreamMode value;
    value.width = width;
    value.height = height;
    value.fps = fps;
    value.docked = docked;
    return value;
}

int main() {
    auto handheld = PowerGovernor::plan(mode(1920, 1080, 60, false));
    assert(handheld[PowerGovernor::CPU] == 714000000 && handheld[PowerGovernor::GPU] == 307000000);
    assert(handheld[PowerGovernor::EMC] == 1331000000);
    auto docked = PowerGovernor::plan(mode(3840, 2160, 60, true));
    assert(docked[PowerGovernor::GPU] == 614000000 && docked[PowerGovernor::EMC] == 3200000000u);
    assert(PowerGovernor::plan(mode(1280, 720, 30, false))[PowerGovernor::GPU] == 230000000);

    assert(PowerGovernor::keepsStock(mode(1920, 1080, 120, true)));
    assert(!PowerGovernor::keepsStock(mode(1920, 1080, 120, false)));
    assert(!PowerGovernor::keepsStock(mode(1920, 1080, 60, true)));

    std::vector<uint32_t> rates = {200, 300, 400, 500};
    assert(PowerGovernor::snap(310, 500, rates) == 400);
    assert(PowerGovernor::snap(300, 500, rates) == 300);
    assert(PowerGovernor::snap(450, 400, rates) == 400);
    assert(PowerGovernor::snap(600, 500, rates) == 500);
    assert(PowerGovernor::snap(310, 500, {}) == 310);
    assert(PowerGovernor::snap(600, 500, {}) == 500);

    assert(!PowerGovernor::behind(second(60, 60), 60, false));
    assert(PowerGovernor::behind(second(60, 50, 10), 60, false));
    assert(PowerGovernor::behind(second(60, 58, 3), 60, false));
    assert(!PowerGovernor::behind(second(60, 58, 2), 60, false));
    assert(!PowerGovernor::behind(second(20, 20), 60, false));
    assert(PowerGovernor::behind(second(40, 40), 60, false));
    assert(!PowerGovernor::behind(second(50, 50), 60, true));
    assert(PowerGovernor::behind(second(60, 50, 10), 60, true));

    PowerGovernor governor;
    assert(!governor.second(second(60, 50, 10), 60));
    assert(!governor.second(second(60, 50, 10), 60));
    assert(governor.second(second(60, 50, 10), 60));
    assert(!governor.second(second(60, 50, 10), 60));

    PowerGovernor reset;
    assert(!reset.second(second(60, 50, 10), 60));
    assert(!reset.second(second(60, 50, 10), 60));
    assert(!reset.second(second(60, 60), 60));
    assert(!reset.second(second(60, 50, 10), 60));
    assert(!reset.second(second(60, 50, 10), 60));
    assert(reset.second(second(60, 50, 10), 60));

    PowerGovernor overlay;
    assert(!overlay.second(second(60, 50, 10), 60));
    assert(!overlay.second(second(60, 50, 10), 60));
    overlay.hold();
    assert(!overlay.second(second(60, 50, 10), 60));
    assert(!overlay.second(second(60, 50, 10), 60));
    assert(overlay.second(second(60, 50, 10), 60));

    PowerGovernor lossy;
    assert(!lossy.second(second(50, 50, 0, 10), 60));
    assert(!lossy.second(second(55, 55, 4), 60));
    assert(!lossy.second(second(55, 55, 4), 60));
    assert(!lossy.second(second(55, 55, 4), 60));
    assert(!lossy.second(second(55, 55, 4), 60));
    assert(lossy.second(second(55, 55, 4), 60));
}
"""


def main():
    cxx = shutil.which("c++") or shutil.which("g++")
    if not cxx:
        raise SystemExit("A host C++ compiler is required for power governor tests")
    with tempfile.TemporaryDirectory() as directory:
        test = Path(directory) / "test.cpp"
        binary = Path(directory) / "test"
        test.write_text(SCENARIOS)
        subprocess.run(
            [cxx, "-std=c++17", "-Wall", "-Wextra", "-Werror",
             "-I", str(ROOT / "app/src/streaming"),
             str(test), str(ROOT / "app/src/streaming/PowerGovernor.cpp"),
             "-o", str(binary)],
            check=True,
        )
        subprocess.run([str(binary)], check=True)
    print("power governor tests passed")


if __name__ == "__main__":
    main()
