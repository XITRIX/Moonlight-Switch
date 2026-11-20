#!/bin/bash
# All dependencies are built for libounce in a project-local prefix.
set -euo pipefail
root=$PWD
src="$root/build/deps/src"
work="$root/build/deps/switch2-work"
prefix="$root/build/deps/switch2"
jobs=${MOONLIGHT_BUILD_JOBS:-8}
toolchain="$DEVKITPRO/libounce/cmake/Switch2.cmake"
[ -f "$toolchain" ] || {
    echo 'Install the Ounce SDK first' >&2
    exit 1
}
mkdir -p "$src" "$work" "$prefix"
fetch() {
    local name=$1 url=$2
    local archive="$src/$name.tar" checksum download
    checksum=$(awk -v file="$name.tar" '$2 == file { print $1 }' "$root/scripts/switch2/sources.sha256")
    [ -n "$checksum" ] || {
        echo "No checksum for $name" >&2
        exit 1
    }
    if [ ! -f "$archive" ] || ! printf '%s  %s\n' "$checksum" "$archive" | sha256sum --check --status; then
        download=$(mktemp "$archive.XXXXXX")
        if ! curl --fail --location --retry 3 "$url" -o "$download"; then
            rm -f "$download"
            return 1
        fi
        if ! printf '%s  %s\n' "$checksum" "$download" | sha256sum --check --status; then
            echo "SHA-256 mismatch for $name downloaded from $url" >&2
            rm -f "$download"
            return 1
        fi
        mv -f "$download" "$archive"
    fi
    if [ ! -d "$src/$name" ]; then
        mkdir -p "$src/$name"
        tar -xf "$archive" -C "$src/$name" --strip-components=1 \
            --exclude='*/tests/cli-tests/bin'
    fi
}
lib() {
    local name=$1
    shift
    if [ ! -f "$work/$name.done" ]; then
        cmake -S "$src/$name" -B "$work/$name" -G Ninja \
            -DCMAKE_TOOLCHAIN_FILE="$toolchain" -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_PREFIX_PATH="$prefix" \
            -DCMAKE_FIND_ROOT_PATH="$prefix" -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
            -DBUILD_SHARED_LIBS=OFF "$@" >"$work/$name.log" 2>&1 || {
            tail -n 40 "$work/$name.log" >&2
            return 1
        }
        cmake --build "$work/$name" --target install -j "$jobs" >>"$work/$name.log" 2>&1 || {
            tail -n 40 "$work/$name.log" >&2
            return 1
        }
        touch "$work/$name.done"
    fi
}
fetch zlib https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz
lib zlib -DZLIB_BUILD_EXAMPLES=OFF
fetch mbedtls https://github.com/Mbed-TLS/mbedtls/archive/refs/tags/v2.28.10.tar.gz
if [ ! -f "$src/mbedtls/.moonlight-entropy" ]; then
    printf '\n#define MBEDTLS_NO_PLATFORM_ENTROPY\n#define MBEDTLS_ENTROPY_HARDWARE_ALT\n' >>"$src/mbedtls/include/mbedtls/config.h"
    printf '\ntarget_sources(mbedcrypto PRIVATE "%s/scripts/switch2/mbedtls_entropy.c")\n' "$root" >>"$src/mbedtls/library/CMakeLists.txt"
    touch "$src/mbedtls/.moonlight-entropy"
fi
python3 - "$src/mbedtls/include/mbedtls/config.h" <<'PY'
import re
import sys
from pathlib import Path

config = Path(sys.argv[1])
config.write_text(re.sub(
    r"^#define (MBEDTLS_TIMING_C|MBEDTLS_HAVEGE_C|MBEDTLS_NET_C)$",
    r"// #define \1", config.read_text(), flags=re.MULTILINE,
))
PY
lib mbedtls -DENABLE_PROGRAMS=OFF -DENABLE_TESTING=OFF
fetch curl https://curl.se/download/curl-8.14.1.tar.xz
lib curl -DBUILD_CURL_EXE=OFF -DBUILD_TESTING=OFF -DCURL_USE_MBEDTLS=ON \
    -DCURL_USE_OPENSSL=OFF -DCURL_USE_LIBPSL=OFF -DCURL_USE_LIBSSH2=OFF \
    -DCURL_ZSTD=OFF -DCURL_BROTLI=OFF -DCURL_USE_LIBIDN2=OFF \
    -DENABLE_THREADED_RESOLVER=OFF -DENABLE_ARES=OFF -DCURL_DISABLE_LDAP=ON \
    -DENABLE_UNIX_SOCKETS=OFF -DHAVE_SOCKETPAIR=OFF -DHAVE_BASENAME=OFF \
    -DBUILD_LIBCURL_DOCS=OFF
