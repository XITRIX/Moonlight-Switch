#!/usr/bin/env python3
"""Generate guarded NRO-specific inputs; never contacts or modifies the console."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
from toolchain import default_sdk, executable

ROOT = Path(__file__).resolve().parent
VI2 = ["openService", "vi2OpenDisplay", "vi2CloseDisplay", "vi2CreateEndpoint",
       "vi2CloseEndpoint", "vi2CreateLayer", "vi2AttachLayer", "vi2SelectLayer",
       "vi2ClearLayerSelection", "vi2DetachLayer", "vi2CloseLayer"]


def elf_code(data, address, count=16):
    if len(data) < 64 or data[:6] != b"\x7fELF\x02\x01" or struct.unpack_from("<H", data, 18)[0] != 183:
        raise ValueError("Expected little-endian AArch64 ELF64")
    offset = struct.unpack_from("<Q", data, 32)[0]
    width, count_headers = struct.unpack_from("<HH", data, 54)
    if width < 56 or offset + width * count_headers > len(data):
        raise ValueError("Truncated ELF program headers")
    for index in range(count_headers):
        kind, flags, file_offset, virtual, _, size, _, _ = struct.unpack_from("<IIQQQQQQ", data, offset + index * width)
        if kind == 1 and flags & 1 and virtual <= address and address + count <= virtual + size:
            start = file_offset + address - virtual
            if start + count > len(data):
                raise ValueError("Truncated executable segment")
            return data[start:start + count]
    raise ValueError(f"Function 0x{address:x} is outside an executable segment")


def from_build(elf_path, nro_path, sdk):
    elf, nro = elf_path.read_bytes(), nro_path.read_bytes()
    if nro[16:20] != b"NRO0":
        raise ValueError("Expected NRO0 image")
    symbols = {}
    command = [str(executable(sdk, "devkitA64/bin/aarch64-none-elf-nm")), "--defined-only", str(elf_path)]
    for line in subprocess.check_output(command, text=True).splitlines():
        row = line.split()
        if len(row) == 3:
            symbols[row[2]] = int(row[0], 16)
    names = VI2 + ["smGetServiceOverride", "private_sm", "fopen", "fread", "fclose"]
    result = {}
    for name in names:
        address = symbols[name]
        entry = {"offset": address}
        if name != "private_sm":
            code = elf_code(elf, address)
            if code != nro[address:address + 16]:
                raise ValueError(f"ELF/NRO code mismatch for {name}; refusing a profile")
            entry["first_bytes"] = code.hex()
        result[name] = entry
    return {"schema": "gameshare-probe-layout-v1", "nro_sha256": hashlib.sha256(nro).hexdigest(),
            "elf_sha256": hashlib.sha256(elf).hexdigest(), "symbols": result}


def emit(profile, output, header):
    if profile.get("schema") != "gameshare-probe-layout-v1" or len(profile.get("nro_sha256", "")) != 64:
        raise ValueError("Unexpected qualification profile")
    symbols = profile["symbols"]
    def offset(name):
        value = symbols[name]["offset"]
        if not isinstance(value, int) or not 0 <= value < 2**32:
            raise ValueError("Invalid symbol offset")
        return value
    def code(name):
        value = bytes.fromhex(symbols[name]["first_bytes"])
        if len(value) != 16:
            raise ValueError("Expected a 16-byte function guard")
        return value
    output.mkdir(parents=True, exist_ok=True)
    anchor, sm = offset("openService"), offset("private_sm")
    for transport, name in [(0, "cmif"), (1, "tipc"), (2, "versioned")]:
        (output / f"agent-layout-{name}.bin").write_bytes(struct.pack("<QQ16sII", anchor, sm, code("openService"), transport, 0))
    (output / "agent-layout-hen.bin").write_bytes(struct.pack("<QQ16sIIQ16s", anchor, sm, code("openService"),
        0, 0, offset("smGetServiceOverride"), code("smGetServiceOverride")))
    file_names = ["fopen", "fread", "fclose"]
    backup = struct.pack("<Q16s3Q", anchor, code("openService"), *(offset(name) for name in file_names))
    backup += b"".join(code(name) for name in file_names)
    (output / "hen-backup-layout.bin").write_bytes(backup)
    (output / "probe-layout.json").write_text(json.dumps(profile, indent=2) + "\n")
    rows = []
    for name in VI2:
        values = ",".join(f"0x{x:02x}" for x in code(name))
        rows.append(f" {{{hex(offset(name))},{{{values}}}}}, // {name}")
    contents = '#pragma once\n#include "research_api.h"\n'
    contents += "// Own probe only; NRO SHA256 " + profile["nro_sha256"] + "\n"
    contents += "static const struct { uint64_t offset; uint8_t bytes[16]; } vi2_own_symbols[]={\n"
    contents += "\n".join(rows) + "\n};\n"
    contents += '''static uintptr_t own_vi2_symbol(const ResearchApi *api,unsigned index) {
 if(index>=sizeof(vi2_own_symbols)/sizeof(vi2_own_symbols[0]))return 0;
 uintptr_t open=(uintptr_t)api->service_open;if(open<vi2_own_symbols[0].offset)return 0;
 uintptr_t base=open-vi2_own_symbols[0].offset;uint8_t checked[16];
 for(unsigned pass=0;pass<2;pass++){unsigned i=pass?index:0;
 if(api->read_memory(base+vi2_own_symbols[i].offset,checked,16))return 0;
 for(unsigned j=0;j<16;j++)if(checked[j]!=vi2_own_symbols[i].bytes[j])return 0;}
 return base+vi2_own_symbols[index].offset;
}
'''
    header.parent.mkdir(parents=True, exist_ok=True)
    header.write_text(contents)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, default=default_sdk())
    parser.add_argument("--elf", type=Path, default=ROOT / "build/gameshare-probe.elf")
    parser.add_argument("--nro", type=Path, default=ROOT / "dist/gameshare-probe.nro")
    parser.add_argument("--installed-031", action="store_true", help="Use the checked-in profile of the original installed probe")
    parser.add_argument("--profile", type=Path, help="Use another saved own-probe layout JSON instead of ELF/NRO")
    parser.add_argument("--output", type=Path, default=ROOT / "build")
    parser.add_argument("--header", type=Path, default=ROOT / "build/own_vi2_symbols.h",
                        help="For a new deployed build, explicitly select examples/own_vi2_symbols.h")
    args = parser.parse_args()
    try:
        if args.installed_031 and args.profile:
            raise ValueError("Choose --installed-031 or --profile, not both")
        if args.profile:
            profile = json.loads(args.profile.read_text())
        elif args.installed_031:
            profile = json.loads((ROOT / "records/installed-probe-0.3.1-layout.json").read_text())
        else:
            profile = from_build(args.elf, args.nro, args.sdk)
        emit(profile, args.output, args.header)
        print(f"Qualified NRO SHA-256: {profile['nro_sha256']}")
        print(f"Wrote guarded inputs to {args.output} and header to {args.header}")
        print("These inputs apply only to that exact NRO; no console request was made.")
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Qualification failed: {error}\n")


if __name__ == "__main__":
    main()
