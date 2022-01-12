#!/bin/bash

cd "$( dirname "${BASH_SOURCE[0]}" )/.."

FIX=""
COLOR="auto"
STYLE="{ \
BasedOnStyle: LLVM,             \
IndentWidth: 4,                 \
DerivePointerAlignment: false,  \
PointerAlignment: Left,         \
}"

for v in "$@"; do
    if [[ "$v" == "--no-ansi" ]] || [[ "$v" == "-n" ]]; then
        COLOR="never"
    fi
    if [[ "$v" == "--fix" ]] || [[ "$v" == "-f" ]]; then
        FIX="1"
    fi
done

function clang_format_run() {
    python ./scripts/run-clang-format.py -r \
        --clang-format-executable="clang-format" \
        --style="$STYLE" \
        --color="$COLOR" \
        --exclude ./extern \
        ./app
}

if [[ -z "$FIX" ]]; then
    clang_format_run
else
    clang_format_run | patch -p1 -N -r -
fi