fetch jansson https://github.com/akheron/jansson/archive/refs/tags/v2.14.1.tar.gz
lib jansson -DJANSSON_BUILD_DOCS=OFF -DJANSSON_EXAMPLES=OFF -DJANSSON_WITHOUT_TESTS=ON
fetch expat https://github.com/libexpat/libexpat/releases/download/R_2_7_1/expat-2.7.1.tar.xz
python3 scripts/switch2/prepare_expat.py
lib expat -DEXPAT_BUILD_TOOLS=OFF -DEXPAT_BUILD_EXAMPLES=OFF -DEXPAT_BUILD_TESTS=OFF -DEXPAT_SHARED_LIBS=OFF
fetch png https://github.com/pnggroup/libpng/archive/refs/tags/v1.6.48.tar.gz
lib png -DPNG_SHARED=OFF -DPNG_TESTS=OFF -DPNG_TOOLS=OFF
fetch opus https://downloads.xiph.org/releases/opus/opus-1.5.2.tar.gz
lib opus -DOPUS_BUILD_TESTING=OFF -DOPUS_BUILD_PROGRAMS=OFF -DOPUS_INSTALL_PKG_CONFIG_MODULE=ON
fetch zstd https://github.com/facebook/zstd/archive/refs/tags/v1.5.7.tar.gz
if [ ! -f "$work/zstd.done" ]; then
    cmake -S "$src/zstd/build/cmake" -B "$work/zstd" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$toolchain" -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$prefix" -DZSTD_BUILD_PROGRAMS=OFF \
        -DZSTD_BUILD_SHARED=OFF -DZSTD_MULTITHREAD_SUPPORT=OFF >"$work/zstd.log" 2>&1
    cmake --build "$work/zstd" --target install -j "$jobs" >>"$work/zstd.log" 2>&1
    touch "$work/zstd.done"
fi
# SDL3 is built from Borealis's pinned XITRIX/SDL submodule by CMake.
fetch ffmpeg https://ffmpeg.org/releases/ffmpeg-7.1.1.tar.xz
if [ ! -f "$work/ffmpeg.done" ]; then
    mkdir -p "$work/ffmpeg"
    cpu=$(sed -n 's/^SWITCH2_CPU[[:space:]]*:=[[:space:]]*//p' "$DEVKITPRO/libounce/switch2_cpu.mk")
    flags="$cpu -mtp=soft -ftls-model=local-exec -fPIE"
    (
        cd "$work/ffmpeg"
        "$src/ffmpeg/configure" --prefix="$prefix" --target-os=none --arch=aarch64 \
            --enable-cross-compile --cross-prefix="$DEVKITPRO/devkitA64/bin/aarch64-none-elf-" \
            --extra-cflags="-D__SWITCH__ -D__SWITCH2__ $flags -isystem $DEVKITPRO/libounce/include" \
            --extra-ldflags="-specs=$DEVKITPRO/libounce/switch.specs $flags -L$DEVKITPRO/libounce/lib" \
            --extra-libs=-lnx --enable-static --disable-shared --enable-pic \
            --disable-debug --disable-autodetect --disable-programs --disable-doc \
            --disable-everything --disable-network --disable-avdevice --disable-avfilter \
            --disable-swscale --enable-swresample --enable-pthreads \
            --enable-decoder=h264,hevc --enable-parser=h264,hevc >"$work/ffmpeg.log" 2>&1
        make -j "$jobs" install >>"$work/ffmpeg.log" 2>&1
    )
    touch "$work/ffmpeg.done"
fi
echo "Switch 2 libraries ready: $prefix"
