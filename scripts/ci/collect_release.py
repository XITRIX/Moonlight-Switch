#!/usr/bin/env python3
"""Give the artifacts from All Builds unambiguous public release filenames."""

import argparse
import re
import shutil
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

# Artifact name: (public platform name, extension, zip the artifact directory).
ASSETS = {
    "Moonlight-Switch.nro": ("Switch1", ".nro", False),
    "Debug.elf": ("Switch1", "-Debug.elf", False),
    "Moonlight-Switch2": ("Switch2", ".zip", False),
    "Moonlight-Switch2-debug": ("Switch2", "-Debug.zip", True),
    "Moonlight-PSVita-VPK": ("PSVita", ".vpk", False),
    "Moonlight-windows-ucrt64": ("Windows-x64", ".zip", True),
    "Moonlight-windows-clangarm64": ("Windows-arm64", ".zip", True),
    "Moonlight-macOS-arm64": ("macOS-arm64", ".zip", False),
    "Moonlight-iOS-arm64-unsigned": ("iOS-arm64", "-unsigned.ipa", False),
    "Moonlight-android-release-apk": ("Android", ".apk", False),
    "Moonlight-android-release-unsigned-apk": ("Android", "-unsigned.apk", False),
}
for architecture in ("x86_64", "arm64"):
    for package, extension in (
        ("appimage", ".AppImage"),
        ("tar", ".tar.gz"),
        ("deb", ".deb"),
        ("rpm", ".rpm"),
    ):
        ASSETS[f"Moonlight-linux-portable-{architecture}-{package}"] = (
            f"Linux-{architecture}",
            extension,
            False,
        )
    ASSETS[f"Moonlight-linux-steamrt4-{architecture}"] = (
        f"SteamOS-{architecture}",
        ".tar.gz",
        False,
    )

ANDROID_ASSETS = {
    "Moonlight-android-release-apk",
    "Moonlight-android-release-unsigned-apk",
}


def collect(tag: str, artifacts: Path, output: Path) -> list[Path]:
    """Validate the complete build set before writing any release files."""
    if not re.fullmatch(r"v[0-9]+\.[0-9]+\.[0-9]+", tag):
        raise ValueError(f"Expected a vX.X.X release tag, got {tag!r}")

    directories = {
        path.name: path
        for path in artifacts.iterdir()
        if path.is_dir() and not path.name.endswith("-build-logs")
    }
    unknown = directories.keys() - ASSETS.keys()
    missing = ASSETS.keys() - ANDROID_ASSETS - directories.keys()
    if unknown or missing:
        raise ValueError(
            f"Unexpected artifacts: {sorted(unknown)}; missing artifacts: {sorted(missing)}"
        )
    if len(directories.keys() & ANDROID_ASSETS) != 1:
        raise ValueError("Expected exactly one signed or unsigned Android APK artifact")

    plan = []
    for name, directory in sorted(directories.items()):
        platform, extension, bundle = ASSETS[name]
        files = sorted(path for path in directory.rglob("*") if path.is_file())
        if not files or (not bundle and len(files) != 1):
            raise ValueError(f"Unexpected file count in {name}: {len(files)}")
        if not bundle and not files[0].name.endswith(extension.removeprefix("-Debug")):
            raise ValueError(f"Unexpected file type in {name}: {files[0].name}")
        destination = output / f"Moonlight-{platform}-{tag}{extension}"
        if destination.exists():
            raise ValueError(f"Release asset already exists: {destination}")
        plan.append((directory, files, destination, bundle))

    output.mkdir(parents=True, exist_ok=True)
    for directory, files, destination, bundle in plan:
        if bundle:
            with ZipFile(destination, "w", compression=ZIP_DEFLATED) as archive:
                for source in files:
                    archive.write(source, source.relative_to(directory).as_posix())
        else:
            shutil.copy2(files[0], destination)
    return [destination for _, _, destination, _ in plan]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tag")
    parser.add_argument("artifacts", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    for asset in collect(args.tag, args.artifacts, args.output):
        print(asset)


if __name__ == "__main__":
    main()
