#!/usr/bin/env python3
"""Package a validated NRO with HBMenu's matching directory and file names."""

import hashlib
import json
import os
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    build = ROOT / os.environ.get("MOONLIGHT_SWITCH2_BUILD_DIR", "build/switch2")
    nro = build / "Moonlight-Switch.nro"
    data = nro.read_bytes()
    validation = json.loads((build / "validation.json").read_text())
    if hashlib.sha256(data).hexdigest() != validation["nroSha256"]:
        raise SystemExit("NRO does not match its validation record")
    if data[0x10:0x14] != b"NRO0":
        raise SystemExit("Invalid NRO header")
    output = ROOT / "build/dist-switch2/Moonlight-Switch2.zip"
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as archive:
        archive.write(nro, "switch/Moonlight-Switch/Moonlight-Switch.nro")
    print(output)


if __name__ == "__main__":
    main()
