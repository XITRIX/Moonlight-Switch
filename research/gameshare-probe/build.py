#!/usr/bin/env python3
"""Build the native Switch 2 diagnostic with the existing devkitPro/libounce SDK."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import struct
import zipfile
from toolchain import default_sdk
from qualify import from_build, emit

ROOT = Path(__file__).resolve().parent


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, default=default_sdk())
    parser.add_argument("--jit-provider", type=Path, help="Optional prebuilt kaiHEN JIT hook object; otherwise build the bundled ISC source")
    args = parser.parse_args()
    sdk = args.sdk.resolve()
    suffix = ".exe" if os.name == "nt" else ""
    gcc = sdk / "devkitA64/bin" / ("aarch64-none-elf-gcc" + suffix)
    tools = sdk / "tools/bin"
    build = ROOT / "build"
    dist = ROOT / "dist"
    build.mkdir(exist_ok=True)
    dist.mkdir(exist_ok=True)
    env = dict(os.environ, DEVKITPRO=sdk.as_posix())
    env["PATH"] = str(gcc.parent) + os.pathsep + str(tools) + os.pathsep + env["PATH"]
    cpu_file = sdk / "libounce/switch2_cpu.mk"
    match = re.search(r"^SWITCH2_CPU\s*:=\s*(.+)$", cpu_file.read_text(), re.MULTILINE)
    if not match:
        raise SystemExit("SDK has no SWITCH2_CPU flags")
    flags = [*shlex.split(match.group(1)), "-mtp=soft", "-fPIE",
             "-std=gnu11", "-O2", "-g", "-Wall", "-Wextra", "-Werror",
             "-ffunction-sections", "-fdata-sections", "-D__SWITCH__", "-D__SWITCH2__",
             "-I" + (sdk / "libounce/include").as_posix()]
    records = []

    def run(command):
        subprocess.run([str(x) for x in command], cwd=ROOT, env=env, check=True)
        records.append([str(x) for x in command])

    objects = []
    for source in sorted((ROOT / "source").glob("*.c")):
        obj = build / (source.stem + ".o")
        run([gcc, *flags, "-c", source, "-o", obj])
        objects.append(obj)
    elf = build / "gameshare-probe.elf"
    jit_source = ROOT / "research-backend/kaihen/libkaihen/source/jit.c"
    jit_provider = args.jit_provider.resolve() if args.jit_provider else build / "kaihen-jit.o"
    if not args.jit_provider:
        run([gcc, *flags, "-I" + str(ROOT / "research-backend/kaihen/libkaihen/include"),
             "-c", jit_source, "-o", jit_provider])
    elif not jit_provider.is_file():
        raise SystemExit("Explicit JIT provider object does not exist")
    run([gcc, *flags, "-specs=" + (sdk / "libounce/switch.specs").as_posix(),
         "-Wl,--gc-sections", "-Wl,-Map," + (build / "gameshare-probe.map").as_posix(),
         *objects, jit_provider, "-L" + (sdk / "libounce/lib").as_posix(), "-lnx", "-lm", "-o", elf])
    nacp = build / "gameshare-probe.nacp"
    version = re.search(r'^#define VERSION "([^"]+)"$', (ROOT / "source/main.c").read_text(), re.MULTILINE).group(1)
    run([tools / ("nacptool" + suffix), "--create", "GameShare Probe", "XITRIX", version, nacp])
    nro = dist / "gameshare-probe.nro"
    run([tools / ("elf2nro" + suffix), elf, nro, "--nacp=" + str(nacp),
         "--icon=" + str(sdk / "libounce/default_icon.jpg")])
    data = nro.read_bytes()
    if data[0x10:0x14] != b"NRO0":
        raise SystemExit("Generated output is not an NRO")
    image_size = struct.unpack_from("<I", data, 0x18)[0]
    if image_size > len(data) or data[image_size:image_size+4] != b"ASET":
        raise SystemExit("Generated NRO has invalid image/asset boundaries")
    record = {"schema": "gameshare-probe-build-v1", "version": version, "nro": nro.name,
              "nro_sha256": sha256(nro), "nro_bytes": len(data), "elf_sha256": sha256(elf),
              "compiler": subprocess.check_output([gcc, "--version"], env=env, text=True).splitlines()[0],
              "sdk": str(sdk), "sdk_library_sha256": sha256(sdk / "libounce/lib/libnx.a"),
              "sdk_cpu_flags_sha256": sha256(cpu_file),
              "jit_provider_sha256": sha256(jit_provider),
              "jit_provider_source_sha256": sha256(jit_source) if not args.jit_provider else None,
              "source_sha256": {str(p.relative_to(ROOT)): sha256(p) for p in
                                sorted((ROOT / "source").glob("*.[ch]"))},
              "commands": records, "device_executed": False}
    (dist / "build.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    qualified = build / "qualified"
    emit(from_build(elf, nro, sdk), qualified, qualified / "own_vi2_symbols.h")
    with zipfile.ZipFile(dist / "gameshare-probe.zip", "w", zipfile.ZIP_DEFLATED) as package:
        package.write(nro, "switch/gameshare-probe/gameshare-probe.nro")
        entries = list(ROOT.glob("*.py")) + list(ROOT.glob("*.md")) + [ROOT/"payload.ld", ROOT/"payload_runtime.c", ROOT/".gitignore"]
        for folder in ("source", "examples", "tests", "records", "research-backend", "archive", "notes"):
            entries.extend(p for p in (ROOT/folder).rglob("*") if p.is_file() and "__pycache__" not in p.parts)
        for entry in sorted(entries):
            if entry == ROOT / "examples/own_vi2_symbols.h":
                continue
            package.write(entry, entry.relative_to(ROOT).as_posix())
        package.write(qualified / "own_vi2_symbols.h", "examples/own_vi2_symbols.h")
        package.write(qualified / "probe-layout.json", "records/packaged-probe-layout.json")
        package.write(dist / "build.json", "build.json")
    print(f"Built {nro} ({len(data):,} bytes)\nSHA-256 {record['nro_sha256']}")


if __name__ == "__main__":
    main()
