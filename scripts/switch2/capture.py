#!/usr/bin/env python3
"""Read and identify a native Vulkan device run without modifying device files."""

import argparse
import hashlib
import json
import re
from datetime import UTC, datetime
from ftplib import FTP
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REMOTE = "/switch/Moonlight-Switch/"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True)
    parser.add_argument("--port", type=int, default=5000)
    args = parser.parse_args()
    build = ROOT / "build/switch2"
    expected = json.loads((build / "validation.json").read_text())
    with FTP() as ftp:
        ftp.connect(args.host, args.port, timeout=10)
        ftp.login()
        data = bytearray()
        ftp.retrbinary("RETR " + REMOTE + "startup.log", data.extend)
        binary_hash = hashlib.sha256()
        ftp.retrbinary("RETR " + REMOTE + "Moonlight-Switch.nro", binary_hash.update)
    stamp = datetime.now(UTC).strftime("%Y%m%d-%H%M%S")
    log = build / ("device-vulkan-" + stamp + ".log")
    log.write_bytes(data)
    clean = re.sub(r"\x1b\[[0-9;]*m", "", data.decode("utf-8", errors="replace"))
    identified = (
        "SWITCH2_BUILD: id=" + expected["buildId"] + " renderer=native-vulkan"
    ) in clean
    matching = binary_hash.hexdigest() == expected["nroSha256"]
    selected = [
        line
        for line in clean.splitlines()
        if any(
            marker in line
            for marker in (
                "SWITCH2_BUILD",
                "SWITCH2_VULKAN",
                "SWITCH2_VK_IMPORT",
                "SWITCH2_HEALTH",
                "HWDEC_STAGE",
                "SWITCH2_HWDEC",
                "NanoVG Vulkan failure",
            )
        )
    ]
    record = {
        "log": log.name,
        "logSha256": hashlib.sha256(data).hexdigest(),
        "deviceNroSha256": binary_hash.hexdigest(),
        "expectedBuildId": expected["buildId"],
        "matchingBinary": matching,
        "matchingLogBuild": identified,
        "deviceExecuted": matching and identified,
        "nativeVulkanStartup": matching
        and identified
        and "SWITCH2_VULKAN: swapchain" in clean,
        "selectedLines": selected,
    }
    (build / ("device-vulkan-" + stamp + ".json")).write_text(
        json.dumps(record, indent=2) + "\n"
    )
    print(
        json.dumps(
            {**record, "selectedLines": selected[:20] + selected[-20:]}, indent=2
        )
    )


if __name__ == "__main__":
    main()
