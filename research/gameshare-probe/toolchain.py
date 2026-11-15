"""Resolve the public homebrew toolchain without depending on a sibling project."""
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parent


def default_sdk():
    configured = os.environ.get("DEVKITPRO")
    if configured:
        return Path(configured).expanduser()
    for parent in ROOT.parents:
        candidate = parent / "devkitPro"
        if (candidate / "libounce/include/switch.h").is_file():
            return candidate
    return Path("C:/devkitPro" if os.name == "nt" else "/opt/devkitpro")


def executable(sdk, relative):
    return Path(sdk) / (relative + (".exe" if os.name == "nt" else ""))
