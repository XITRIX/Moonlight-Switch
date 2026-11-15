#!/usr/bin/env python3
"""Verify the existing on-screen probe key and save it locally, never in Git."""
import argparse
import getpass
import os
from agent import Agent, ROOT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True, help="Switch LAN IPv4 address")
    parser.add_argument("--port", type=int, default=39381)
    args = parser.parse_args()
    key = getpass.getpass("Probe's 16-character pairing key (hidden): ").strip()
    state = Agent(args.host, key, args.port).status()
    folder = ROOT / "evidence"
    folder.mkdir(exist_ok=True)
    path = folder / "agent-key.txt"
    descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(descriptor, "w", encoding="ascii") as stream:
        stream.write(key + "\n")
    print(f"Paired with probe session {state.get('session_id', 'unknown')}; key saved in ignored local evidence/.")
    print("Only a read-only status request was sent; no native command was submitted.")


if __name__ == "__main__":
    main()
