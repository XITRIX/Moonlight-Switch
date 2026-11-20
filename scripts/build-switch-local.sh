#!/usr/bin/env bash
# Run from the normal Windows desktop/MSYS2, outside an app sandbox.
set -Eeuo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
exec > >(tee -a build/switch-local.log) 2>&1
echo "=== Switch OLED local build: $(date -Iseconds) ==="
trap 'result=$?; printf "Build error: line %s, exit %s, command: %s\n" "$LINENO" "$result" "$BASH_COMMAND" >&2; exit "$result"' ERR

export DEVKITPRO="${DEVKITPRO:-/c/devkitPro}"
export DEVKITA64="$DEVKITPRO/devkitA64"
export PATH="$DEVKITA64/bin:$DEVKITPRO/tools/bin:$PATH"
if [ ! -x "$DEVKITA64/bin/aarch64-none-elf-g++" ]; then
    echo "Missing devkitA64. Install Switch Development with the devkitPro installer."
    exit 1
fi

# A partial MSYS2 upgrade can leave an installed CMake with missing DLLs.
# Probe it before downloading sources. Core runtime upgrades require closing
# the MSYS2 shell, so repair them explicitly before restarting this script.
if ! /usr/bin/cmake.exe --version; then
    echo 'CMake cannot start. Update MSYS2 with pacman -Syu, restart the shell and repeat pacman -Syu until fully updated.' >&2
    exit 127
fi

# The Switch Development group installs the SDK, but not these app libraries.
# Upgrade the whole MSYS2 environment rather than creating a partial upgrade.
pacman -Syu --needed --noconfirm cmake make git \
    switch-glfw switch-sdl2 switch-mbedtls switch-libpng switch-libopus \
    switch-jansson switch-libexpat switch-zlib switch-libzstd switch-glm switch-dav1d
/usr/bin/cmake.exe --version

git submodule update --init \
    extern/borealis extern/moonlight-common-c extern/mdns extern/CImg
# Switch 1 uses deko3d, not SDL3. Avoid cloning SDL's full multi-platform history.
git -C extern/borealis submodule update --init library/lib/extern/glfw
git -C extern/moonlight-common-c submodule update --init --recursive
for dependency in borealis/library moonlight-common-c mdns; do
    if [ ! -f "extern/$dependency/CMakeLists.txt" ]; then
        echo "Incomplete dependency checkout: extern/$dependency/CMakeLists.txt" >&2
        exit 1
    fi
done

# Upstream CI permits an initial configure failure and configures a second time.
cmake -S . -B build/switch-local -G 'Unix Makefiles' \
    -DPLATFORM_SWITCH=ON -DUSE_DEKO3D=ON -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 || true
cmake -S . -B build/switch-local -G 'Unix Makefiles' \
    -DPLATFORM_SWITCH=ON -DUSE_DEKO3D=ON -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build/switch-local --target Moonlight.nro -j4

test -s build/switch-local/Moonlight.nro
mkdir -p build/dist
cp build/switch-local/Moonlight.nro build/dist/Moonlight-LowLatency.nro
sha256sum build/dist/Moonlight-LowLatency.nro
echo "NRO: $(pwd)/build/dist/Moonlight-LowLatency.nro"
echo "Experimental build: console testing still required."
