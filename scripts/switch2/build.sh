#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
: "${DEVKITPRO:?Set DEVKITPRO to the SDK containing libounce and OunceVK}"
export PATH="$DEVKITPRO/devkitA64/bin:$DEVKITPRO/tools/bin:$PATH"
mode=${1:-all}
case "$mode" in all | libraries | app) ;; *) exit 2 ;; esac
prefix="$PWD/build/deps/switch2"
build_dir=${MOONLIGHT_SWITCH2_BUILD_DIR:-build/switch2}
if [ "$mode" != app ]; then bash scripts/switch2/dependencies.sh; fi
if [ "$mode" = libraries ]; then exit 0; fi
python3 scripts/switch2/test_entropy.py
python3 scripts/switch2/test_touch.py
python3 scripts/switch2/test_decoder.py
python3 scripts/switch2/test_presentation.py
python3 scripts/switch2/test_capture.py
python3 scripts/switch2/test_mouse_scroll.py
python3 scripts/switch2/test_system_button_settings.py
python3 extern/borealis/tests/test_activity_lifecycle.py
python3 extern/borealis/tests/test_deletion_queue.py
python3 extern/borealis/tests/test_timing.py
python3 extern/borealis/tests/test_switch_mouse.py
python3 extern/borealis/tests/test_system_buttons.py
python3 extern/borealis/tests/test_half_joycon.py
python3 scripts/switch2/build_vulkan_shaders.py
cmake -S . -B "$build_dir" -G Ninja -DPLATFORM_SWITCH2=ON \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_PREFIX_PATH="$prefix" -DCMAKE_FIND_ROOT_PATH="$prefix" \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DENABLE_UPSCALING=OFF
cmake --build "$build_dir" --target Moonlight-Switch.nro -j "${MOONLIGHT_BUILD_JOBS:-8}"
verification_args=()
if [ "${MOONLIGHT_SWITCH2_ALLOW_DIRTY_DEPS:-0}" = 1 ]; then
    verification_args+=(--allow-dirty-dependencies)
fi
python3 scripts/switch2/verify_build.py "${verification_args[@]}"
