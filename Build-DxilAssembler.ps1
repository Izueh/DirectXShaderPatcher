param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [ValidateSet("x64", "Win32")]
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildRoot = Join-Path $repoRoot "build\$Platform-$Configuration"
$dxcPreset = Join-Path $repoRoot "external\DirectXShaderCompiler\cmake\caches\PredefinedParams.cmake"

Push-Location $repoRoot
try {
    git submodule update --init --recursive

    $cmakeArgs = @(
        "-C", $dxcPreset,
        "-S", $repoRoot,
        "-B", $buildRoot,
        "-G", "Visual Studio 17 2022",
        "-A", $(if ($Platform -eq "Win32") { "Win32" } else { "x64" })
    )

    & cmake @cmakeArgs
    & cmake --build $buildRoot --config $Configuration --target dxil_patch_tool
}
finally {
    Pop-Location
}