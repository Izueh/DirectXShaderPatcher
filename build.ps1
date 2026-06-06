[CmdletBinding()]
param(
    [switch]$RunTests,
    [switch]$Pack
)

Import-Module 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
Enter-VsDevShell -VsInstallPath 'C:\Program Files\Microsoft Visual Studio\2022\Community' -Arch amd64
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
