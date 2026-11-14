#!/usr/bin/env python3
"""Stage and readback-verify the native Switch 2 NRO, retaining any prior build."""

import argparse
import hashlib
import json
from datetime import UTC, datetime
from ftplib import FTP, error_perm
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REMOTE = "/switch/Moonlight-Switch"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True)
    parser.add_argument("--port", type=int, default=5000)
    args = parser.parse_args()
    nro = ROOT / "build/switch2/Moonlight-Switch.nro"
    data = nro.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    validation = json.loads((nro.parent / "validation.json").read_text())
    if validation["nroSha256"] != digest:
        parser.error("The NRO does not match its validation record")
    with FTP() as ftp:
        ftp.connect(args.host, args.port, timeout=30)
        ftp.login()
        try:
            ftp.cwd(REMOTE)
        except error_perm:
            ftp.mkd(REMOTE)
            ftp.cwd(REMOTE)

        def remote_hash(name):
            result = hashlib.sha256()
            ftp.retrbinary("RETR " + name, result.update)
            return result.hexdigest()

        filename = "Moonlight-Switch.nro"
        staged = filename + ".upload-" + digest[:12]
        with nro.open("rb") as stream:
            ftp.storbinary("STOR " + staged, stream, blocksize=262144)
        assert remote_hash(staged) == digest, "Staged upload verification failed"
        names = ftp.nlst()
        backup = None
        if any(name.rsplit("/", 1)[-1] == filename for name in names):
            previous = remote_hash(filename)
            stamp = datetime.now(UTC).strftime("%Y%m%d-%H%M%S")
            backup = filename + ".backup-" + previous[:12] + "-" + stamp
            ftp.rename(filename, backup)
        ftp.rename(staged, filename)
        assert remote_hash(filename) == digest, "Activated NRO verification failed"
    record = {
        "host": args.host,
        "port": args.port,
        "path": REMOTE + "/Moonlight-Switch.nro",
        "sha256": digest,
        "bytes": len(data),
        "previousNroBackup": backup,
        "readbackVerified": True,
        "deviceExecuted": False,
    }
    (nro.parent / "deployment.json").write_text(json.dumps(record, indent=2) + "\n")
    print(json.dumps(record, indent=2))


if __name__ == "__main__":
    main()
