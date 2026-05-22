# SM5 Library Reorganization Plan

## Accepted Constraints

This plan takes the following as fixed design requirements:

- The supported public SM5 API should be the declarative recipe surface plus in-memory patch execution.
- Low-level DXBC token parsing, matching, rewriting, and serialization should not be part of the default public API.
- Advanced behavior should be exposed through recipe callbacks and step composition, similar in spirit to the SM6 recipe API.
- The SM5 library should not expose broad token-level machinery just because internal tests use it.
- Any narrowing should preserve the current working CLI and focused SM5 test coverage during migration.

## Problem Statement

SM5 currently exposes a much larger surface than intended.

The umbrella header exports all of these SM5 modules:

- `dxp/sm5/Container.h`
- `dxp/sm5/Match.h`
- `dxp/sm5/Model.h`
- `dxp/sm5/Patch.h`
- `dxp/sm5/Parse.h`
- `dxp/sm5/RecipeParse.h`
- `dxp/sm5/Recipe.h`
- `dxp/sm5/Rewrite.h`
- `dxp/sm5/Serialize.h`

That makes the token IR, matcher, rewrite engine, and serializer look like supported external API even though the intended product surface is declarative patching.

The deeper design leak is that the current public recipe model depends directly on internal token and matcher types:

- `Recipe.h` uses `InstructionMatch`
- `Recipe.h` uses `Instruction`
- those types come from `Match.h` and `Model.h`

So today, `Recipe.h` cannot remain public without dragging the low-level bytecode API into the public contract.

## End-State Goals

The end state should make SM5 feel like a declarative patch library with an escape hatch for custom steps, not like a public DXBC token toolkit.

### Public SM5 surface

The supported public SM5 headers should be reduced to:

- `dxp/sm5/Patch.h`
- `dxp/sm5/Recipe.h`
- `dxp/sm5/RecipeParse.h`

The umbrella header should export only those three SM5 headers.

### Internal SM5 surface

These modules should be treated as implementation detail and removed from the umbrella surface:

- `Container.h`
- `Model.h`
- `Parse.h`
- `Match.h`
- `Rewrite.h`
- `Serialize.h`

They may remain physically in `include/dxp/sm5` during migration if needed for build stability, but the target state is that they are not part of the advertised public API. A later cleanup can move them under an internal path such as `src/dxp/sm5/internal` or `include/dxp/sm5/detail`.

## Target Public API Shape

### 1. Patch API

`Patch.h` should stay small.

It should expose only:

- patch result and patch execution context types
- in-memory patch entry points

It should not expose:

- DXBC container internals
- token parse helpers
- recipe parsing entry points

That part of the cleanup is already directionally correct.

### 2. Recipe Parse API

`RecipeParse.h` should remain the YAML/text parse boundary.

It should expose:

- `RecipeParseResult`
- `ParseRecipeText`
- `ParseRecipeFile`

It should parse into the public declarative `Recipe` model, not into token-level matcher or instruction types.

### 3. Recipe API

`Recipe.h` needs the largest rewrite.

It should become the only public place where SM5 recipes are authored programmatically.

The public concepts should be:

- `Recipe`
- `RecipeStep`
- `RecipeContext`
- `RecipeExecutionOptions`
- `RecipeStepResult`
- public declarative descriptors for resource declarations and rewrite rules
- step factory helpers
- custom step callback support

The public concepts should not be:

- raw `Program`
- raw `Instruction`
- raw `Operand`
- raw `InstructionMatch`
- raw `RewriteAction`
- direct `ExecuteRecipe*` runtime helpers

## Recommended Public Recipe Model

The SM5 public recipe model should be rewritten around declarative steps plus callbacks.

### Public execution types

Recommended public types in `Recipe.h`:

- `RecipeExecutionOptions`
  - trace flag
  - arbitrary inputs
  - initial state

- `RecipeStepResult`
  - success
  - changed
  - match count
  - stop recipe

- `RecipeContext`
  - trace flag
  - diagnostics
  - total rule matches
  - state bag
  - declared resources added through the recipe
  - helper methods for state and diagnostics

- `RecipeStepExecutor`
  - callback type used by custom steps

- `RecipeStep`
  - step name
  - callable executor

- `Recipe`
  - owns a sequence of steps
  - supports `AddStep(...)`
  - exposes `GetSteps()`

### Public declarative descriptors

The public declarative descriptors should describe intent, not token IR.

Recommended categories:

- shader prefilter specs
  - shader version
  - opcode-count expectations
  - resource-count expectations

