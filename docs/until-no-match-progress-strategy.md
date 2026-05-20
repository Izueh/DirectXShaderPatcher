# UntilNoMatch Progress Strategy Plan

## Goal

Make `UntilNoMatch` safe and explicit by requiring each rule to declare how one successful application guarantees forward progress toward a fixed point.

This addresses two current problems:

1. `UntilNoMatch` can rely on hidden pruning side effects instead of explicit rewrite semantics.
2. When convergence depends on broad pruning, failures become hard to reason about and can interact badly with DXIL/LLVM teardown.

## Recommended Policy

- Keep `UntilNoMatch` as an engine capability.
- Do not rely on broad automatic pruning as an implicit convergence mechanism.
- Require an explicit progress strategy for `UntilNoMatch` rules.
- Allow `Before` and `After` under `UntilNoMatch` only when the rule explicitly guarantees invalidation of the old match.
- Keep full dead-code cleanup as a separate, explicit recipe step.

## Progress Strategy Definition

A progress strategy is the rule's declared reason why the same rule becomes less applicable after one successful application.

The engine should support these strategies:

- `replace_root`
  - The matched root is replaced and no longer exists in matchable form.
  - Natural fit for rewrite mode `Replace`.

- `replace_range`
  - The matched region is replaced as a unit.
  - Natural fit for rewrite mode `ReplaceRange`.

- `prune_roots`
  - The rewrite may insert `Before` or `After`, but it explicitly prunes the matched instructions after replacement.
  - Should name the captures that are expected to be removed.

- `predicate_only`
  - The structure may remain, but a predicate guarantees the same match fails next pass.
  - Should be treated as advanced mode and require a hard iteration cap.

## Proposed Recipe Shape

Prefer rule-local declaration so the progress contract travels with the rule definition.

Example:

```yaml
rules:
  blue_noise_scalar_slice_lhs:
    match: ...
    rewrite:
      mode: Replace
    progress:
      kind: replace_root
```

For explicit prune semantics:

```yaml
rules:
  some_insert_rule:
    match: ...
    rewrite:
      mode: After
    progress:
      kind: prune_roots
      captures:
        - old_root
        - old_handle
```

## Engine Model Changes

### 1. Add progress metadata to rewrite rules

Add a small enum and optional payload to `DxilRewriteRule`.

Suggested shape:

- `enum class DxilRewriteProgressKind { Unspecified, ReplaceRoot, ReplaceRange, PruneRoots, PredicateOnly };`
- optional `std::vector<std::string>` for prune capture names

Primary file:

- `include/dxp/sm6/Transforms.h`

### 2. Parse and validate progress metadata

Extend the parser to recognize a `progress` block and validate obvious mismatches.

Validation rules:

- `UntilNoMatch` with no progress declaration: reject or warn initially, then reject.
- `progress.kind: replace_root` requires rewrite mode `Replace`.
- `progress.kind: replace_range` requires rewrite mode `ReplaceRange`.
- `progress.kind: predicate_only` requires a predicate.
- `UntilNoMatch` with rewrite mode `Before` or `After` requires `prune_roots` or `predicate_only`.

Primary file:

- `src/dxp/sm6/Parse.cpp`

### 3. Add runtime guardrails

Some callback-based rules cannot be fully validated statically. Add runtime safety checks for `UntilNoMatch`:

- count applications per rule
- enforce a hard iteration cap, for example `256` or `1024`
- fail with a clear diagnostic including:
  - rule name
  - application mode
  - progress strategy
  - number of applications before abort

Optional follow-up:

- detect repeated application to the same root/capture set if practical

Primary file:

- `src/dxp/sm6/Recipe.cpp`

### 4. Narrow pruning semantics inside rewrite application

The engine should not treat broad recursive dead-code pruning as an ambient part of rewrite semantics.

Preferred rule:

- rewrite application may prune only explicit prune roots requested by the rule
- full dead-code cleanup remains in the explicit `prune_dead_code` recipe step

This keeps convergence visible and reduces the risk that unrelated dead-tree deletion changes engine behavior.

Primary file:

- `src/dxp/sm6/Transforms.cpp`

## Behavior Guidance by Mode

### `Once`

Use when:

- only one application is intended
- rewrite does not need fixed-point convergence
- `Before` or `After` insertion is fine without additional convergence guarantees

### `SinglePass`

Use when:

- all matches should be collected from the original IR before mutation
- rewrite does not need repeated rescanning
- rule order within a pass is acceptable, but repeated self-feeding is not needed

### `UntilNoMatch`

Use only when:

- the rule is intended as a normalization/fixed-point transform
- the rule declares a progress strategy
- each application makes the same rule less applicable

## Recommended Restrictions

- `UntilNoMatch` should not be the default.
- `Before` and `After` should be considered advanced under `UntilNoMatch`.
- Broad automatic pruning should not count as a valid progress strategy.
- If a rule terminates only because ambient prune deletes the old match, the rule is underspecified.

## Documentation Updates

Update recipe docs to reflect actual engine support and the new policy.

Tasks:

- document `SinglePass` alongside `Once` and `UntilNoMatch`
- explain progress strategies and when they are required
- give examples of safe `UntilNoMatch` usage
- warn against `Before`/`After` without explicit invalidation of the matched root

Primary file:

- `recipes/recipe_schema.md`

## Testing Plan

Add tests for:

- valid `UntilNoMatch` with `replace_root`
- valid `UntilNoMatch` with `replace_range`
- valid `UntilNoMatch` with `Before` plus `prune_roots`
- invalid `UntilNoMatch` with `Before` and no progress strategy
- invalid `predicate_only` with no predicate
- runtime abort on a deliberately non-converging callback rule
- docs/example coverage for `SinglePass` versus `UntilNoMatch`

Likely locations:

- `test/`
- parser-oriented tests if available; otherwise CLI-style declarative recipe tests

## Implementation Order

1. Add progress metadata to `DxilRewriteRule` in `include/dxp/sm6/Transforms.h`.
2. Parse and validate the new `progress` block in `src/dxp/sm6/Parse.cpp`.
3. Add runtime guardrails and iteration caps in `src/dxp/sm6/Recipe.cpp`.
4. Narrow rewrite-time pruning semantics in `src/dxp/sm6/Transforms.cpp`.
5. Update documentation in `recipes/recipe_schema.md`.
6. Add focused tests under `test/`.

## Practical Outcome

This keeps `UntilNoMatch` available, but changes it from a vague retry loop into a controlled fixed-point transform with explicit convergence rules.

The intended long-term invariant is:

- a rule author must say why repeated application converges
- the engine validates that claim as much as it can
- cleanup remains explicit instead of being a hidden semantic dependency
