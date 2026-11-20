"""Identify archive and devkitPro package installations of the Switch 2 SDK."""

from pathlib import Path


def sdk_identity(sdk: Path) -> str:
    for name in ("ounce-devkit-version.txt", "ounce-sdk-version.txt"):
        record = sdk / name
        if record.is_file() and record.read_text().strip():
            return record.read_text().strip()

    # Package-based SDK installs do not ship the archive's version record.
    # Read the database under this DEVKITPRO, rather than identifying another
    # installation via whichever pacman happens to be on PATH.
    database = sdk / "pacman/var/lib/pacman/local"
    packages = []
    for name in ("libounce", "ouncevk"):
        versions = set()
        for desc in database.glob(name + "-*/desc"):
            fields = {}
            for section in desc.read_text().split("\n\n"):
                lines = section.strip().splitlines()
                if len(lines) >= 2:
                    fields[lines[0]] = lines[1]
            if fields.get("%NAME%") == name and fields.get("%VERSION%"):
                versions.add(fields["%VERSION%"])
        if len(versions) != 1:
            raise SystemExit(
                f"Cannot identify Switch 2 SDK at {sdk}: expected an SDK version "
                f"record or installed libounce/ouncevk package metadata ({name}). "
                "Check DEVKITPRO and the SDK installation."
            )
        packages.append(name + "=" + versions.pop())
    return "devkitPro packages: " + ", ".join(packages)


if __name__ == "__main__":
    import os

    print("Switch 2 SDK: " + sdk_identity(Path(os.environ["DEVKITPRO"])))