- declaration specs
  - texture declaration spec
  - cbuffer declaration spec
  - sampler declaration spec

- rewrite rule specs
  - opcode-oriented rule shape
  - operand-pattern shape
  - capture names
  - replacement opcode
  - emitted instruction descriptors
  - application mode

These should be schema-shaped types that the YAML parser can produce directly.

They should not reuse internal token structures like `InstructionMatch` or `Instruction`.

### Public step factories

Recommended public factories:

- `MakeCustomRecipeStep(...)`
- `MakeRewriteRulesStep(...)`
- `MakePrefilterStep(...)`
- `MakeAddTextureStep(...)`
- `MakeAddCBufferStep(...)`
- `MakeAddSamplerStep(...)`
- `MakeExpectShaderVersionStep(...)`
- `MakeExpectOpcodeCountStep(...)`
- `MakeExpectResourceCountStep(...)`

This gives SM5 the same usage pattern as SM6: public recipe composition is step-based, and custom behavior is attached through a callback rather than by exposing the entire internal engine.

## Callback Strategy

The callback model is the key design change.

### Intended role of callbacks

Callbacks are the only supported escape hatch for behavior that is not expressible by the default declarative rule set.

That means:

- default consumers use YAML or public declarative step factories
- advanced consumers compose recipes with custom steps
- low-level token operations remain behind the step execution boundary

### What callbacks should receive

Preferred public callback signature:

- `RecipeStepResult(RecipeContext &context)`

That keeps the callback API stable and avoids publishing the raw SM5 token IR.

### What context should expose

`RecipeContext` should expose recipe-oriented capabilities such as:

- read recipe inputs and state
- write diagnostics and errors
- inspect high-level shader facts needed by public steps
- register or query declarations added by the recipe
- request built-in rewrite rule application
- stop execution with structured failure

### What should stay out of the public callback contract

The callback contract should not expose these directly in the stable public API:

- `Program`
- `Instruction`
- `Operand`
- `MatchResult`
- `RewriteAction`
- container chunk structures

If truly necessary, a separate non-umbrella advanced header can later expose unstable raw-program access for internal tools and tests, but that should not define the main SM5 public API.

## Internal Module Rewrite Map

The public API rewrite implies a corresponding internal reorganization.

### Public-facing modules

- `Patch.h/.cpp`
  - top-level patch orchestration only
  - parse container bytes
  - parse public recipe
  - execute public recipe
  - serialize output bytes

- `Recipe.h/.cpp`
  - public recipe composition model
  - public step factories
  - callback execution surface

- `RecipeParse.h/.cpp`
  - YAML/text to public `Recipe`

### Internal-only modules

- `Container.h/.cpp`
  - DXBC container parsing and rebuild

- `Model.h/.cpp`
  - token IR

- `Parse.h/.cpp`
  - shader bytecode to token IR

- `Match.h/.cpp`
  - token-level matcher

- `Rewrite.h/.cpp`
  - token-level rewrite primitives

- `Serialize.h/.cpp`
  - token IR back to bytes

- new internal execution adapter module
  - suggested name: `RecipeExecution.h/.cpp` or `RecipeRuntime.h/.cpp`
  - translates public rule specs into internal match and rewrite operations
  - executes public steps against the internal SM5 program model

This is the main missing layer in the current design. Today `Recipe.h` is both public recipe schema and internal execution interface. Those responsibilities must be separated.

## Header-by-Header Disposition

### `Patch.h`

Keep public.

Public contents should remain limited to:

- `RecipeContext` or its renamed equivalent
- `PatchResult`
- `PatchContainerInMemory(...)`

### `RecipeParse.h`

Keep public.

It is already a good public boundary.

### `Recipe.h`

Keep public, but rewrite it heavily.

Remove direct dependence on:

- `Match.h`
- `Rewrite.h`
- low-level token `Instruction`

Add:

- step-based recipe composition
- callback step support
- declarative schema-level descriptors

Move execution helpers out of the public header.

### `Container.h`

Demote from umbrella-exported public API.

Keep it available only for internal code and any explicit advanced tooling that opts into internals.

### `Model.h`

Demote from umbrella-exported public API.

This is raw SM5 token IR and should not define the default consumer contract.

### `Parse.h`

Demote from umbrella-exported public API.

`ParseProgram`, `ParseShaderChunk`, and `GetShaderBytecode` are internal pipeline functions.

### `Match.h`

Demote from public API.

This is runtime implementation detail for declarative rewrite execution.

### `Rewrite.h`

Demote from public API.

