#!/usr/bin/env python3
"""Compile and validate the native video shaders; validate vendored UI SPIR-V."""

import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
from pathlib import Path
from sdk_identity import sdk_identity

ROOT = Path(__file__).resolve().parents[2]


def write_if_changed(path, content):
    if not path.is_file() or path.read_text() != content:
        path.write_text(content)


def main():
    compiler = shutil.which("glslangValidator")
    validator = shutil.which("spirv-val")
    if not compiler or not validator:
        raise SystemExit(
            "glslangValidator and spirv-val are required for native Vulkan shaders"
        )
    output = ROOT / "build/switch2-generated"
    output.mkdir(exist_ok=True)
    header = "#pragma once\n#include <cstdint>\n"
    record = {}
    for name, stage in (("vert", "vert"), ("frag", "frag"), ("texel_frag", "frag")):
        source = ROOT / ("app/src/streaming/video/Vulkan/video." + stage)
        binary = output / ("video." + name + ".spv")
        defines = ["-DTEXEL_BUFFER_SAMPLING=1"] if name == "texel_frag" else []
        subprocess.run(
            [
                compiler,
                "-V",
                "--target-env",
                "vulkan1.1",
                *defines,
                str(source),
                "-o",
                str(binary),
            ],
            check=True,
        )
        subprocess.run(
            [validator, "--target-env", "vulkan1.1", str(binary)], check=True
        )
        data = binary.read_bytes()
        words = struct.unpack("<" + "I" * (len(data) // 4), data)
        header += "alignas(4) static constexpr uint32_t video_" + name + "[] = {\n"
        for i in range(0, len(words), 8):
            header += ",".join(hex(v) for v in words[i : i + 8]) + ",\n"
        header += "};\n"
        key = source.relative_to(ROOT).as_posix() + (
            "#texel" if name == "texel_frag" else ""
        )
        record[key] = hashlib.sha256(data).hexdigest()
    for stage in ("vert", "frag"):
        source = ROOT / (
            "extern/borealis/library/lib/extern/nanovg-vulkan/src/shader/fill."
            + stage
            + ".inc"
        )
        words = [int(v, 16) for v in re.findall(r"0x[0-9a-fA-F]+", source.read_text())]
        binary = output / ("ui." + stage + ".spv")
        data = struct.pack("<" + "I" * len(words), *words)
        binary.write_bytes(data)
        subprocess.run(
            [validator, "--target-env", "vulkan1.1", str(binary)], check=True
        )
        record[source.relative_to(ROOT).as_posix()] = hashlib.sha256(data).hexdigest()
    write_if_changed(output / "video_spirv.hpp", header)
    write_if_changed(output / "shader-validation.json", json.dumps(record, indent=2) + "\n")
    fingerprint = hashlib.sha256()
    inputs = [ROOT / "CMakeLists.txt", ROOT / "extern/cmake/toolchain.cmake"]
    for folder in (
        "app/src",
        "scripts/switch2",
        "extern/borealis/library/lib/extern/nanovg-vulkan",
    ):
        inputs += [
            p
            for p in (ROOT / folder).rglob("*")
            if p.is_file() and "__pycache__" not in p.parts
        ]
    for folder in (
        "extern/borealis/library/include/borealis/platforms/switch",
        "extern/borealis/library/lib/platforms/switch",
    ):
        inputs += [p for p in (ROOT / folder).rglob("*") if p.is_file()]
    inputs.append(
        ROOT / "extern/borealis/library/include/borealis/core/frame_resources.hpp"
    )
    inputs.append(
        ROOT / "extern/borealis/library/include/borealis/core/frame_presentation.hpp"
    )
    for path in sorted(inputs):
        fingerprint.update(path.relative_to(ROOT).as_posix().encode())
        fingerprint.update(path.read_bytes())
    dependencies = {}
    for name, folder in (
        ("borealis", "extern/borealis"),
        ("SDL3", "extern/borealis/library/lib/extern/SDL3"),
    ):
        # Native Windows Git and MSYS Git must agree on CRLF checkouts.
        git = ["git", "-c", "core.autocrlf=true", "-C", str(ROOT / folder)]
        revision = subprocess.check_output(
            git + ["rev-parse", "HEAD"], text=True
        ).strip()
        dependencies[name] = revision
        fingerprint.update(name.encode() + revision.encode())
        fingerprint.update(subprocess.check_output(git + ["diff", "HEAD", "--"]))
    sdk_root = Path(os.environ["DEVKITPRO"])
    sdk_version = sdk_identity(sdk_root)
    fingerprint.update(b"sdk\0" + sdk_version.encode())
    build_id = fingerprint.hexdigest()[:20]
    write_if_changed(output / "switch2_build_id.hpp",
        '#define MOONLIGHT_SWITCH2_BUILD_ID "' + build_id + '"\n'
    )
    write_if_changed(output / "source-fingerprint.json",
        json.dumps(
            {"buildId": build_id, "dependencies": dependencies, "sdk": sdk_version},
            indent=2,
        )
        + "\n"
    )
    print("Native Vulkan video shaders compiled; video and UI SPIR-V validation passed")


if __name__ == "__main__":
    main()
