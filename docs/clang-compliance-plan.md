# Clang Tooling Compliance Plan

This repository now uses the same top-level clang tooling settings as DirectXShaderCompiler:

- `.clang-format`: `BasedOnStyle: LLVM`
- `.clang-tidy`: `Checks: '-*,clang-diagnostic-*,llvm-*,misc-*'`
- Tooling scope: local project code only under `include/`, `src/`, `test/`, and `tools/`
- Excluded from helper checks: `external/` and generated/build trees (`build/`, `out/`, `x64/`)

## Supported entry points

- Configure-time linting: `-DDXP_ENABLE_CLANG_TIDY=ON`
- Manual format: `cmake --build <build-dir> --target dxp_clang_format`
- Manual format verification: `cmake --build <build-dir> --target dxp_clang_format_check`
- Manual clang-tidy pass: `cmake --build <build-dir> --target dxp_clang_tidy`

These helper targets do not scan the vendored DirectXShaderCompiler subtree.

## Rollout plan

1. Establish the baseline.
   - Reconfigure once so `compile_commands.json` is current.
   - Run `dxp_clang_format_check` to see the full formatting delta.
   - Run `dxp_clang_tidy` and capture the first pass of diagnostics.

2. Land formatting-only changes separately.
   - Reformat by directory or subsystem, not the whole tree in one mixed commit.
   - Keep behavior changes out of formatting commits so future diffs stay reviewable.
   - Prioritize `include/`, `src/dxp/`, `tools/`, then `test/`.

3. Fix clang-tidy issues in descending risk order.
   - Start with `clang-diagnostic-*` findings because they usually represent real compile or language issues.
   - Address `misc-*` findings next where they improve correctness or readability without changing semantics.
   - Handle `llvm-*` naming and style findings last, batching them into mechanical cleanups.

4. Add narrow exceptions only when they are justified.
   - Prefer local code changes over weakening the shared rule set.
   - If a rule conflicts with intentional shader-bytecode construction or test fixtures, document the reason at the call site or use a targeted `NOLINT`.
   - Avoid changing the root config unless DXC changes first or the project has a demonstrated local need.

5. Make compliance routine.
   - Run `dxp_clang_format_check` before merging C++ changes.
   - Use `DXP_ENABLE_CLANG_TIDY=ON` for local cleanup branches and CI-style verification builds.
   - When the warning volume is low enough, enable clang-tidy by default in at least one regular validation configuration.

## Suggested implementation sequence

1. Reformat headers under `include/` and library code under `src/dxp/sm6/`.
2. Re-run the existing test suite to confirm the formatting-only pass is behavior-neutral.
3. Clean up clang-tidy findings in `tools/dxp.cpp` and shared test support next.
4. Finish the long tail in scenario tests once the production code is clean.