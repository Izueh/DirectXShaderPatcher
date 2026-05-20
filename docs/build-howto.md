# Build How-To

## Working build path

This repo builds correctly on Windows with the `x64-debug` CMake preset, but the preset assumes an MSVC developer environment is already loaded.

The preset in [CMakePresets.json](C:/Users/izueh/source/repos/DirectXShaderPatcher/CMakePresets.json) uses:

- `generator: Ninja`
- `CMAKE_C_COMPILER: cl`
- `CMAKE_CXX_COMPILER: cl`

That means plain PowerShell is not enough by itself. `cl` must come from a Visual Studio developer shell so the standard library include paths and linker environment are populated.

## Recommended commands

From Visual Studio 2022 Developer PowerShell:

```powershell
Set-Location C:\Users\izueh\source\repos\DirectXShaderPatcher
cmake --preset x64-debug
cmake --build --preset x64-debug
ctest --test-dir out/build/x64-debug --output-on-failure
```

To build a single target:

```powershell
cmake --build --preset x64-debug --target dxil_patch_declarative_emit_resource_recipe_test
```

To run a filtered test slice:

```powershell
ctest --test-dir out/build/x64-debug -R "declarative_emit_resource_recipe_0x56C468C3" --output-on-failure
```

## If you start from plain PowerShell

Load the Visual Studio developer environment first, then use the preset commands above:

```powershell
$vsPath = 'C:\Program Files\Microsoft Visual Studio\2022\Community'
$devShell = Join-Path $vsPath 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
Import-Module $devShell
Enter-VsDevShell -VsInstallPath $vsPath -DevCmdArguments '-arch=x64 -host_arch=x64'
Set-Location C:\Users\izueh\source\repos\DirectXShaderPatcher
cmake --preset x64-debug
cmake --build --preset x64-debug
```

## Failure signature

If the environment is wrong, builds fail very early with missing standard headers, for example:

- `fatal error C1083: Cannot open include file: 'memory': No such file or directory`
- `fatal error C1083: Cannot open include file: 'cstddef': No such file or directory`
- `fatal error C1083: Cannot open include file: 'cstdint': No such file or directory`
- `fatal error C1083: Cannot open include file: 'any': No such file or directory`

Those errors do not mean the repo or preset is broken. They mean the `cl` toolchain environment is not active.

## VS Code note

If using CMake Tools in VS Code, make sure the active configure preset is `x64-debug` before building or running tests. Without an active preset, CMake Tools cannot build or discover tests for this workspace.