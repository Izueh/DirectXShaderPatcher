[CmdletBinding()]
param(
    [ValidateSet("debug", "release")]
    [string]$Mode = "debug",
    [switch]$RunTests,
    [switch]$RunBenchmarks,
    [switch]$Pack
)

function Find-VsInstallPath {
    $instances = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -property installationPath
    if ($instances) {
        return $instances.Trim()
    }
    Write-Error "Could not locate Visual Studio 2022 installation."
    exit 1
}

$vsInstallPath = Find-VsInstallPath
$devShellPath = Join-Path $vsInstallPath 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
Import-Module $devShellPath
Enter-VsDevShell -VsInstallPath $vsInstallPath -Arch amd64
Set-Location $PSScriptRoot

$preset = "ninja-msvc-${Mode}"

if ($RunBenchmarks) {
    $preset = "ninja-msvc-release-bench"
}

cmake --preset $preset
if ($LASTEXITCODE -ne 0) { Write-Error "Configure failed"; exit 1 }

cmake --build --preset $preset --parallel
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed"; exit 1 }

if ($RunTests) {
    ctest --preset $preset --output-on-failure
    if ($LASTEXITCODE -ne 0) { Write-Error "Tests failed"; exit 1 }
}

if ($RunBenchmarks) {
    $benchExe = "out/build/${preset}/sm5_benchmarks.exe"
    if (-not (Test-Path $benchExe)) {
        Write-Error "Benchmark executable not found: $benchExe"
        exit 1
    }
    Write-Host "Running SM5 benchmarks..." -ForegroundColor Cyan
    & $benchExe --benchmark_min_time=0.01s
    if ($LASTEXITCODE -ne 0) { Write-Error "Benchmarks failed"; exit 1 }
}

if ($Pack) {
    $staging = "out/build/${preset}/sdk-staging"
    if (Test-Path $staging) { Remove-Item -Recurse -Force $staging }
    New-Item -ItemType Directory -Path $staging | Out-Null
    cmake --install "out/build/${preset}" --component DirectXShaderPatcherSDK --prefix "$staging"
    cmake --install "out/build/${preset}" --component DirectXShaderPatcherCLI --prefix "$staging"
    if (Test-Path "${staging}/DirectXShaderPatcher_SDK") {
        Move-Item -Path "${staging}/DirectXShaderPatcher_SDK" -Destination "${staging}/DirectXShaderPatcher"
    }
    $zipPath = "out/build/${preset}/DirectXShaderPatcher-msvc-${Mode}.zip"
    Compress-Archive -Path "${staging}/*" -DestinationPath $zipPath -Force
    Write-Host "Package created: $zipPath" -ForegroundColor Green
}
