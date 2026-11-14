#!/usr/bin/env python3
"""Check native Switch 2 ABI, library selection and packaged NRO integrity."""

import hashlib
import json
import os
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / os.environ.get("MOONLIGHT_SWITCH2_BUILD_DIR", "build/switch2")


def main():
    sdk = Path(os.environ["DEVKITPRO"])

    def run(tool, *args):
        return subprocess.check_output(
            [
                str(sdk / "devkitA64/bin" / ("aarch64-none-elf-" + tool)),
                *map(str, args),
            ],
            text=True,
        )

    elf = BUILD / "Moonlight.elf"
    nro = BUILD / "Moonlight-Switch.nro"
    header = run("readelf", "-h", elf)
    assert "AArch64" in header and "DYN" in header, header
    assert not run("nm", "-u", elf).strip(), "Unresolved symbols in final ELF"
    data = nro.read_bytes()
    assert data[0x10:0x14] == b"NRO0", "Invalid NRO header"
    commands = json.loads((BUILD / "compile_commands.json").read_text())
    for row in commands:
        command = row.get("command", " ".join(row.get("arguments", [])))
        assert "-mcpu=cortex-a78c+crypto" in command, row["file"]
        assert "-mtp=soft" in command and "-ftls-model=local-exec" in command, row[
            "file"
        ]
        assert "-D__SWITCH2__" in command, row["file"]
        assert "/libnx/" not in command.replace("\\", "/"), row["file"]
        assert "streaming/video/deko3d/" not in row["file"].replace("\\", "/"), row[
            "file"
        ]
    ninja = (BUILD / "build.ninja").read_text()
    assert "lib/switch/libav" not in ninja and "lib/switch/libcurl" not in ninja
    assert "-ldeko3d" not in ninja and "libdeko3d.a" not in ninja
    link_map = (BUILD / "Moonlight.map").read_text()
    assert "libvulkan.a" in link_map or "libouncevk.a" in link_map, (
        "Native OunceVK driver is missing"
    )
    for forbidden in ("libouncevk-gl.a", "libEGL.a", "libGLESv2.a", "libglapi.a"):
        assert forbidden not in link_map, "Unwanted GL dependency: " + forbidden
    assert "/libnx/" not in link_map and "libvulkan_nouveau" not in link_map
    symbols = run("nm", elf)
    assert any("/SDL3/src/" in row["file"].replace("\\", "/") for row in commands), (
        "SDL must build from the fork submodule"
    )
    assert any(
        "/switch/switch2_vulkan.cpp" in row["file"].replace("\\", "/")
        for row in commands
    ), "Borealis must own the native UI backend"
    assert not any(
        name in symbols
        for name in (" zink_", " eglCreate", " glDrawArrays", " glad_gl")
    ), "GL/EGL/Zink renderer leaked into native build"
    assert "nvgCreateVk" in symbols and "vkCreateViSurfaceNN" in symbols
    assert "mbedtls_hardware_poll" in symbols, "Secure entropy adapter was not linked"
    for name in (
        "videoDecoderCreate",
        "videoDecoderSubmit",
        "videoDecoderGetFrame",
        "videoDecoderCopyFrame",
        "videoDecoderReleaseFrame",
        "videoDecoderClose",
    ):
        assert name in symbols, "Missing native decoder SDK symbol: " + name
    sources = [
        "CMakeLists.txt",
        "extern/cmake/commonOption.cmake",
        "extern/cmake/toolchain.cmake",
        "app/src/main.cpp",
        "app/src/streaming/ffmpeg/FFmpegVideoDecoder.cpp",
        "app/src/utils/Settings.hpp",
        "app/src/favorite_tab.cpp",
        "app/src/forwarder_maker.cpp",
        "app/src/settings_tab.cpp",
        "app/src/streaming/MoonlightSession.cpp",
        "app/src/streaming/switch/Switch2VideoDecoder.cpp",
        "app/src/streaming/switch/Switch2VideoDecoder.hpp",
        "app/src/streaming/switch/Switch2NativeFrame.hpp",
        "app/src/streaming/switch/Switch2ImageInterop.hpp",
        "app/src/streaming/switch/Switch2ImageInterop.cpp",
        "app/src/streaming/AVFrameHolder.hpp",
        "app/src/streaming/AVFrameHolder.cpp",
        "app/src/streaming/switch/Switch2DecoderTrace.cpp",
        "app/src/streaming/switch/SwitchMoonlightSessionDecoderAndRenderProvider.cpp",
    ]
    sources += [
        p.relative_to(ROOT).as_posix()
        for p in sorted((ROOT / "app/src/streaming/video/Vulkan").rglob("*"))
        if p.is_file()
    ]
    sources += [
        p.relative_to(ROOT).as_posix()
        for p in sorted(
            (ROOT / "extern/borealis/library/lib/extern/nanovg-vulkan").rglob("*")
        )
        if p.is_file()
    ]
    sources += [
        str(p.relative_to(ROOT)).replace("\\", "/")
        for p in sorted((ROOT / "scripts/switch2").rglob("*"))
        if p.is_file() and "__pycache__" not in str(p)
    ]
    fingerprint = json.loads(
        (ROOT / "build/switch2-generated/source-fingerprint.json").read_text()
    )
    for name, folder in (
        ("borealis", "extern/borealis"),
        ("SDL3", "extern/borealis/library/lib/extern/SDL3"),
    ):
        git = ["git", "-c", "core.autocrlf=true", "-C", str(ROOT / folder)]
        assert (
            subprocess.check_output(git + ["rev-parse", "HEAD"], text=True).strip()
            == fingerprint["dependencies"][name]
        )
        assert not subprocess.check_output(
            git + ["status", "--porcelain", "--ignore-submodules"], text=True
        ).strip(), name + " must be committed"
    sdk_version = next(
        (
            path
            for path in (
                sdk / "ounce-devkit-version.txt",
                sdk / "ounce-sdk-version.txt",
            )
            if path.is_file()
        ),
        None,
    )
    assert sdk_version, "SDK version record is missing"
    record = {
        "platform": "Switch 2",
        "sdk": sdk_version.read_text().strip(),
        "renderer": "Native Vulkan UI and video over OunceVK; no GL, EGL or Zink",
        "decoder": "SDK hardware H.264 leased NV12; AV1 copy-back; HEVC FFmpeg software",
        "nativeVulkanOnly": True,
        "shaderValidation": json.loads(
            (ROOT / "build/switch2-generated/shader-validation.json").read_text()
        ),
        "buildId": fingerprint["buildId"],
        "dependencies": fingerprint["dependencies"],
        "translationUnitsChecked": len(commands),
        "undefinedSymbols": 0,
        "nroBytes": len(data),
        "nroSha256": hashlib.sha256(data).hexdigest(),
        "deviceExecuted": False,
        "streamingVerified": False,
        "sourceSha256": {
            p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in sources
        },
    }
    (BUILD / "validation.json").write_text(json.dumps(record, indent=2) + "\n")
    print(json.dumps(record, indent=2))


if __name__ == "__main__":
    main()
