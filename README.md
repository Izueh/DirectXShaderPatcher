# DirectXShaderPatcher

DirectXShaderPatcher is a shader patching library and CLI for applying declarative and programmatic edits to DXIL and SM5 shader containers.

## Build

Build on Windows from a Visual Studio Developer PowerShell so the MSVC toolchain environment is loaded.

```powershell
cmake --preset ninja-msvc-debug
cmake --build --preset ninja-msvc-debug
ctest --preset ninja-msvc-debug --output-on-failure
```

## Schemas

[SM5](docs/sm5_recipe_schema.md)\
[SM6](docs/sm6_recipe_schema.md)
