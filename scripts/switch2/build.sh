#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
run_tests=false
clean_build=false
usage() {
    echo 'Usage: bash scripts/switch2/build.sh [--clean-build] [--run-tests]'
}
for arg in "$@"; do
    case "$arg" in
        --run-tests) run_tests=true ;;
        --clean-build) clean_build=true ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown argument: $arg" >&2; usage >&2; exit 2 ;;
    esac
done
: "${DEVKITPRO:?Set DEVKITPRO to the SDK containing libounce and OunceVK}"
export PATH="$DEVKITPRO/devkitA64/bin:$DEVKITPRO/tools/bin:$PATH"
prefix="$PWD/build/deps/switch2"
build_dir=${MOONLIGHT_SWITCH2_BUILD_DIR:-build/switch2}
python3 scripts/switch2/sdk_identity.py
if [ "$clean_build" = true ]; then
    python3 scripts/switch2/clean_build.py "$build_dir"
fi
bash scripts/switch2/dependencies.sh
if [ "$run_tests" = true ]; then
    python3 scripts/switch2/test_sdk_identity.py
    python3 scripts/switch2/test_entropy.py
    python3 scripts/switch2/test_touch.py
    python3 scripts/switch2/test_decoder.py
    python3 scripts/switch2/test_presentation.py
    python3 scripts/switch2/test_capture.py
    python3 scripts/switch2/test_mouse_scroll.py
    python3 scripts/switch2/test_system_button_settings.py
    python3 scripts/test_stream_lifecycle.py
    python3 scripts/test_onscreen_gamepad.py
    python3 extern/borealis/tests/test_activity_lifecycle.py
    python3 extern/borealis/tests/test_deletion_queue.py
    python3 extern/borealis/tests/test_timing.py
    python3 extern/borealis/tests/test_switch_mouse.py
    python3 extern/borealis/tests/test_system_buttons.py
    python3 extern/borealis/tests/test_half_joycon.py
    python3 extern/borealis/tests/test_switch_players.py
fi
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
printf 'Switch 2 NRO ready: %s/Moonlight-Switch.nro\n' "$(cd "$build_dir" && pwd)"
