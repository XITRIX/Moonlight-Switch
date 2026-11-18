"""Remove Switch 2 build products, retaining verified source download archives."""

import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCES = ("zlib", "mbedtls", "curl", "jansson", "expat", "png", "opus", "zstd", "ffmpeg")


def clean(root: Path, build: Path):
    root = root.resolve()
    build = build.resolve()
    base = root / "build"
    # Check the app output before removing anything. A typo or an override
    # must not turn a clean build into removal of a source tree or SDK.
    if not build.is_relative_to(base) or build in (
        base, base / "deps", base / "deps/src", base / "switch2-generated",
    ) or build.is_relative_to(base / "deps"):
        raise SystemExit("--clean-build requires an app output directory under the project's build/ tree")
    if build.exists() and any(build.iterdir()):
        cache = build / "CMakeCache.txt"
        if not cache.is_file() or "PLATFORM_SWITCH2:BOOL=ON" not in cache.read_text():
            raise SystemExit(f"Refusing to clean a directory without a Switch 2 CMake cache: {build}")
    targets = [build, base / "switch2-generated", base / "deps/switch2", base / "deps/switch2-work"]
    targets += [base / "deps/src" / name for name in SOURCES]
    if any(not target.parent.resolve().is_relative_to(base) for target in targets):
        raise SystemExit("Refusing to clean through a directory symlink outside build/")
    for target in targets:
        if target.is_symlink():
            target.unlink()
        elif target.exists():
            shutil.rmtree(target)


if __name__ == "__main__":
    clean(ROOT, Path(sys.argv[1]))
    print("Cleaned Switch 2 app and dependencies; source download archives retained")
