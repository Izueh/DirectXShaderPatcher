$projectDir = 'C:\Users\izueh\source\repos\DirectXShaderPatcher'
Import-Module 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
Enter-VsDevShell -VsInstallPath 'C:\Program Files\Microsoft Visual Studio\2022\Community' -Arch amd64
cd $projectDir

cmake --preset ninja-msvc-debug --fresh
if ($LASTEXITCODE -ne 0) { Write-Error "Configure failed"; exit 1 }

cmake --build --preset ninja-msvc-debug --target dxp_lib
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed"; exit 1 }

cmake --build --preset ninja-msvc-debug --target sm5_recipe_parse_validation_test
if ($LASTEXITCODE -ne 0) { Write-Error "Test build failed"; exit 1 }

Write-Host "Build succeeded."
