param(
    [ValidateSet('all','libraries','app')][string]$Target = 'all',
    [string]$MsysRoot = "$env:USERPROFILE/.dirt2-tools/msys64",
    [string]$DevkitProDirectory = "$PSScriptRoot/../../../devkitPro"
)
$ErrorActionPreference = 'Stop'
$env:MSYSTEM = 'MSYS'
$env:CHERE_INVOKING = '1'
$env:MOONLIGHT_PROJECT_WIN = (Resolve-Path "$PSScriptRoot/../..").Path
$env:MOONLIGHT_SDK_WIN = (Resolve-Path -LiteralPath $DevkitProDirectory).Path
$env:MOONLIGHT_TARGET = $Target
& "$MsysRoot/usr/bin/bash.exe" -lc '
set -euo pipefail
cd "$(cygpath -u "$MOONLIGHT_PROJECT_WIN")"
export DEVKITPRO=$(cygpath -u "$MOONLIGHT_SDK_WIN")
export PATH="/ucrt64/bin:$PATH"
bash scripts/switch2/build.sh "$MOONLIGHT_TARGET"
'
if ($LASTEXITCODE -ne 0) { throw "Moonlight Switch 2 build failed ($LASTEXITCODE)" }
