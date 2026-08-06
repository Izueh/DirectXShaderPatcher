# DirectXShaderPatcher

Declarative shader patching API for **DXBC (SM5)** and **DXIL (SM6)**
shader containers. 

Meant for patching shaders live during game runtime using patterns instead of manipulating bytecode manually.

## Features

- Support for adding new resources (SRs, Inputs, Outputs).
- Extracting information from operands (cbuffers, constants).
- Matching and rewriting patterns in the bytecode.

## How To Use
Recipes are the entry point of the API they are sequence of steps, steps are how users declare what the engine will do with the shader.

Main steps for building recipes are:
- `ApplyRule` — match, rewrite or extract information from a shader.
- `AddResource` — declare textures, raw/structured resources, constant
  buffers, samplers, UAVs, input/output signatures, and temps.

Recipes can be declare trough YAML or directly trough C++ API.

Steps support conditional execution trough a condition engine.

## Building

Pre-Requisites

- Visual Studio 2026 (MSVC v145)
- CMake 3.27+
- Ninja
- LLVM 20+ (for clang builds)

`scripts/build.ps1` sets up the VS Dev Shell environment it wraps cmake commands it's a convenient wrapper for building. 

## Tools

### dxp

The `dxp` tool validates recipes and patches shaders:

```
dxp sm5 patch <recipe.recipe.yml> <input.cso> [output.cso] [--log-level <level>]
dxp sm5 validate <recipe.recipe.yml>
dxp sm6 patch <recipe.recipe.yml> <input.cso> [output.cso] [--log-level <level>]
dxp sm6 validate <recipe.recipe.yml>
```

Run `dxp --help` for the full usage reference.

## Documentation

- [SM5 recipe schema](docs/sm5_recipe_schema.md)
- [SM6 recipe schema](docs/sm6_recipe_schema.md)

