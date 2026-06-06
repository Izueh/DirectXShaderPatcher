[CmdletBinding()]
param(
    [switch]$RunTests,
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

cmake --preset ninja-msvc-debug
if ($LASTEXITCODE -ne 0) { Write-Error "Configure failed"; exit 1 }

cmake --build --preset ninja-msvc-debug --parallel
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed"; exit 1 }

if ($RunTests) {
    ctest --preset ninja-msvc-debug --output-on-failure
    if ($LASTEXITCODE -ne 0) { Write-Error "Tests failed"; exit 1 }
}

if ($Pack) {
    $staging = "out/build/ninja-msvc-debug/sdk-staging"
    if (Test-Path $staging) { Remove-Item -Recurse -Force $staging }
    New-Item -ItemType Directory -Path $staging | Out-Null
    cmake --install "out/build/ninja-msvc-debug" --component DirectXShaderPatcherSDK --prefix "$staging"
    cmake --install "out/build/ninja-msvc-debug" --component DirectXShaderPatcherCLI --prefix "$staging"
    if (Test-Path "${staging}/DirectXShaderPatcher_SDK") {
        Move-Item -Path "${staging}/DirectXShaderPatcher_SDK" -Destination "${staging}/DirectXShaderPatcher"
    }
    $zipPath = "out/build/ninja-msvc-debug/DirectXShaderPatcher-msvc-debug.zip"
    Compress-Archive -Path "${staging}/*" -DestinationPath $zipPath -Force
    Write-Host "Package created: $zipPath" -ForegroundColor Green
}
