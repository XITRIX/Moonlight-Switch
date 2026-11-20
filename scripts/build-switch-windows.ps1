param([string]$DevkitPro = 'C:\devkitPro')
$ErrorActionPreference = 'Stop'
$devkitBash = Join-Path $DevkitPro 'msys2\usr\bin\bash.exe'
if (-not (Test-Path -LiteralPath $devkitBash)) {
    throw "devkitPro MSYS2 not found: $devkitBash"
}
$projectDirectory = Split-Path -Parent $PSScriptRoot
$projectScript = Join-Path $PSScriptRoot 'build-switch-local.sh'
$devkitPath = '/' + $DevkitPro.Substring(0, 1).ToLowerInvariant() + $DevkitPro.Substring(2).Replace('\', '/')
$scriptPath = '/' + $projectScript.Substring(0, 1).ToLowerInvariant() + $projectScript.Substring(2).Replace('\', '/')
$env:DEVKITPRO = $devkitPath
$env:MSYS2_PATH_TYPE = 'inherit'
# Pass the script as an argument rather than embedding paths in shell code.
& $devkitBash --login $scriptPath
if ($LASTEXITCODE -ne 0) {
    throw "Build failed (exit $LASTEXITCODE). See $projectDirectory\build\switch-local.log"
}
Write-Output "NRO ready: $projectDirectory\build\dist\Moonlight-LowLatency.nro"
