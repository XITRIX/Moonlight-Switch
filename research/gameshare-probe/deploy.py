#!/usr/bin/env python3
"""Install only GameShare Probe's NRO through FTP, preserving any prior build."""
import argparse
from datetime import datetime, timezone
from ftplib import FTP, error_perm
import hashlib
import io
import json
import secrets
from pathlib import Path

ROOT = Path(__file__).resolve().parent
REMOTE = "/switch/gameshare-probe"
NAME = "gameshare-probe.nro"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True)
    parser.add_argument("--port", type=int, default=5000)
    args = parser.parse_args()
    data = (ROOT / "dist" / NAME).read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    build = json.loads((ROOT / "dist/build.json").read_text())
    if digest != build["nro_sha256"] or data[16:20] != b"NRO0":
        parser.error("NRO does not match its build record")
    backup = None
    with FTP() as ftp:
        ftp.connect(args.host, args.port, timeout=15)
        ftp.login()
        ftp.cwd("/switch")
        try:
            ftp.cwd(REMOTE)
        except error_perm:
            ftp.mkd(REMOTE)
            ftp.cwd(REMOTE)

        def read_hash(name):
            result = hashlib.sha256()
            ftp.retrbinary("RETR " + name, result.update)
            return result.hexdigest()

        staged = NAME + ".upload-" + digest[:12]
        ftp.storbinary("STOR " + staged, io.BytesIO(data))
        if read_hash(staged) != digest:
            raise RuntimeError("Uploaded staging file failed SHA-256 readback")
        names = [name.rsplit("/", 1)[-1] for name in ftp.nlst()]
        if NAME in names:
            previous = read_hash(NAME)
            stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
            backup = NAME + ".backup-" + previous[:12] + "-" + stamp
            ftp.rename(NAME, backup)
        ftp.rename(staged, NAME)
        if read_hash(NAME) != digest:
            raise RuntimeError("Installed file failed SHA-256 readback")
        names = [name.rsplit("/",1)[-1] for name in ftp.nlst()]
        if "agent-key.txt" in names:
            key_bytes=bytearray()
            ftp.retrbinary("RETR agent-key.txt",key_bytes.extend)
            key=bytes(key_bytes).decode("ascii").strip()
            if len(key)!=16 or any(c not in "0123456789abcdef" for c in key):
                raise RuntimeError("Existing agent pairing key is invalid; retained without replacing")
        else:
            key=secrets.token_hex(8)
            key_bytes=(key+"\n").encode("ascii")
            ftp.storbinary("STOR agent-key.txt",io.BytesIO(key_bytes))
            if read_hash("agent-key.txt")!=hashlib.sha256(key_bytes).hexdigest():
                raise RuntimeError("Agent pairing key readback failed")
        (ROOT / "evidence").mkdir(exist_ok=True)
        (ROOT / "evidence/agent-key.txt").write_text(key+"\n",encoding="ascii")
        listing = ftp.nlst()
    record = {"host": args.host, "port": args.port, "path": REMOTE + "/" + NAME,
              "bytes": len(data), "sha256": digest, "previous_backup": backup,
              "readback_verified": True, "device_executed": False, "pairing_key_configured":True, "listing": listing}
    (ROOT / "dist/deployment.json").write_text(json.dumps(record, indent=2) + "\n")
    print(json.dumps(record, indent=2))


if __name__ == "__main__":
    main()
