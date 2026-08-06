<#
.SYNOPSIS
    Run clang-tidy across the project from a VS Developer Shell.

.DESCRIPTION
    Wraps LLVM's run-clang-tidy (shipped next to clang-tidy.exe) so linting sees
    the same MSVC toolchain/INCLUDE as the build that produced the compile DB.

    Linting targets the clang-x64 compile database — clang-tidy requires a
    clang-produced compile_commands.json — so run scripts/build.ps1 -Preset
    clang-x64 first. Uses the project .clang-tidy config; output is passed
    through unfiltered.

.EXAMPLE
    .\scripts\run-clang-tidy.ps1                      # all project TUs, .clang-tidy config
    .\scripts\run-clang-tidy.ps1 -Files 'sm5/step'    # only TUs whose path matches
    .\scripts\run-clang-tidy.ps1 -Changed             # only TUs changed since HEAD
    .\scripts\run-clang-tidy.ps1 -Fix                 # apply fixes
#>
param (
    [switch]$Fix,
    [switch]$Quiet,
    [string]$Files,
    [switch]$Changed,
    [string]$Base = "HEAD",
    [int]$Jobs = 16,
    [switch]$Help
)

$ErrorActionPreference = "Stop"

# --- Help mode ---
if ($Help) {
    Write-Host "`nUsage: .\scripts\run-clang-tidy.ps1 [options]`n" -ForegroundColor Cyan
    Write-Host "Options:" -ForegroundColor Yellow
    Write-Host "  -Files <regexes>   Comma-separated regexes to scope which TUs run"
    Write-Host "  -Changed           Only lint TUs changed since -Base (default HEAD)"
    Write-Host "  -Base <ref>        Git ref for -Changed"
    Write-Host "  -Fix               Apply automatic fixes (and format)"
    Write-Host "  -Quiet             Suppress per-TU clang-tidy noise"
    Write-Host "  -Jobs <n>          Parallel jobs (default: 16)"
    Write-Host "  -Help              Show this help"
    Write-Host "`nNote: requires the clang-x64 compile DB; run scripts/build.ps1 -Preset clang-x64 first.`n" -ForegroundColor Gray
    exit 0
}

$scriptRoot = $PSScriptRoot
$repoRoot = Split-Path $scriptRoot -Parent
$BuildDir = "out/build/clang-x64"

# --- Initialize the VS Dev Shell (same environment as build.ps1) ---
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Host "ERROR: vswhere.exe not found. Visual Studio must be installed." -ForegroundColor Red
    exit 1
}
$vsPath = & $vswhere -latest -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) {
    Write-Host "ERROR: No matching Visual Studio installation detected." -ForegroundColor Red
    exit 1
}
$devShellModule = Join-Path $vsPath "Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
if (-not (Test-Path $devShellModule)) {
    Write-Host "ERROR: Developer Shell management assembly missing: $devShellModule" -ForegroundColor Red
    exit 1
}
Import-Module $devShellModule
Enter-VsDevShell -VsInstallPath $vsPath -DevCmdArguments "-arch=x64 -host_arch=x64" -SkipAutomaticLocation
Write-Host "Dev Shell: $vsPath" -ForegroundColor Gray

# --- Locate clang-tidy ---
$clangTidy = "C:\Program Files\LLVM\bin\clang-tidy.exe"
if (-not (Test-Path $clangTidy)) {
    $cmd = Get-Command clang-tidy -ErrorAction SilentlyContinue
    if ($cmd) { $clangTidy = $cmd.Source }
}
if (-not (Test-Path $clangTidy)) {
    Write-Host "ERROR: clang-tidy not found. Install LLVM or add it to PATH." -ForegroundColor Red
    exit 1
}
Write-Host "clang-tidy: $clangTidy" -ForegroundColor Gray

# --- Locate LLVM's run-clang-tidy next to clang-tidy.exe (ships without .py on Windows) ---
$llvmBinDir = Split-Path $clangTidy -Parent
$runner = $null
foreach ($candidate in @("run-clang-tidy", "run-clang-tidy.py")) {
    $candidatePath = Join-Path $llvmBinDir $candidate
    if (Test-Path $candidatePath) {
        $runner = $candidatePath
        break
    }
}
if (-not $runner) {
    Write-Host "ERROR: run-clang-tidy not found next to clang-tidy.exe: $llvmBinDir" -ForegroundColor Red
    exit 1
}

# --- Locate Python (run-clang-tidy is a Python script) ---
$python = $null
foreach ($candidate in @("py", "python", "python3")) {
    if (Get-Command $candidate -ErrorAction SilentlyContinue) { $python = $candidate; break }
}
if (-not $python) {
    Write-Host "ERROR: Python not found (run-clang-tidy is a Python script)." -ForegroundColor Red
    exit 1
}

# --- Verify compile DB ---
$compileDb = Join-Path $repoRoot "$BuildDir/compile_commands.json"
if (-not (Test-Path $compileDb)) {
    Write-Host "`nERROR: compile_commands.json not found at: $compileDb" -ForegroundColor Red
    Write-Host "Run scripts/build.ps1 -Preset clang-x64 to generate it." -ForegroundColor Yellow
    exit 1
}

# --- Build runner arguments ---
$rootName = Split-Path $repoRoot -Leaf
$runnerArgs = @(
    "-p", "$BuildDir"
    "-clang-tidy-binary", $clangTidy
    "-j", $Jobs.ToString()
    "-header-filter=$rootName[\\/](src|include|tests|tools)[\\/]"
    "-exclude-header-filter=external[\\/]|out[\\/]build"
    # Project TUs only: must live under src|tests|tools|include and not external/.
    "-source-filter=^(?!.*[\\/]external[\\/])(?=.*[\\/](src|tests|tools|include)[\\/]).*$"
)
if ($Quiet) { $runnerArgs += "-quiet" }
if ($Fix) { $runnerArgs += "-fix"; $runnerArgs += "-format" }

# --- Compute TU scope: positional REGEXES (slash-agnostic) for the runner ---
# The LLVM runner matches positional args as regexes against absolute compile-DB
# paths (backslash separators on Windows), so bare names match anywhere and
# forward slashes are made separator-agnostic.
if ($Changed) {
    $changedFiles = @(& git diff --name-only --relative "$Base" -- src tests tools include 2>$null)
    Write-Host "Changed TUs since $Base : $($changedFiles.Count)" -ForegroundColor Gray
    foreach ($f in $changedFiles) {
        $escaped = [regex]::Escape($f.Replace('\', '/'))
        $runnerArgs += $escaped.Replace('/', '[/\\]')
    }
} elseif ($Files) {
    foreach ($pattern in ($Files -split "," | ForEach-Object { $_.Trim() } | Where-Object { $_ })) {
        $runnerArgs += $pattern.Replace('/', '[/\\]')
    }
}

Write-Host "`nRunning clang-tidy..." -ForegroundColor Cyan
Write-Host "  Runner: $runner" -ForegroundColor Gray
Write-Host "  BuildDir: $BuildDir" -ForegroundColor Gray
Write-Host "  Jobs: $Jobs" -ForegroundColor Gray
Write-Host "  Checks: .clang-tidy (config)" -ForegroundColor Gray

$runOutput = @(& $python $runner @runnerArgs 2>&1)
$exitCode = $LASTEXITCODE
$runOutput | ForEach-Object { Write-Host $_ }

if ($exitCode -ne 0) {
    Write-Host "`nclang-tidy finished with warnings/errors (exit $exitCode)." -ForegroundColor Yellow
}
exit $exitCode
