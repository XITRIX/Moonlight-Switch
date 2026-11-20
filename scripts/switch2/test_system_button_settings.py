#!/usr/bin/env python3
"""Check Chat availability and stored system-button compatibility on both Switches."""

import re
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    cxx = shutil.which("c++") or shutil.which("g++")
    if not cxx:
        raise SystemExit("A host C++ compiler is required for button settings tests")
    header = (ROOT / "app/src/utils/Settings.hpp").read_text()
    enum = re.search(r"enum class ButtonOverrideType[^;]+;", header)[0]
    source = (ROOT / "app/src/utils/Settings.cpp").read_text()
    parser = source[
        source.index("ButtonOverrideType system_button_type(") : source.index(
            "\n}\n}", source.index("ButtonOverrideType system_button_type(")
        )
        + 2
    ]
    scenarios = (
        r"""
#include <cassert>
#include <climits>
#include <borealis.hpp>
#include "SystemButtonSettings.hpp"
using json_int_t=long long;
"""
        + enum
        + parser
        + r"""
int main(){
    static_assert(static_cast<int>(ButtonOverrideType::NONE)==0);
    static_assert(static_cast<int>(ButtonOverrideType::SCREENSHOT)==1);
    static_assert(static_cast<int>(ButtonOverrideType::HOME)==2);
    static_assert(static_cast<int>(ButtonOverrideType::CHAT)==3);
    const auto options=systemButtonOptions();
    assert(options[0]=="hints/off");
    assert(options[1]=="settings/buttons/screenshot");
    assert(options[2]=="settings/buttons/home");
    assert(system_button_type(0)==ButtonOverrideType::NONE);
    assert(system_button_type(1)==ButtonOverrideType::SCREENSHOT);
    assert(system_button_type(2)==ButtonOverrideType::HOME);
#ifdef __SWITCH2__
    assert(options.size()==4&&options[3]=="settings/buttons/chat");
    assert(system_button_type(3)==ButtonOverrideType::CHAT);
#else
    assert(options.size()==3&&system_button_type(3)==ButtonOverrideType::NONE);
#endif
    assert(system_button_type(-1)==ButtonOverrideType::NONE);
    assert(system_button_type(LLONG_MAX)==ButtonOverrideType::NONE);
}
"""
    )
    with tempfile.TemporaryDirectory(prefix="moonlight-button-settings-") as name:
        tmp = Path(name)
        (tmp / "borealis.hpp").write_text(
            "#pragma once\n#include <string>\nnamespace brls::literals {\n"
            'inline std::string operator""_i18n(const char* s,size_t n){return {s,n};}\n}\n'
        )
        harness = tmp / "test.cpp"
        harness.write_text(scenarios)
        for platform in ("switch1", "switch2"):
            binary = tmp / (platform + ".exe")
            subprocess.run(
                [
                    cxx,
                    "-std=c++17",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    *(["-D__SWITCH2__"] if platform == "switch2" else []),
                    "-I",
                    str(tmp),
                    "-I",
                    str(ROOT / "app/src/utils"),
                    str(harness),
                    "-o",
                    str(binary),
                ],
                check=True,
            )
            subprocess.run([str(binary)], check=True)
    print(
        "System-button settings: stored IDs, platform choices and invalid values passed"
    )


if __name__ == "__main__":
    main()
