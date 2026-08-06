param (
    [Parameter(Mandatory = $false)]
    [ValidateSet("ninja-x64", "ninja-x86", "clang-x64", "clang-x86")]
    [string]$Preset = "ninja-x64",

    [Parameter(Mandatory = $false)]
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string]$Config = "Release",

    [Parameter(Mandatory = $false)]
    [string]$Jobs = "8",

    [Parameter(Mandatory = $false)]
    [switch]$Clean,

    [Parameter(Mandatory = $false)]
    [switch]$PartialClean,

    [Parameter(Mandatory = $false)]
    [switch]$Tests,

    [Parameter(Mandatory = $false)]
    [string]$ExtraArgs = ""
)

$ErrorActionPreference = "Stop"

# ─── Helpers ───────────────────────────────────────────────────────────────────

function Test-CommandSuccess {
    param([int]$ExitCode, [string]$PhaseName, [string]$Command)
    if ($ExitCode -ne 0) {
        Write-Host "`nERROR: $PhaseName failed (exit code $ExitCode)." -ForegroundColor Red
        Write-Host "Command: $Command" -ForegroundColor Yellow
        Write-Host "Fix the above errors and re-run the build." -ForegroundColor Yellow
        exit $ExitCode
    }
}

# ─── 1. Locate Visual Studio ──────────────────────────────────────────────────

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Error "Error: vswhere.exe not found. Visual Studio must be installed."
}

Write-Host "[1/6] Locating Visual Studio installation..." -ForegroundColor Cyan
$vsPath = & $vswhere -latest -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) {
    Write-Error "Error: No matching Visual Studio installation detected."
}
Write-Host "Found: $vsPath" -ForegroundColor Gray

# ─── 2. Initialize Developer Shell ────────────────────────────────────────────

Write-Host "[2/6] Initializing Developer Shell Environment..." -ForegroundColor Cyan
$devShellModule = Join-Path $vsPath "Common7\Tools\Microsoft.VisualStudio.DevShell.dll"

if (-not (Test-Path $devShellModule)) {
    Write-Error "Error: Developer Shell management assembly is missing."
}

Import-Module $devShellModule
$arch = if ($Preset -eq "ninja-x86" -or $Preset -eq "clang-x86") { "x86" } else { "x64" }
Enter-VsDevShell -VsInstallPath $vsPath -DevCmdArguments "-arch=$arch -host_arch=x64" -SkipAutomaticLocation

# ─── 3. Optional pre-build cleaning ───────────────────────────────────────────

if ($Clean) {
    Write-Host "[3/6] Performing full clean (removing out/ directory)..." -ForegroundColor Yellow
    if (Test-Path "out") { Remove-Item -Recurse -Force "out" }
} elseif ($PartialClean) {
    Write-Host "[3/6] Performing partial clean (preserving DXC/LLVM build cache)..." -ForegroundColor Yellow
    $buildDir = Join-Path (Get-Location) "out"
    if (Test-Path $buildDir) {
        # Remove DXP build artifacts but preserve the dxc/ subtree
        Get-ChildItem -Path $buildDir -Directory | Where-Object { $_.Name -like "$Preset*" } | ForEach-Object {
            $configDir = Join-Path $_.FullName "$Config"
            if (Test-Path $configDir) {
                # Remove object files, libraries, executables, and PDBs
                Get-ChildItem -Path $configDir -Recurse -File | Where-Object {
                    $_.Extension -in @(".obj", ".lib", ".exe", ".dll", ".pdb", ".exp", ".ilk", ".idb", ".pch", ".o")
                } | Remove-Item -Force
                Write-Host "  Cleaned: $configDir" -ForegroundColor Gray
            }
        }
    } else {
        Write-Host "  Nothing to clean (out/ does not exist)." -ForegroundColor Gray
    }
} else {
    Write-Host "[3/6] Skipping clean phase..." -ForegroundColor Gray
}

# ─── 4. CMake Configure ───────────────────────────────────────────────────────

Write-Host "[4/6] Configuring CMake with Preset: '$Preset' [$Config]..." -ForegroundColor Cyan
$configCmd = "cmake --preset $Preset $ExtraArgs"
Write-Host "Running: $configCmd" -ForegroundColor Gray
Invoke-Expression $configCmd
$configureExit = $LASTEXITCODE
Test-CommandSuccess $configureExit "CMake configure" $configCmd

# ─── 5. CMake Build ───────────────────────────────────────────────────────────

Write-Host "[5/6] Building CMake with Preset: '$Preset' [$Config]..." -ForegroundColor Cyan
$buildConfig = $Config.ToLower()
$buildCmd = "cmake --build --preset $Preset-$buildConfig --parallel $Jobs"
Write-Host "Running: $buildCmd" -ForegroundColor Gray
Invoke-Expression $buildCmd
$buildExit = $LASTEXITCODE
Test-CommandSuccess $buildExit "CMake build" $buildCmd

# ─── 6. Tests (optional) ──────────────────────────────────────────────────────

if ($Tests) {
    Write-Host "[6/6] Running tests..." -ForegroundColor Cyan
    $testConfig = $Config.ToLower()
    $testCmd = "ctest --preset $Preset-$testConfig --output-on-failure"
    Write-Host "Running: $testCmd" -ForegroundColor Gray
    Invoke-Expression $testCmd
    $testExit = $LASTEXITCODE
    Test-CommandSuccess $testExit "ctest" $testCmd
    Write-Host "All tests passed!" -ForegroundColor Green
}

Write-Host "`nBuild pipeline completed successfully!" -ForegroundColor Green
