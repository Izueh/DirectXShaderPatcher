# Glaze YAML Migration Plan — Final

## Overview

Replace `llvm::yaml` with [glaze](https://github.com/stephenberry/glaze) for YAML parsing in SM5 and SM6 recipe parsers. Goal: better validation error messages (line/column info, clear descriptions) and reduced LLVM YAML dependency.

---

## Prerequisites

### C++ Version: 17 → 23

- **Verified:** C++23 builds clean with zero errors, all 48 tests pass
- **Change:** `CMAKE_CXX_STANDARD 17` → `23` in `CMakeLists.txt`
- **Risk:** None — project uses only basic C++17 features (structured bindings, `std::any`, lambdas). No code changes needed.
- **CMake minimum:** 3.24 (project) ≥ 3.21 (glaze) — compatible

---

## Key Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| `YamlImmediateScalar` deserialization | `glz::from<YAML, YamlImmediateScalar>` + `glz::to<YAML, YamlImmediateScalar>` | Reads plain YAML scalars (strings like `frame_seed` or numbers like `0x3F000000`) and populates `.value`. Decode layer untouched. Zero LLVM dependency — pure glaze API. |
| SM6 schema location | New `src/dxp/sm6/YamlSchema.h` | Same pattern as SM5, keeps code organized |
| Enum serialization sharing | Keep SM5 and SM6 separate | SM6 uses its own types (`DxilRecipeRuleApplicationMode`, etc.) with hand-written parse tables — no shared ScalarTraits |
| BOM handling | Strip 3-byte UTF-8 BOM before `glz::read_yaml` | Glaze doesn't handle BOMs; simple pre-processing |

---

## Structs Requiring `glz::meta` vs. Automatic Reflection

All YAML structs are **public aggregates with default member initializers and snake_case field names** matching YAML keys. Glaze's automatic reflection handles most of them out of the box.

### Need `glz::meta` (9 types total)

| Type | Category | Why | What's needed |
|------|----------|-----|---------------|
| `RecipeRuleApplicationMode` | Enum | Glaze serializes enums as integers by default | `glz::meta<T>` with `keys`/`value` arrays |
| `RecipeRuleRewriteMode` | Enum | Same | `glz::meta<T>` with `keys`/`value` arrays |
| `RecipeUavKind` | Enum | Same | `glz::meta<T>` with `keys`/`value` arrays |
| `RecipeOperandIndexRepresentation` | Enum | Same | `glz::meta<T>` with `keys`/`value` arrays |
| `InterpolationMode` | Enum | Same | `glz::meta<T>` with `keys`/`value` arrays |
| `ResourceDimension` | Enum | Same | `glz::meta<T>` with `keys`/`value` arrays |
| `CbufferAccessPattern` | Enum | Same | `glz::meta<T>` with `keys`/`value` arrays |
| `SamplerMode` | Enum | Same | `glz::meta<T>` with `keys`/`value` arrays |
| `YamlImmediateScalar` | Struct | Wraps a plain scalar — needs custom deserialization | `glz::from<YAML>` + `glz::to<YAML>` specializations |

### Need NO `glz::meta` — automatic reflection (20 structs)

All work out of the box because field names match YAML keys and they are aggregate types with default initializers:

| Struct | Notes |
|--------|-------|
| `YamlMatch` | snake_case fields match YAML keys |
| `YamlInstructionMatch` | snake_case fields match YAML keys |
| `YamlComponentSelector` | Nested struct — automatic |
| `YamlOperandIndex` | snake_case fields match YAML keys |
| `YamlOperand` | snake_case fields match YAML keys |
| `YamlEmitInstruction` | snake_case fields match YAML keys |
| `YamlRule` | snake_case fields match YAML keys |
| `YamlStepCondition` | Self-referential (`std::vector<YamlStepCondition>`) — automatic |
| `YamlStepCondition::Comparison` | Nested struct — automatic |
| `YamlStep` | snake_case fields match YAML keys |
| `YamlRecipeDocument` | Top-level document — automatic |
| `YamlTextureDecl` | snake_case fields match YAML keys |
| `YamlInputDecl` | snake_case fields match YAML keys |
| `YamlOutputDecl` | snake_case fields match YAML keys |
| `YamlRawResourceDecl` | snake_case fields match YAML keys |
| `YamlStructuredResourceDecl` | snake_case fields match YAML keys |
| `YamlCBufferDecl` | snake_case fields match YAML keys |
| `YamlSamplerDecl` | snake_case fields match YAML keys |
| `YamlUavDecl` | snake_case fields match YAML keys |

---

## Enum Key Mappings (snake_case YAML → enum values)

| Enum | YAML Keys (snake_case) |
|------|----------------------|
| `RecipeRuleApplicationMode` | `first`, `last`, `match_all` |
| `RecipeRuleRewriteMode` | `none`, `replace`, `before`, `after`, `replace_range` |
| `RecipeUavKind` | `typed`, `raw`, `structured` |
| `RecipeOperandIndexRepresentation` | `immediate32`, `immediate64`, `relative`, `immediate32_plus_relative`, `immediate64_plus_relative` |
| `InterpolationMode` | `undefined`, `constant`, `linear`, `linear_centroid`, `linear_noperspective`, `linear_noperspective_centroid`, `linear_sample`, `linear_noperspective_sample` |
| `ResourceDimension` | `texture_1d`, `texture_2d`, `texture_2dms`, `texture_cube`, `texture_3d`, `texture_2d_array`, `texture_2dms_array`, `texture_cube_array` |
| `CbufferAccessPattern` | `immediate_indexed`, `dynamic_indexed` |
| `SamplerMode` | `default`, `comparison`, `mono` |

---

## Implementation Strategy

### Option A: Keep struct definitions, swap parsing layer (Recommended)

**Approach:**
1. Keep all `YamlMatch`, `YamlOperand`, etc. struct definitions in `RecipeParse.cpp`
2. Replace LLVM `MappingTraits` with `glz::meta<T>` in a new `YamlSchema.h`
3. Replace `llvm::yaml::Input >> document` with `glz::read_yaml(document, text)`
4. Keep all decode/compile logic unchanged

**Pros:** Minimal diff, existing logic untouched, low risk
**Cons:** Need `glz::from<YAML>` + `glz::to<YAML>` for `YamlImmediateScalar`

---

## Task Breakdown

### Phase 1: CMake — Add glaze via FetchContent + bump to C++23

- [ ] 1.1 Bump `CMAKE_CXX_STANDARD` from 17 to 23 in `CMakeLists.txt`
- [ ] 1.2 Add `FetchContent_Declare` for glaze (GIT_TAG v7.7.1) to `CMakeLists.txt`
- [ ] 1.3 Call `FetchContent_MakeAvailable(glaze)` before `add_library(dxp_lib ...)`
- [ ] 1.4 Add `target_link_libraries(dxp_lib PRIVATE glaze::glaze)`
- [ ] 1.5 Add `/Zc:preprocessor` for MSVC (required by glaze)
- [ ] 1.6 Verify build compiles before code changes

### Phase 2: SM5 — Create glaze YAML schema header

**File:** `src/dxp/sm5/YamlSchema.h`

- [ ] 2.1 Create `YamlSchema.h` — include all YAML struct definitions currently in `RecipeParse.cpp`
- [ ] 2.2 Define `glz::from<YAML, YamlImmediateScalar>` and `glz::to<YAML, YamlImmediateScalar>` for plain scalar deserialization
- [ ] 2.3 Define `glz::meta<T>` for 8 SM5 enums using `keys`/`value` arrays (snake_case YAML keys → enum values) — see enum key mappings table above
- [ ] 2.4 All 20 struct types work via automatic reflection — no `glz::meta` needed for them

### Phase 3: SM5 — Rewrite RecipeParse.cpp to use glaze

**File:** `src/dxp/sm5/RecipeParse.cpp`

- [ ] 3.1 Include `YamlSchema.h` instead of `YamlTraits.h`
- [ ] 3.2 Remove all `llvm/Support/YAMLTraits.h` includes
- [ ] 3.3 Remove all `LLVM_YAML_IS_SEQUENCE_VECTOR` declarations (glaze handles vectors automatically — all 33 macros)
- [ ] 3.4 Replace `llvm::yaml::Input input(recipeText); input >> document;` with `glz::read_yaml(document, recipeText)`
- [ ] 3.5 Replace error handling: capture `glz::error_ctx` and use `glz::format_error(ec, recipeText)` for human-readable errors
- [ ] 3.6 Add UTF-8 BOM stripping before `glz::read_yaml` (strip 3 bytes if present)
- [ ] 3.7 Keep all post-parse decode logic (DecodeOperandIndexPatterns, CompileEmitOperand, etc.) unchanged

### Phase 4: SM5 — Remove YamlTraits.h

- [ ] 4.1 Delete `YamlTraits.h` (all `ScalarTraits` replaced by `glz::meta` in `YamlSchema.h`)

### Phase 5: SM6 — Create glaze YAML schema header

**File:** `src/dxp/sm6/YamlSchema.h`

- [ ] 5.1 Create `YamlSchema.h` — move ~13 SM6 YAML struct definitions from `Parse.cpp`
- [ ] 5.2 All SM6 structs work via automatic reflection — no `glz::meta` needed for them
- [ ] 5.3 SM6 has NO enum ScalarTraits — enum parsing stays as hand-written `ParseRecipeRuleApplicationMode()` function

### Phase 6: SM6 — Rewrite Parse.cpp to use glaze

**File:** `src/dxp/sm6/Parse.cpp`

- [ ] 6.1 Include `YamlSchema.h` instead of `llvm/Support/YAMLTraits.h`
- [ ] 6.2 Remove `llvm/Support/YAMLTraits.h` include (keep `llvm/Regex.h`, `llvm/ADT/ArrayRef.h`, etc.)
- [ ] 6.3 Remove all `LLVM_YAML_IS_SEQUENCE_VECTOR` declarations
- [ ] 6.4 Replace `llvm::yaml::Input input(recipeText); input >> document;` with `glz::read_yaml(document, recipeText)`
- [ ] 6.5 Replace error handling with `glz::format_error(ec, recipeText)`
- [ ] 6.6 Add UTF-8 BOM stripping before `glz::read_yaml`

### Phase 7: Build and fix compilation

- [ ] 7.1 Run `build.ps1` and fix all compilation errors
- [ ] 7.2 Ensure no unused includes or dead code remains

### Phase 8: Run tests and fix failures

- [ ] 8.1 Run full test suite via `ctest --preset ninja-msvc-debug --output-on-failure`
- [ ] 8.2 Fix any test failures caused by parsing behavior changes
- [ ] 8.3 Verify error messages are improved (better line/column info, clearer descriptions)
- [ ] 8.4 Confirm all ~48 tests pass

---

## Files to Change (Estimated)

| File | Change | Risk |
|------|--------|------|
| `CMakeLists.txt` | Bump to C++23, add FetchContent for glaze + link | Low |
| `src/dxp/sm5/YamlSchema.h` | **New file** — all SM5 YAML structs + 8 enum `glz::meta` + `YamlImmediateScalar` custom | Medium |
| `src/dxp/sm5/RecipeParse.cpp` | Replace includes, YAML parsing call, MappingTraits | Medium |
| `src/dxp/sm5/YamlTraits.h` | **Delete** — replaced by YamlSchema.h | Low |
| `src/dxp/sm6/YamlSchema.h` | **New file** — all SM6 YAML structs (automatic reflection) | Medium |
| `src/dxp/sm6/Parse.cpp` | Remove llvm::yaml includes, replace parsing call | Low |

---

## Notes

- **Backward compatibility is critical** — all `.recipe.yml` files must parse identically
- **Post-parse decode logic is untouched** — the complex `DecodeOperandIndexPatterns`, `CompileEmitOperand`, etc. remain exactly as-is
- **YAML field names must not change** — existing `.recipe.yml` files must continue to parse
- **`YamlImmediateScalar` uses `glz::from<YAML>` / `glz::to<YAML>`** — transparent scalar wrapper, decode layer `.value` access unchanged
- **Glaze is header-only** — no library linking required beyond `glaze::glaze` target
- **Tests use `Contains()` for error messages** — partial matching means improved error messages won't break tests
- **SM6 uses its own types** — `DxilRecipeRuleApplicationMode` etc. are separate from SM5 types; no shared enum serialization needed
- **All `LLVM_YAML_IS_SEQUENCE_VECTOR` macros (33 total) can be deleted** — glaze handles vectors automatically
