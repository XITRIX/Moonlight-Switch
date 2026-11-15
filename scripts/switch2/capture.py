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


def presentation_evidence(log):
    windows = []
    for line in log.splitlines():
        if "SWITCH2_VULKAN_PRESENT: presents=" not in line:
            continue
        values = dict(re.findall(r"(\w+)=(true|false|[0-9]+(?:\.[0-9]+)?)", line))
        if int(values.get("newVideo", 0)):
            windows.append(values)
    direct = bool(windows) and any(int(w.get("directBuffer", 0)) for w in windows)
    direct = direct and all(int(w.get("copyBuffer", 0)) == 0 for w in windows)
    texel = direct and all(
        int(w.get("texel", 0)) > 0 and int(w.get("storage", 0)) == 0 for w in windows
    )
    return {
        "videoWindows": len(windows),
        "directBufferPresents": sum(int(w.get("directBuffer", 0)) for w in windows),
        "copyBufferPresents": sum(int(w.get("copyBuffer", 0)) for w in windows),
        "newVideoPresents": sum(int(w.get("newVideo", 0)) for w in windows),
        "skippedPresents": sum(int(w.get("skipped", 0)) for w in windows),
        "sharedGpuRegistration": bool(
            re.search(r"GPU-backed native image bytes=\d+ rc=0x0\b", log)
        ),
        "directBufferPathActive": direct,
        "directTexelSamplingActive": texel
        and "source=direct-decoder-buffer sampling=texel-buffer" in log,
        "frameDrivenPresentationActive": any(
            w.get("frameDriven") in ("true", "1") and int(w.get("skipped", 0)) > 0
            for w in windows
        ),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True)
    parser.add_argument("--port", type=int, default=5000)
    parser.add_argument("--build-dir", type=Path, default=Path("build/switch2"))
    parser.add_argument("--require-direct-texel", action="store_true")
    parser.add_argument("--require-frame-driven", action="store_true")
    args = parser.parse_args()
    build = ROOT / args.build_dir
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
    start = clean.rfind("SWITCH2_BUILD:")
    if start >= 0:
        clean = clean[start:]
    identified = (
        "SWITCH2_BUILD: id=" + expected["buildId"] + " renderer=native-vulkan"
    ) in clean
    matching = binary_hash.hexdigest() == expected["nroSha256"]
    evidence = presentation_evidence(clean)
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
        "presentationEvidence": evidence,
        "directBufferVerified": matching
        and identified
        and evidence["sharedGpuRegistration"]
        and evidence["directBufferPathActive"],
        "directTexelVerified": matching
        and identified
        and evidence["sharedGpuRegistration"]
        and evidence["directTexelSamplingActive"],
        "frameDrivenVerified": matching
        and identified
        and evidence["frameDrivenPresentationActive"],
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
    if args.require_direct_texel and not record["directTexelVerified"]:
        parser.exit(1, "Direct decoder-buffer texel sampling was not verified\n")
    if args.require_frame_driven and not record["frameDrivenVerified"]:
        parser.exit(1, "Frame-driven presentation was not verified\n")


if __name__ == "__main__":
    main()
