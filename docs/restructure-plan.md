# Restructure Plan

## Goal

Split the current monolithic DXIL patcher implementation into a conventional library layout without changing behavior. The immediate objective is not to redesign features. It is to make the SM6 code easier to evolve and test without prematurely constraining a future SM5 library.

## Shader Model Scope

The current split is intentionally SM6-specific.

- `src/dxp/sm6` is an implementation area for DXIL-based patching only.
- A future SM5 implementation should be introduced as a separate library in the same repository, not as another backend folded into the current SM6 internals.
- Shared abstractions should be introduced only after concrete overlap appears in both implementations.

## DXC Alignment

We should align with DirectXShaderCompiler selectively.

- Match DXC on terminology that directly maps to DXIL and LLVM concepts.
- Match DXC on contributor-facing conventions when they reduce friction: `out/build`, LLVM-based formatting, and familiar target naming.
- Avoid copying DXC's internal directory structure or subsystem layering unless a concrete patcher requirement justifies it.
- Keep patch/recipe abstractions project-specific. They are the product surface of this repository, not DXC's.

This strikes the right balance: easy comparison against DXC internals where needed, without turning this repository into a partial mirror that is costly to maintain.

## Proposed Layout

```text
include/
  DirectXShaderPatcher.h
src/
  DirectXShaderPatcher.cpp
  dxp/
    Container.cpp
    MutateResources.cpp
    Match.cpp
    Rewrite.cpp
    RecipeModel.cpp
    RecipeExecute.cpp
    RecipeParseYaml.cpp
tools/
  dxp.cpp
test/
  support/
examples/
docs/
```

## Extraction Order

1. Establish a stable public facade under `include/` and `src/`.
2. Move container load/serialize utilities into one implementation unit.
3. Move resource addition and binding helpers into one implementation unit.
4. Move match and rewrite logic into separate implementation units.
5. Move recipe model and execution into separate implementation units.
6. Move YAML parsing last, because it depends on the recipe model and was the most regression-prone area in the abandoned refactor.
7. Refactor test helpers into shared support files as public and internal seams become clearer.

## Guardrails

- Preserve current behavior at each step.
- Prefer one narrow move plus one narrow validation over large file churn.
- Keep new public headers small. Most extracted headers should remain internal until the API is intentionally designed.
- Defer broad renames until after the code is split enough that names reflect stable responsibilities.