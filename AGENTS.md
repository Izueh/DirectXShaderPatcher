# DirectXShaderPatcher — AGENTS.md

## Project Overview

DirectXShaderPatcher (DXP) is a shader patching library and CLI for applying declarative and programmatic edits to DXIL (SM6) and SM5 shader containers. It parses compiled shader binaries (`.cso`), matches instruction patterns, and rewrites them according to YAML recipe definitions.

## Build and Test

### Prerequisites

- Visual Studio 2022 Community
- CMake (must be on PATH)
- Ninja (must be on PATH)
- PowerShell (`powershell.exe`)

### Build

Use the provided `build.ps1` script — it bootstraps the MSVC toolchain via `Enter-VsDevShell` (the only reliable method for CMake presets):

```bash
# Build (no tests)
powershell.exe -ExecutionPolicy Bypass -File ./build.ps1

# Build + run tests
powershell.exe -ExecutionPolicy Bypass -File ./build.ps1 -RunTests

# Build + package SDK
powershell.exe -ExecutionPolicy Bypass -File ./build.ps1 -Pack

# Build + tests + package
powershell.exe -ExecutionPolicy Bypass -File ./build.ps1 -RunTests -Pack
```

Output: `out/build/ninja-msvc-debug/DirectXShaderPatcher-msvc-debug.zip`

### CLI Usage

```bash
# Validate a recipe
dxp sm5 validate recipes/sm5_recipe.yml

# Patch a shader
dxp sm5 patch recipe.yml input.cso output.cso
dxp sm5 patch recipe.yml input.cso output.cso --trace
```

### Testing Instructions

- Run all tests: `ctest --preset ninja-msvc-debug --output-on-failure`
- Run a specific test: `ctest --preset ninja-msvc-debug --output-on-failure -R <test_name>`
- Run a test executable directly: `.\out\build\ninja-msvc-debug\<test_name>.exe <shader.cso>`
- Patched test artifacts are written to `out/build/ninja-msvc-debug/test-output/`
- Always run the full test suite before committing

### Benchmarking

Benchmarks are built in a separate release-bench configuration and run via the `-RunBenchmarks` flag:

```bash
# Build + run benchmarks (release-bench preset)
powershell.exe -ExecutionPolicy Bypass -File ./build.ps1 -RunBenchmarks
```

The benchmark executable is located at `out/build/ninja-msvc-release-bench/sm5_benchmarks.exe`.

**Benchmark suites:**

| Benchmark | What it measures |
|---|---|
| `BM_CollectMatches` | Single-instruction O(N) pattern matching |
| `BM_CollectSequenceMatches` | Multi-pattern O(N×M) sequence matching |
| `BM_RefreshDeclarations` | Full declaration refresh (Resources, Samplers, CBuffers, etc.) |
| `BM_RewriteAndRebuild` | Rewrite actions on a program with many matches |
| `BM_RebuildShaderChunk` | Program-to-bytes serialization |
| `BM_SerializeDxbcContainer` | Container serialization (chunks + header + offset table) |
| `BM_PatchContainer_end_to_end` | Full patch pipeline (parse → recipe → serialize) |
| `BM_PatchSequenceMatch_end_to_end` | End-to-end patching with sequence matching |

**Run with custom benchmark flags:**

```bash
# Run benchmarks with longer iterations for stable results
.\out\build\ninja-msvc-release-bench\sm5_benchmarks.exe --benchmark_min_time=0.1s

# Run a specific benchmark
.\out\build\ninja-msvc-release-bench\sm5_benchmarks.exe --benchmark_filter=BM_RefreshDeclarations

# Run with CSV output for regression tracking
.\out\build\ninja-msvc-release-bench\sm5_benchmarks.exe --benchmark_out=results.csv --benchmark_out_format=csv
```
