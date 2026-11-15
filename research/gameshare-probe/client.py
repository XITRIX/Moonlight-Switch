#!/usr/bin/env python3
"""Read a running GameShare Probe; standard library only, no remote mutations."""
import argparse
from datetime import datetime, timezone
import getpass
import ipaddress
import json
from pathlib import Path
import time
from urllib.error import HTTPError, URLError
from urllib.request import Request, ProxyHandler, build_opener

SCHEMA = "gameshare-probe-v1"
MAX_REPORT = 512 * 1024


def fetch(host, port, key, timeout=5):
    # A literal LAN address also prevents accidental public endpoint/URL injection.
    address = ipaddress.IPv4Address(host)
    if not (address.is_private or address.is_loopback) or address.is_unspecified:
        raise ValueError("Use the console's literal LAN IPv4 address")
    request = Request(f"http://{address}:{port}/report", headers={"X-Probe-Key": key})
    opener = build_opener(ProxyHandler({}))
    with opener.open(request, timeout=timeout) as response:
        data = response.read(MAX_REPORT + 1)
    if len(data) > MAX_REPORT:
        raise ValueError("Report exceeds protocol limit")
    report = json.loads(data)
    if report.get("schema") != SCHEMA or report.get("error"):
        raise ValueError(f"Invalid probe report: {report.get('error', 'schema mismatch')}")
    return report


def summary(report):
    lines = [f"GameShare Probe {report.get('version', '?')} | firmware {report.get('firmware', '?')}"]
    for service in report.get("services", []):
        result = service.get("result")
        detail = "" if result is None else f" {result['hex']} (module {result['module']}, description {result['description']})"
        lines.append(f"  {service['name']:8} {service['state']}{detail}")
    creator = report.get("streamplay_creator")
    if creator:
        result = creator.get("result")
        lines.append(f"StreamPlay scoped-object creator: {creator['state']} " +
                     (result['hex'] if result else "(pending/untried)"))
    scan = report.get("module_scan", {})
    symbols = scan.get("symbols", [])
    candidates = [s for s in symbols if s.get("executable_candidate")]
    lines.append(f"Own modules: {len(scan.get('modules', []))}; matching symbols: {len(symbols)}; executable candidates: {len(candidates)}")
    for symbol in candidates:
        lines.append(f"  {symbol['address']} {symbol['name']}")
    if not report.get("worker_done"):
        lines.append("Service worker pending; this is not a failed/absent service result.")
    lines.append("Native command results are recorded separately by the research agent." if report.get("remote_execution_enabled")
                 else "No hosting commands or discovered functions were called.")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("host", help="Console's LAN IPv4 address shown on screen")
    parser.add_argument("--port", type=int, default=39381)
    parser.add_argument("--key", help="Pairing key shown by the NRO (prompted if omitted)")
    parser.add_argument("--output", type=Path, default=Path("evidence"))
    parser.add_argument("--watch", type=float, metavar="SECONDS", help="Poll until service discovery finishes or this deadline elapses")
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error("Invalid port")
    key = args.key or getpass.getpass("Pairing key from console: ")
    if len(key) != 16 or any(c not in "0123456789abcdef" for c in key):
        parser.error("Pairing key must be the 16 lowercase hexadecimal characters shown on screen")
    if args.watch is not None and not 0 < args.watch <= 300:
        parser.error("--watch must be between 0 and 300 seconds")
    args.output.mkdir(parents=True, exist_ok=True)
    deadline = time.monotonic() + (args.watch or 0)
    previous = None
    try:
        while True:
            report = fetch(args.host, args.port, key)
            if previous != report.get("generation"):
                print(summary(report))
                stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
                record = {"captured_at_utc": stamp, "host": args.host, "port": args.port, "report": report}
                path = args.output / f"gameshare-{stamp}.json"
                path.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
                print(f"Saved {path.resolve()}")
                previous = report.get("generation")
            if report.get("worker_done") or not args.watch or time.monotonic() >= deadline:
                break
            time.sleep(min(1, max(0, deadline-time.monotonic())))
    except (OSError, ValueError, HTTPError, URLError) as error:
        parser.exit(1, f"Probe capture failed: {error}\n")


if __name__ == "__main__":
    main()
