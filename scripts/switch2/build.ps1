param(
    [string]$MsysRoot = "$env:USERPROFILE/.dirt2-tools/msys64",
    [string]$DevkitProDirectory = "$PSScriptRoot/../../../devkitPro",
    [switch]$AllowDirtyDependencies,
    [switch]$RunTests,
    [switch]$CleanBuild
)
$ErrorActionPreference = 'Stop'
$env:MSYSTEM = 'MSYS'
$env:CHERE_INVOKING = '1'
$env:MOONLIGHT_PROJECT_WIN = (Resolve-Path "$PSScriptRoot/../..").Path
$env:MOONLIGHT_SDK_WIN = (Resolve-Path -LiteralPath $DevkitProDirectory).Path
$env:MOONLIGHT_CLEAN_BUILD = if ($CleanBuild) { '1' } else { '0' }
$env:MOONLIGHT_RUN_TESTS = if ($RunTests) { '1' } else { '0' }
$env:MOONLIGHT_SWITCH2_ALLOW_DIRTY_DEPS = if ($AllowDirtyDependencies) { '1' } else { '0' }
& "$MsysRoot/usr/bin/bash.exe" -lc '
set -euo pipefail
cd "$(cygpath -u "$MOONLIGHT_PROJECT_WIN")"
export DEVKITPRO=$(cygpath -u "$MOONLIGHT_SDK_WIN")
export PATH="/ucrt64/bin:$PATH"
build_args=()
if [ "$MOONLIGHT_CLEAN_BUILD" = 1 ]; then build_args+=(--clean-build); fi
if [ "$MOONLIGHT_RUN_TESTS" = 1 ]; then build_args+=(--run-tests); fi
bash scripts/switch2/build.sh "${build_args[@]}"
'
if ($LASTEXITCODE -ne 0) { throw "Moonlight Switch 2 build failed ($LASTEXITCODE)" }
