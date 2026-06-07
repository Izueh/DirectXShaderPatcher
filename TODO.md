# TODO: Migrate from llvm::yaml to glaze

## Goal
Replace llvm::yaml with glaze for YAML parsing in both SM5 and SM6 recipe parsers, improving validation error messages and reducing LLVM dependencies.

## Status: COMPLETE ✅

All 48 tests pass. Full migration complete.

---

## Completed Tasks

### 1. CMake: Add glaze via FetchContent ✅
- [x] 1.1 Bump `CMAKE_CXX_STANDARD` from 17 to 23
- [x] 1.2 Add `FetchContent_Declare` for glaze (GIT_TAG v7.7.1) to `CMakeLists.txt`
- [x] 1.3 Call `FetchContent_MakeAvailable(glaze)` before `add_library(dxp_lib ...)`
- [x] 1.4 Add `target_link_libraries(dxp_lib PRIVATE glaze::glaze)`
- [x] 1.5 Add `/Zc:preprocessor` for MSVC
- [x] 1.6 Verify build compiles before code changes

### 2. SM5: Create glaze YAML schema header ✅
- [x] 2.1 Create `src/dxp/sm5/YamlSchema.h` — moved all YAML struct definitions from `RecipeParse.cpp`
- [x] 2.2 Define `glz::meta<YamlImmediateScalar>` using `glz::custom<read_fn, write_fn>` for plain scalar deserialization
- [x] 2.3 Define `glz::meta<T>` for 8 SM5 enums using `keys`/`value` arrays (snake_case YAML keys → enum values)
- [x] 2.4 Define `glz::meta<YamlStepCondition>` — maps `"not"` → `not_condition`
- [x] 2.5 Define `glz::meta<YamlStep>` — maps `"if"` → `if_condition`
- [x] 2.6 All 17 other structs work via automatic reflection (no `glz::meta` needed)
- [x] 2.7 Keep YAML field names identical to current schema (backward compatibility)

### 3. SM5: Rewrite RecipeParse.cpp to use glaze ✅
- [x] 3.1 Remove all `llvm/Support/YAMLTraits.h` includes
- [x] 3.2 Remove all `LLVM_YAML_IS_SEQUENCE_VECTOR` declarations (20 macros)
- [x] 3.3 Replace `llvm::yaml::Input` parsing with `glz::read_yaml(document, text)`
- [x] 3.4 Replace `YamlTraits.h` enum serialization with glaze equivalents
- [x] 3.5 Improve error reporting: capture `glz::error_ctx` and use `glz::format_error(ec, text)` for human-readable errors
- [x] 3.6 Add UTF-8 BOM stripping before `glz::read_yaml`
- [x] 3.7 Keep the post-parse decode logic (DecodeOperandIndexPatterns, CompileEmitOperand, etc.) unchanged

### 4. SM5: Remove YamlTraits.h ✅
- [x] 4.1 Delete `src/dxp/sm5/YamlTraits.h` (all `ScalarTraits` replaced by `glz::meta` in `YamlSchema.h`)

### 5. SM6: Remove llvm::yaml dependency from Parse.cpp ✅
- [x] 5.1 Create `src/dxp/sm6/YamlSchema.h` — moved ~13 SM6 YAML struct definitions from `Parse.cpp`
- [x] 5.2 All SM6 structs work via automatic reflection (no `glz::meta` needed)
- [x] 5.3 Define `glz::meta<YamlRecipeStepModel>` — maps `"if"` → `if_condition`
- [x] 5.4 Remove `llvm/Support/YAMLTraits.h` include (keep `llvm/Regex.h`, `llvm/ADT/ArrayRef.h`)
- [x] 5.5 Remove all `LLVM_YAML_IS_SEQUENCE_VECTOR` declarations (13 macros)
- [x] 5.6 Remove all `MappingTraits<...>` definitions (~13 templates)
- [x] 5.7 Replace `llvm::yaml::Input` parsing with `glz::read_yaml(document, text)`
- [x] 5.8 Replace error handling with `glz::format_error(ec, text)`
- [x] 5.9 Add UTF-8 BOM stripping before `glz::read_yaml`

### 6. Build and fix compilation ✅
- [x] 6.1 Run `build.ps1` — clean build, zero errors
- [x] 6.2 Ensure no unused includes or dead code remains

### 7. Run tests and fix failures ✅
- [x] 7.1 Run full test suite via `ctest --preset ninja-msvc-debug --output-on-failure`
- [x] 7.2 Fix SM5 `YamlStepCondition` and `YamlStep` `"if"` key mapping
- [x] 7.3 Fix SM6 `YamlRecipeStepModel` `"if"` key mapping
- [x] 7.4 Verify error messages are improved (glaze produces line/column info)
- [x] 7.5 Confirm all 48 tests pass

---

## Remaining Work (Optional)

### 8. Update public API — Structured parse errors ✅
- [x] 8.1 Create `include/dxp/ParseError.h` with `ParseError` struct (line, column, path, message)
- [x] 8.2 Update `RecipeParseResult` (uses `ParseError Error`) and `DxilRecipeParseResult` (uses `::dxp::ParseError yaml_diagnostic`) to use `ParseError` instead of `std::string`
- [x] 8.3 Update SM5 `RecipeParse.cpp` to populate structured error fields from glaze `error_ctx`
- [x] 8.4 Update SM6 `Parse.cpp` to populate structured error fields from glaze `error_ctx`
- [x] 8.5 Update CLI `dxp.cpp` to display structured error details via `ParseError::format(sourceName)`
- [x] 8.6 Run full test suite — 48/48 tests pass

---

## Summary

### What changed
- **C++17 → C++23** (verified compatible, zero code changes needed)
- **Added glaze v7.7.1** via FetchContent (header-only, no library linking beyond `glaze::glaze` target)
- **Removed ~75 lines** of LLVM YAML infrastructure (49 SM5 + 26 SM6 macros/templates)
- **Added ~500 lines** of glaze schema (2 new files: `YamlSchema.h` for SM5 and SM6)
- **Deleted 1 file:** `YamlTraits.h`

### Key decisions
- `YamlImmediateScalar` uses `glz::custom` — transparent scalar wrapper, decode layer `.value` access unchanged
- All struct field names preserved (snake_case YAML keys match C++ member names)
- Enum serialization uses `glz::meta<T>` with `keys`/`value` arrays (snake_case YAML strings → enum values)
- SM5 and SM6 kept separate (SM6 uses its own types with hand-written parse tables)

### Verification
```bash
# Build
powershell.exe -ExecutionPolicy Bypass -File ./build.ps1
# Result: Clean build, zero errors

# Tests
ctest --preset ninja-msvc-debug --output-on-failure
# Result: 100% tests passed, 0 tests failed out of 48
```
