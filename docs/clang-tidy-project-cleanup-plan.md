# Clang-Tidy Project Cleanup Plan

This note captures the next cleanup phase for clang-tidy issues that remain in DirectXShaderPatcher-owned code after the tooling configuration updates.

Current baseline:

- `#pragma once` is accepted by the root `.clang-tidy` config by disabling `llvm-header-guard`.
- Vendored DXC / LLVM code is excluded from normal formatting and helper tooling scope.
- DXC include directories are marked as `SYSTEM`, so clang-tidy output is now focused on project-owned files.
- Direct diagnostics in `src/dxp/sm6/Transforms.cpp` are clean; the remaining warnings are concentrated in project headers and a small number of implementation files.

## Remaining recurring issue classes

The recurring project-side clang-tidy findings currently fall into these buckets:

- `misc-non-private-member-variables-in-classes`
- `misc-no-recursion`
- `misc-include-cleaner`
- `misc-const-correctness`
- occasional `llvm-include-order`

These should not be fixed in one mixed sweep. The work should be split by risk and by whether the warning is mechanical or design-driven.

## Phase 1: Mechanical implementation-file cleanup

Scope this first pass to implementation files under `src/dxp/sm6/`.

Target checks:

- `misc-const-correctness`
- `misc-include-cleaner`
- `llvm-include-order`

Plan:

1. Run focused clang-tidy passes on `.cpp` files only.
2. Apply the low-risk fixes in small batches.
3. Rebuild and rerun the relevant tests after each batch.

Why first:

- These fixes are low risk and mostly stylistic or hygiene-related.
- They reduce warning noise before touching public data structures or recursive matcher logic.

## Phase 2: Define the project policy for DTO-style structs

Primary files:

- `include/dxp/sm6/Transforms.h`
- `include/dxp/sm6/Resources.h`
- `include/dxp/sm6/Patch.h`

The `misc-non-private-member-variables-in-classes` warnings are mostly hitting data carrier types such as:

- `DxilOperandPattern`
- `DxilCallPattern`
- `DxilMatchResult`
- `DxilRewriteRule`
- `ResourceBindingDesc`
- `DxilLoadedShaderState`

Plan:

1. Classify each type as either:
   - a deliberate data carrier / config object, or
   - an encapsulated type with invariants.
2. For deliberate data carriers, prefer a narrow suppression strategy over adding boilerplate getters and setters.
3. For invariant-bearing types, convert members to private and add only the minimal accessors or mutators needed.

Working recommendation:

- Treat the pattern / match / rewrite descriptor types in `Transforms.h` as intentional data-model structs unless a real invariant needs to be enforced.
- Review utility state holders such as `DxilLoadedShaderState` case by case.

## Phase 3: Resolve recursion warnings in the matcher tree model

Primary hotspot:

- `include/dxp/sm6/Transforms.h`

The `misc-no-recursion` warnings are tied to the recursive operand-pattern tree, especially:

- `DxilOperandPattern` owning `std::vector<DxilOperandPattern>`
- recursive matcher / traversal helpers in `Transforms.cpp`

Plan:

1. Decide whether the recursion is intentional and practically bounded.
2. If the recursion is intentional and expected to stay shallow:
   - add narrow suppressions on the affected types and helpers,
   - document why the recursive representation is acceptable.
3. If the recursion is considered an actual design problem:
   - redesign the pattern tree into an iterative or arena-backed representation,
   - update traversal helpers accordingly.

Working recommendation:

- Do not refactor the tree model blindly.
- Start by documenting the expected recipe depth and only refactor if there is evidence that recursion depth is a real maintenance or runtime risk.

## Phase 4: Clean remaining project-header warnings after policy decisions

After Phases 2 and 3 are settled:

1. Rerun clang-tidy on all project translation units.
2. Revisit remaining warnings in:
   - `include/dxp/sm6/Transforms.h`
   - `include/dxp/sm6/Resources.h`
   - `include/dxp/sm6/Patch.h`
3. Resolve each remaining warning either by:
   - code changes, or
   - targeted `NOLINT` comments with documented rationale.

This phase should be where intentional exceptions are made explicit instead of remaining as background warning noise.

## Phase 5: Keep the tooling signal clean

Follow-up rules for future cleanup work:

- Run clang-tidy against project translation units, not directly against headers.
- Keep vendored and generated trees out of formatting and clang-tidy review scope.
- Avoid enabling additional checks until the current project-owned warning set is stable.
- Keep formatting-only, mechanical tidy cleanup, and semantic refactors in separate commits.

## Recommended execution order

1. Fix mechanical `.cpp` issues.
2. Decide and document the struct-versus-class policy.
3. Decide and document the recursion policy for the matcher tree.
4. Clean the remaining project-header warnings.
5. Rerun full project clang-tidy plus build and tests.

## Smallest sensible next slice

The smallest useful next pass should be:

1. auto-fix `misc-const-correctness`, `misc-include-cleaner`, and `llvm-include-order` in:
   - `src/dxp/sm6/Parse.cpp`
   - `src/dxp/sm6/Patch.cpp`
   - `src/dxp/sm6/Recipe.cpp`
   - `src/dxp/sm6/Resources.cpp`
   - `src/dxp/sm6/Transforms.cpp`
2. then review `include/dxp/sm6/Transforms.h` and `include/dxp/sm6/Resources.h` for policy-based suppressions versus actual encapsulation changes.

That ordering keeps the first cleanup pass reviewable and avoids mixing design decisions with low-risk tidy hygiene.
