Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$gitArguments = [string[]] $args

. (Join-Path $PSScriptRoot "tool-environment.ps1")

$repositoryRoot = Split-Path -Parent $PSScriptRoot
Enable-CsxRepositoryGitSafety -RepositoryRoot $repositoryRoot

$git = Get-Command git -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $git) {
    throw "Git was not found on PATH. Install Git for this platform."
}

Push-Location $repositoryRoot
try {
    & $git.Source @gitArguments
    exit $LASTEXITCODE
} finally {
    Pop-Location
}
