param([string]$RepositoryName = 'moonlight-switch-low-latency')
$ErrorActionPreference = 'Stop'
$projectDirectory = Split-Path -Parent $PSScriptRoot
$workspaceDirectory = Split-Path -Parent $projectDirectory
$ghDirectory = Join-Path $workspaceDirectory '.local-tools\github-cli'
$ghExecutable = Join-Path $ghDirectory 'gh.exe'
if (-not (Test-Path -LiteralPath $ghExecutable)) {
    throw "GitHub CLI not found: $ghExecutable"
}
$env:GH_CONFIG_DIR = Join-Path $workspaceDirectory '.local-tools\github-auth'
$env:PATH = "$ghDirectory;C:\devkitPro\msys2\usr\bin;$env:PATH"
$outputDirectory = Join-Path $projectDirectory 'build\github-actions'
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
Start-Transcript -Path (Join-Path $outputDirectory 'publish.log') -Append | Out-Null
try {
    Set-Location -LiteralPath $projectDirectory
    & $ghExecutable auth status
    if ($LASTEXITCODE -ne 0) {
        # Complete the official login in the user's normal Windows session.
        & $ghExecutable auth login --hostname github.com --git-protocol https --web --scopes workflow --skip-ssh-key
        if ($LASTEXITCODE -ne 0) { throw 'GitHub login did not complete.' }
    }
    $account = & $ghExecutable api user --jq '.login'
    if ($LASTEXITCODE -ne 0 -or -not $account) { throw 'Could not verify the GitHub account.' }

    $branch = (& git branch --show-current).Trim()
    if ($LASTEXITCODE -ne 0 -or $branch -ne 'switch-oled-low-latency') {
        throw 'Expected local branch switch-oled-low-latency. No code was pushed.'
    }

    # Configure credentials only for child Git processes; store no token in Git.
    $env:GIT_CONFIG_COUNT = '1'
    $env:GIT_CONFIG_KEY_0 = 'credential.https://github.com.helper'
    $env:GIT_CONFIG_VALUE_0 = '!gh auth git-credential'

    $remoteNames = @(& git remote)
    if ($LASTEXITCODE -ne 0) { throw 'Could not inspect local Git remotes.' }
    if ($remoteNames -contains 'actions-build') {
        $remoteUrl = & git remote get-url actions-build
        if ($LASTEXITCODE -ne 0) { throw 'Could not read the actions-build remote.' }
        $expectedPrefix = "https://github.com/$account/"
        if (-not $remoteUrl.StartsWith($expectedPrefix)) {
            throw 'Existing actions-build remote is not owned by the authenticated account.'
        }
        $repository = $remoteUrl.Substring('https://github.com/'.Length) -replace '\.git$', ''
    } else {
        $repository = "$account/$RepositoryName"
        $repositoryNames = @(& $ghExecutable repo list $account --limit 1000 --json name --jq '.[].name')
        if ($LASTEXITCODE -ne 0) { throw 'Could not inspect existing repositories.' }
        if ($repositoryNames -contains $RepositoryName) {
            # Do not overwrite or push into an unrelated existing repository.
            $RepositoryName += '-' + (Get-Date -Format 'yyyyMMdd-HHmmss')
            $repository = "$account/$RepositoryName"
        }
        & $ghExecutable repo create $repository --private --description 'Experimental Moonlight Switch OLED with selectable low latency and smooth video' --source $projectDirectory --remote actions-build
        if ($LASTEXITCODE -ne 0) { throw 'Could not create the private repository.' }
    }

    $repository | Set-Content -LiteralPath (Join-Path $outputDirectory 'repository.txt') -Encoding UTF8
    & git push --set-upstream actions-build $branch
    if ($LASTEXITCODE -ne 0) { throw 'Git push failed.' }
    Write-Output "GitHub Actions: https://github.com/$repository/actions/workflows/switch-oled.yml"

    $commit = (& git rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0) { throw 'Could not determine the pushed commit.' }
    $runId = $null
    for ($attempt = 0; $attempt -lt 12; $attempt++) {
        $runJson = & $ghExecutable run list --repo $repository --branch $branch --commit $commit --limit 10 --json databaseId,workflowName
        if ($LASTEXITCODE -ne 0) { throw 'Could not inspect workflow runs.' }
        $run = @($runJson | ConvertFrom-Json | Where-Object workflowName -eq 'Switch OLED Low Latency' | Select-Object -First 1)
        if ($run.Count -gt 0) { $runId = $run[0].databaseId; break }
        Start-Sleep -Seconds 3
    }
    if (-not $runId) {
        throw "The workflow was not scheduled. Open https://github.com/$repository/actions to inspect Actions permissions."
    }
    $runUrl = "https://github.com/$repository/actions/runs/$runId"
    $runUrl | Set-Content -LiteralPath (Join-Path $outputDirectory 'run-url.txt') -Encoding UTF8
    Write-Output "Build: $runUrl"
    & $ghExecutable run watch $runId --repo $repository --exit-status --interval 10
    if ($LASTEXITCODE -ne 0) {
        & $ghExecutable run view $runId --repo $repository --log-failed 2>&1 |
            Out-File -LiteralPath (Join-Path $outputDirectory 'failure.log') -Encoding UTF8
        throw "Actions build failed. Details: $outputDirectory\failure.log"
    }
    $artifactDirectory = Join-Path $outputDirectory "artifact-$runId"
    & $ghExecutable run download $runId --repo $repository --name Moonlight-Switch-OLED-LowLatency --dir $artifactDirectory
    if ($LASTEXITCODE -ne 0) { throw 'Could not download the NRO artifact.' }
    $nro = Join-Path $artifactDirectory 'switch\Moonlight-LowLatency\Moonlight.nro'
    if (-not (Test-Path -LiteralPath $nro)) { throw 'Expected NRO missing from the downloaded artifact.' }
    Write-Output "NRO ready: $nro"
    Get-FileHash -LiteralPath $nro -Algorithm SHA256 | Format-List
} finally {
    Stop-Transcript | Out-Null
}