This is token-level mutation plumbing.

### `Serialize.h`

Demote from public API.

This is internal rebuild machinery.

## Migration Phases

The rewrite should be staged. Do not attempt a one-shot header collapse.

### Phase 0: Freeze the target contract in docs

- Update this plan to reflect the final public/private split.
- Treat it as the contract for subsequent edits.

### Phase 1: Narrow the advertised umbrella surface

- Reduce `include/DirectXShaderPatcher.h` so SM5 exports only:
  - `dxp/sm5/Patch.h`
  - `dxp/sm5/Recipe.h`
  - `dxp/sm5/RecipeParse.h`
- Update internal repo consumers that currently depend on umbrella-exported internals.

This changes the advertised API immediately without yet changing every internal include path.

### Phase 2: Split public recipe schema from internal execution

- Rewrite `Recipe.h` to hold only public declarative and callback-facing types.
- Move `ExecuteRecipeStep`, `ExecutePrefilters`, `ExecuteRecipe`, `MakeRecipeStepSuccess`, and `MakeRecipeStepFailure` into a new internal execution header.
- Introduce public step and callback abstractions.

This is the critical structural phase.

### Phase 3: Introduce an internal adapter layer

- Add `RecipeExecution.h/.cpp` or `RecipeRuntime.h/.cpp`.
- Translate public rewrite rule specs into internal matcher and rewrite types.
- Keep `Match`, `Rewrite`, `Serialize`, and token-level program mutation behind that layer.

After this phase, `Patch.cpp` should call into the internal recipe runtime rather than public `Recipe.h` execution helpers.

### Phase 4: Rewrite YAML parsing to target the new public schema

- Update `RecipeParse.cpp` so YAML maps directly into the new public `Recipe` types.
- Remove any remaining dependence on internal token or matcher types from the parse boundary.

### Phase 5: Move private headers out of the advertised surface

- Keep internal includes working either by:
  - moving internal headers under a private path, or
  - leaving them in place but documenting them as unsupported and no longer umbrella-exported
- Prefer a real internal path once the codebase is stable enough to absorb the include churn.

### Phase 6: Update tests by intent

Split tests into two categories:

- public API tests
  - CLI-level patching
  - YAML parse to recipe
  - recipe execution against real shaders

- internal engine tests
  - token parser
  - matcher
  - serializer
  - low-level rewrite mechanics

Internal engine tests can still include internal headers explicitly. They should not determine what stays in the public umbrella.

## Compatibility Approach

During migration, keep compatibility short-lived and deliberate.

Allowed temporary shims:

- transitional includes from public headers to internal adapters
- temporary type aliases while `Recipe.h` is rewritten
- temporary forwarders from old helper names to new runtime locations

Do not keep long-term compatibility shims for token-level SM5 APIs in the umbrella. The whole purpose of this rewrite is to stop treating them as stable public surface.

## Validation Plan

The validation strategy should mirror the current working build flow in `x64-debug`.

### Required build checks after each phase

- build `dxp`
- build focused SM5 tests that cover public recipe parsing and patching

### Required focused SM5 tests after each behavior-changing phase

- `dxp_validate_sm5_recipe_noop`
- `dxp_patch_sm5_recipe_noop_aliens_0x7AFF256C`
- `dxp_validate_sm5_recipe_objective`
- `dxp_patch_sm5_recipe_objective_aliens_0x7AFF256C`
- `sm5_recipe_emit_arbitrary_aliens_0x7AFF256C`
- `sm5_recipe_replace_capture_aliens_0x7AFF256C`
- `sm5_recipe_match_sequence_aliens_0x7AFF256C`
- `sm5_recipe_add_sampler_decl_aliens_0x7AFF256C`
- `sm5_recipe_objective_ign_texture_patch_aliens_0x7AFF256C`

### Additional internal validation once internals are demoted

- `sm5_container_roundtrip_aliens_0x7AFF256C`
- `sm5_replace_frc_with_mov_aliens_0x7AFF256C`
- `sm5_recipe_replace_frc_with_mov_aliens_0x7AFF256C`

These remain important, but they should be understood as internal-engine coverage, not proof that the broad token API must stay public.

## Recommended End State Summary

The intended SM5 story should be:

- consumers patch in-memory shader bytes
- consumers parse or compose declarative recipes
- consumers extend behavior with custom recipe steps and callbacks
- the DXBC token IR, matcher, rewriter, parser, and serializer are internal implementation detail

That is the correct API direction if the declarative recipe language is the product and the low-level engine is only the mechanism behind it.