[CmdletBinding()]
param(
    [switch]$RunTests
)

$projectDir = 'C:\Users\izueh\source\repos\DirectXShaderPatcher'
Import-Module 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
Enter-VsDevShell -VsInstallPath 'C:\Program Files\Microsoft Visual Studio\2022\Community' -Arch amd64
cd $projectDir

cmake --preset ninja-msvc-debug
if ($LASTEXITCODE -ne 0) { Write-Error "Configure failed"; exit 1 }

cmake --build --preset ninja-msvc-debug --parallel
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed"; exit 1 }

if ($RunTests) {
    ctest --preset ninja-msvc-debug --output-on-failure
    if ($LASTEXITCODE -ne 0) { Write-Error "Tests failed"; exit 1 }
}
