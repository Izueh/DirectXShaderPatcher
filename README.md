# DirectXShaderPatcher

DirectXShaderPatcher is a shader patching library and CLI for applying declarative and programmatic edits to DXIL and SM5 shader containers.

## Build

Build on Windows from a Visual Studio Developer PowerShell so the MSVC toolchain environment is loaded.

```powershell
cmake --preset ninja-msvc-debug
cmake --build --preset ninja-msvc-debug
ctest --preset ninja-msvc-debug --output-on-failure
```

## CLI Usage

The `dxp` CLI is built from `tools/dxp.cpp` and supports SM5/SM6 recipe validation and patching.

```powershell
# Validate a recipe
dxp sm5 validate recipes/sm5_recipe.yml
dxp sm6 validate recipes/sm6_recipe.yml

# Patch a shader with a recipe
dxp sm5 patch recipe.recipe.yml input.cso output.cso
dxp sm6 patch recipe.recipe.yml input.cso output.cso

# Emit per-step diagnostics while applying a recipe
dxp sm5 patch recipe.recipe.yml input.cso output.cso --trace
dxp sm6 patch recipe.recipe.yml input.cso output.cso --trace
```

## Schemas

[SM5](docs/sm5_recipe_schema.md)\
[SM6](docs/sm6_recipe_schema.md)
