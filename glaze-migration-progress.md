# Glaze YAML Migration — Progress Report

## Status: ALL PHASES COMPLETE ✅

**Date:** 2026-06-06  
**Tests:** 48/48 passing  
**Build:** Clean (C++23, MSVC, Ninja)

---

## Completed Work

### Phase 1: CMake — glaze FetchContent + C++23 bump ✅

- [x] Bumped `CMAKE_CXX_STANDARD` from 17 to 23
- [x] Added `FetchContent_Declare` for glaze v7.7.1
- [x] Added `FetchContent_MakeAvailable(glaze)` before `add_library(dxp_lib ...)`
- [x] Linked `glaze::glaze` to `dxp_lib`
- [x] Added `/Zc:preprocessor` for MSVC
- [x] Verified build compiles with zero errors

### Phase 2: SM5 — glaze YAML schema header ✅

**File:** `src/dxp/sm5/YamlSchema.h` (new, ~340 lines)

- [x] Moved all 20 YAML struct definitions from `RecipeParse.cpp`
- [x] Defined `glz::meta<YamlImmediateScalar>` using `glz::custom<read_fn, write_fn>` for plain scalar deserialization
  - Read function: `std::string` — captures both strings (`frame_seed`) and numbers (`0x3F000000`)
  - Write function: returns `const std::string&` — serializes as plain YAML scalar
- [x] Defined `glz::meta<T>` for 8 SM5 enums using `keys`/`value` arrays (snake_case → enum values)
- [x] Defined `glz::meta<YamlStepCondition>` — maps `"not"` → `not_condition`
- [x] Defined `glz::meta<YamlStep>` — maps `"if"` → `if_condition`
- [x] All other 17 structs work via automatic reflection (no `glz::meta` needed)

### Phase 3: SM5 — Rewrite RecipeParse.cpp to use glaze ✅

**File:** `src/dxp/sm5/RecipeParse.cpp`

- [x] Replaced `#include "YamlTraits.h"` with `#include "YamlSchema.h"`
- [x] Removed all `llvm/Support/YAMLTraits.h` includes
- [x] Removed all `LLVM_YAML_IS_SEQUENCE_VECTOR` declarations (33 macros)
- [x] Removed all `MappingTraits<...>` definitions (~20 templates)
- [x] Removed `ScalarTraits<YamlImmediateScalar>` definition
- [x] Replaced `llvm::yaml::Input` parsing with `glz::read_yaml(document, text)`
- [x] Replaced error handling: `glz::format_error(ec, text)` for human-readable errors
- [x] Added UTF-8 BOM stripping (`StripBom` function)
- [x] All post-parse decode logic unchanged (verified by passing tests)

### Phase 4: SM5 — Remove YamlTraits.h ✅

- [x] Deleted `src/dxp/sm5/YamlTraits.h` (all `ScalarTraits` replaced by `glz::meta`)

### Phase 5: SM6 — glaze YAML schema header ✅

**File:** `src/dxp/sm6/YamlSchema.h` (new, ~160 lines)

- [x] Moved all 13 SM6 YAML struct definitions from `Parse.cpp`
- [x] All structs work via automatic reflection (no `glz::meta` needed)
- [x] Defined `glz::meta<YamlRecipeStepModel>` — maps `"if"` → `if_condition`
- [x] SM6 has NO enum ScalarTraits — enum parsing stays as hand-written tables

### Phase 6: SM6 — Rewrite Parse.cpp to use glaze ✅

**File:** `src/dxp/sm6/Parse.cpp`

- [x] Added `#include "YamlSchema.h"` and `#include <glaze/yaml.hpp>`
- [x] Removed `llvm/Support/YAMLTraits.h` include
- [x] Removed all `LLVM_YAML_IS_SEQUENCE_VECTOR` declarations (13 macros)
- [x] Removed all `MappingTraits<...>` definitions (~13 templates)
- [x] Replaced `llvm::yaml::Input` parsing with `glz::read_yaml(document, text)`
- [x] Replaced error handling with `glz::format_error(ec, text)`
- [x] Added UTF-8 BOM stripping (`StripBom` function)
- [x] Moved `BuildStepCondition` function into correct anonymous namespace

### Phase 7: Build and fix compilation ✅

- [x] Full build succeeds with zero errors
- [x] No unused includes or dead code

### Phase 8: Run tests and fix failures ✅

- [x] All 48 tests pass
- [x] Error messages now include glaze's line/column info

---

## Key Implementation Details

### `YamlImmediateScalar` — `glz::custom` approach

```cpp
template <>
struct meta<YamlImmediateScalar> {
  using T = YamlImmediateScalar;
  static constexpr auto value = glz::custom<
    [](T& val, std::string sv) { val.value = std::move(sv); },
    [](const T& val) -> const std::string& { return val.value; }
  >;
};
```

This lets glaze treat `YamlImmediateScalar` as a transparent scalar wrapper. YAML like `immediates_u32: [frame_seed]` or `immediates_u32: [0x3F000000]` deserializes correctly into `YamlImmediateScalar{.value = "frame_seed"}` or `YamlImmediateScalar{.value = "0x3F000000"}`.

### Structs needing `glz::meta` vs. automatic reflection

| Category | Count | Why |
|----------|-------|-----|
| **Need `glz::meta`** | 11 | 8 enums (snake_case keys) + 3 structs (`YamlStepCondition`, `YamlStep`, `YamlRecipeStepModel` with `"if"` key) |
| **Automatic reflection** | 27 | All other structs — snake_case field names match YAML keys exactly |

### Removed LLVM YAML infrastructure

| Item | SM5 count | SM6 count |
|------|-----------|-----------|
| `LLVM_YAML_IS_SEQUENCE_VECTOR` macros | 20 | 13 |
| `MappingTraits<...>` templates | 20 | 13 |
| `ScalarTraits<...>` templates | 9 | 0 |
| **Total removed** | **49** | **26** |

---

## Files Changed

| File | Action | Lines Changed |
|------|--------|---------------|
| `CMakeLists.txt` | Modified | +10, -1 |
| `src/dxp/sm5/YamlSchema.h` | **New** | ~340 |
| `src/dxp/sm5/RecipeParse.cpp` | Modified | -400 (removed), ~30 (added) |
| `src/dxp/sm5/YamlTraits.h` | **Deleted** | -300 |
| `src/dxp/sm6/YamlSchema.h` | **New** | ~160 |
| `src/dxp/sm6/Parse.cpp` | Modified | -300 (removed), ~50 (added) |

---

## Remaining Work (Optional)

### Phase 6: Update public API (not started)

- [ ] Review `RecipeParseResult` / `DxilRecipeParseResult` — consider adding `glz::error_ctx`-style structured errors (line, column, path)
- [ ] Update `RecipeParse.h` and `RecipeParse.h` (SM6) header comments if API changes
- [ ] Update CLI `dxp.cpp` error output to show structured error details

---

## Verification

```bash
# Build
powershell.exe -ExecutionPolicy Bypass -File ./build.ps1
# Result: Clean build, zero errors

# Tests
ctest --preset ninja-msvc-debug --output-on-failure
# Result: 100% tests passed, 0 tests failed out of 48
```
