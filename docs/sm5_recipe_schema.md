# SM5 YAML Recipe Schema

JSON Schema companion: [sm5_recipe_schema.json](sm5_recipe_schema.json)

SM5 recipes use schema version `1`.

## Top-Level Shape

```yaml
version: 1
steps: []
```

Rules:

- `steps` is required and execution order is defined only by `steps`.
- Top-level `rewrite_rules` are rejected in schema version `1`; use `steps[].kind: apply_rules`.
- Top-level `*_decls` are rejected in schema version `1`; use `add_*` declaration steps.

This schema is specific to `dxp::sm5` and DXBC token IR patching.

## Steps

Common step fields:

- `name` required (must be unique across the recipe)
- `kind` optional only for `apply_rules` (defaults to `apply_rules` when omitted)
- `if` optional conditional guard
- `abort_on_failure` optional, defaults to `true`

Supported step kinds:

- `apply_rules`
- `check_shader_version`
- `check_opcode_count`
- `check_resource_count`
- `add_temp`
- `add_input`
- `add_output`
- `add_texture`
- `add_raw_resource`
- `add_structured_resource`
- `add_cbuffer`
- `add_sampler`
- `add_uav`

Notes:

- `refresh_resources` and `verify_program` were removed from SM5 steps.
- `mode` is valid only for `apply_rules`.
- `rules` is valid only for `apply_rules`.

## Exports

Recipes may declare typed exports to extract captured data after execution.

Export shape:

- `kind` required: `captured_operands`, `captured_instructions`, `captured_index_values`, `variables`, `state`
- `keys` optional: array of names; omit for all

Example:

```yaml
exports:
  - kind: captured_operands
    keys: [dst, src]
  - kind: variables
  - kind: state
    keys: [rule_matched]
```

Export data is available in `PatchResult` fields: `captured_operands`, `captured_instructions`, `captured_index_values`, `variables`, `state`.

## Variables

Variables are set via `SetVariable()` and persist across steps. The `Inputs` map, `SetInput()`, and `FindInput()` have been removed — all input data should use `SetVariable()` instead. `InitialVariables` and `SnapshotInitialVariables()` are internal helpers for `ResetVariables()` and are not part of the public API.

## Conditional Steps

Any SM5 step may define `if`.

Condition forms (exactly one per object):

- `state`
- `and`
- `or`
- `eq`
- `ne`
- `gt`
- `gte`
- `lt`
- `lte`

Optional field:

- `not` (defaults to `false`)

Notes:

- `if.state` reads from recipe context state.
- Comparison forms (`eq`/`ne`/`gt`/`gte`/`lt`/`lte`) require exactly one selector (`state`) plus literal `value`.
- Missing state values are treated as `false`.

## Explicit Check Steps

Scalar checks are first-class steps in SM5 schema version `1`.

`check_shader_version` fields:

- `name` required
- `major` required
- `minor` required

`check_opcode_count` fields:

- `name` required
- `opcode` required
- `expected_count` required

`check_resource_count` fields:

- `name` required
- `expected_resources` required

Notes:

- Each check step publishes a boolean state value under the step `name`.
- On a mismatch the step returns a failed result after publishing `false`.
- Set `abort_on_failure: false` when a check is intended to act only as a guard-producing probe.
- Pattern probing no longer has a dedicated step kind; use `apply_rules` with `match.rewrite_mode: none` and guard later steps with the rule `name`.

## Apply Rules Steps

Rules live inside `apply_rules` steps.

Apply-rules step fields:

- `name` required
- `mode` optional: `first`, `last`, `match_all` (defaults to `first`)
- `rules` required, non-empty

Rule fields:

- `name` required (must be unique across the recipe)
- `match` required
- `emit` optional
- `mode` optional: `first`, `last`, `match_all`
- `required_match` optional
- `refresh_declarations` optional, defaults to `false`

Removed rule fields:

- `replace` removed
- `set_match_state` removed

Rule behavior notes:

- Rule-level rewrite behavior is controlled by `match.rewrite_mode`.
- `match.rewrite_mode` values: `none`, `replace`, `before`, `after`, `replace_range`.
- `match.rewrite_mode: none` is the generic non-mutating probe path for pattern gating.
- `before` and `after` require `match.insert_relative_index >= 0`.
- `replace_range` is required when custom `range_start_offset` or `range_end_offset` are used.
- `match.sequence` cannot be combined with single-instruction match fields in the same `match` object.
- `match.opcode` and `emit[].opcode` accept canonical SM5 opcode names.
- Rule outcomes are published into recipe context state under the rule `name`.
- `refresh_declarations: true` tells the patcher to call `RefreshDeclarations`
  after the rule applies, updating derived metadata from the instruction stream.
  Set this on rules that modify DCL instructions.

## Global Capture System

All declarative matching writes captured operands, instructions, and index
immediates into a global `CaptureStore` as **copies** (not pointers). This
eliminates thousands of transient `unordered_map` allocations that previously
occurred per-instruction during `CollectMatches`. Captures survive instruction
rewrites and are automatically merged into subsequent steps.

### Cross-Step Capture References

`match_capture` references can point to any capture name defined in any rule
across any step in the recipe. All named identifiers share a single global
namespace:

| Name type | Where defined | Example |
|---|---|---|
| Capture names | `match.Operands[].capture`, `emit[].Operands[].capture` | `dst`, `src` |
| Instruction capture names | `match.capture`, `match.sequence[].capture`, `emit[].capture` | `frc_instr`, `mul_instr` |
| Match-capture names | `match.Operands[].match_capture`, `emit[].Operands[].match_capture` | `dst` (references a capture) |
| From-handle names | `emit[].Operands[].from_handle` | `my_texture` |
| Variable names | `immediates_u32`/`immediates_u64`/`immediates_i32`/`immediates_i64`/`immediates_f32`/`immediates_f64` shorthand array entries (when a value is a non-numeric identifier) | `my_reg` |
| Step names | `RecipeStep.name` | `step1` |
| Rule names | `RecipeRule.name` | `rule1` |

### Name Uniqueness

All recipe-defined names must be unique across the entire recipe. Duplicate
names are rejected at parse time with a clear error message:

```
duplicate name 'dst' in step 'step1' rule 'rule1'
```

### Index Capture Storage

`match_capture` on index objects resolves captured index immediates from the
global store. The old `CapturedInstructionIndices` map and
`GetCapturedInstructionIndex()` method have been removed — instruction indices
for sequence matches are now stored in `context.captures.indexValues` under
the key `<captureName>_index`.

### Capture Fields Projection

`match_capture_fields` and `capture_fields` control which operand fields are
replayed from captures. When `capture_fields: { components: true }` is used on
an emit operand, the component mode is automatically converted based on the
role change (source → destination, etc.). See the [Operand Roles](#operand-roles-and-component-mode-conversion) section for details.

### Instruction Capture and Emit

Instructions matched via `match.capture` or `match.sequence[].capture` are
stored in the global `CaptureStore` and can be emitted in subsequent rules or
steps using `emit[].capture` — mirroring the operand capture/emit pattern.

#### Single-Instruction Capture

Add `capture` to a single-instruction `match` block to store the matched
instruction:

```yaml
rules:
  - name: capture_frc
    match:
      opcode: frc
      operands:
        - capture: src
      capture: frc_instr
    emit:
      - opcode: frc
        operands:
          - capture: src
```

#### Sequence Capture

Add `capture` to any entry in `match.sequence` to store that specific
instruction from the sequence:

```yaml
rules:
  - name: capture_frc_then_mul
    match:
      sequence:
        - opcode: frc
          capture: frc_instr
          operands:
            - capture: frc_src
        - opcode: mul
          operands:
            - capture: mul_dst
            - capture: mul_src
    emit:
      - capture: frc_instr
      - opcode: mul
        operands:
          - capture: mul_dst
          - capture: mul_src
```

#### Emitting Captured Instructions

Use `emit[].capture` with the capture name to emit the stored instruction as a
raw copy. Captured instructions are emitted in their entirety by default.

```yaml
emit:
  - capture: frc_instr
```

#### Instruction Capture Fields Projection

Like operand capture, `capture_fields` can project specific fields from a
captured instruction. When specified, only the listed fields are replayed;
all other fields (operands, immediates, etc.) are omitted:

```yaml
emit:
  - capture: frc_instr
    capture_fields:
      opcode: true
      saturate: true
      operands: true
```

Supported instruction capture fields:

- `opcode` — replay the opcode
- `saturate` — replay the saturate modifier
- `test_boolean` — replay the test boolean value
- `operands` — replay all operands
- `immediates` — replay all immediates

#### Cross-Step Instruction Capture

Instruction captures persist across steps. A capture defined in an earlier
step is available in `emit[].capture` in any subsequent step:

```yaml
steps:
  - name: capture_step
    rules:
      - name: capture_rule
        match:
          opcode: frc
          capture: frc_instr
        emit:
          - opcode: frc
            operands:
              - capture: src

  - name: emit_step
    rules:
      - name: emit_frc
        match:
          opcode: add
        emit:
          - capture: frc_instr
          - opcode: add
            operands:
              - capture: add_dst
              - capture: add_src
```

#### Validation Rules

- `capture` on a match block requires `opcode` or `sequence`.
- `capture` and `opcode` cannot both be specified on the same `emit` entry.
- `capture_fields` requires a `capture` name on the same emit entry.
- The `capture` reference must resolve to an instruction capture defined in
  any rule across any step in the recipe.

## Operand Roles and Component Mode Conversion

Internally, `dxp::sm5` tracks whether each operand position in an instruction is a **source** (read) or **destination** (write) using an `InstructionLayout` table covering all ~190 SM5 opcodes. When `capture_fields: { components: true }` is used on an emit operand, the component mode is automatically converted based on the role change:

| Capture Role | Emit Role | Conversion |
|---|---|---|
| Source | Destination | NOSWIZZLE → full mask (xyzw); SELECT_1 → mask with selected bit; SWIZZLE → mask with unique sorted components; MASK → keep as-is |
| Destination | Source | NOSWIZZLE → keep as-is; single-bit MASK → SELECT_1; multi-bit MASK → SWIZZLE (sequential); SWIZZLE → keep as-is; SELECT_1 → keep as-is |
| Same role | Same role | Keep as-is |

This ensures that when a captured source operand (e.g., a single-component read from `ult`) is emitted as a destination (e.g., the target of `mov`), the write mask matches the read components.

### Context-Aware Priority

When converting a source operand to a destination, the conversion uses a context-aware priority to determine which components to write:

1. **Emit template literal spec** — if the emit operand has an explicit `mask`, `swizzle`, `select`, or `num_components` field, that defines the target mask.
2. **Matched instruction's destination mask** — if no literal spec, use the destination mask of the matched instruction (e.g., if the matched `add` writes to `xyz`, only those components are relevant).
3. **Source swizzle's unique components** — as a fallback, use the unique components from the source operand's swizzle.

This priority ensures the user's explicit intent takes precedence, while still producing correct defaults when no explicit mask is provided.

## Operands

Match operand fields:

- `any`
- `type`
- `indices`
- `components.kind`
- `components.value`
- `num_components`
- `modifier`
- `capture`
- `match_capture`
- `match_capture_fields`

Emit operand fields:

- `capture`
- `capture_fields`
- `from_handle`
- `type`
- `indices`
- `immediates_u32`
- `immediates_u64`
- `immediates_i32`
- `immediates_i64`
- `immediates_f32`
- `immediates_f64`
- `components.kind`
- `components.value`
- `num_components`
- `modifier`

Emit instruction fields:

- `opcode` or `capture` (mutually exclusive)
- `capture_fields` (requires `capture`)
- `saturate`
- `interpolation_mode`
- `test_boolean`
- `operands`

Notes:

- `from_handle` resolves declaration handles (`add_temp`, `add_input`, `add_output`, `add_texture`, `add_raw_resource`, `add_structured_resource`, `add_cbuffer`, `add_sampler`, `add_uav`).
- `from_handle` requires an explicit operand `type`.
- `bind_handle` was removed.
- `scratch` and `state_temp` are not part of schema version `1`.
- YAML selectors use `components.kind` + `components.value`; direct `mask`/`swizzle`/`select` YAML fields are not part of schema version `1`.
- Emit operands may use explicit `indices` or shorthand immediates arrays, but not both on the same operand.
- Shorthand immediates arrays are emit-only and rejected on match operands.
- `match_capture` and `capture` names must be unique across the entire recipe (see [Global Capture System](#global-capture-system)).

`indices` is an ordered list of index objects.

Index object fields:

- `any`
- `representation`: `immediate32`, `immediate64`, `relative`, `immediate32_plus_relative`, `immediate64_plus_relative`
- `immediate_lo`
- `immediate_hi`
- `capture`
- `match_capture`

Notes:

- `immediate_lo` and `immediate_hi` accept integer literals (parsed from YAML strings).
- `match_capture` on index objects resolves captured index immediates from the
global `CaptureStore`. May reference captures from any rule in any step.

## Declaration Steps

Common declaration fields:

- `name` required
- `abort_on_failure` optional
- `handle` (all declaration kinds except `add_temp`)
- `handles` (`add_temp` only)
- `bind_point` optional (except `add_temp`)
- `auto_bind` optional (except `add_temp`)

Step-specific fields:

- `add_temp`: `handles` required; `handle` is rejected
- `add_input`: optional `interpolation_mode` (default `linear`)
- `add_texture`: optional `dimension` (default `texture_2d`)
- `add_structured_resource`: optional `stride` (default `16`)
- `add_cbuffer`: optional `elements` (default `1`), `access_pattern` (default `immediate_indexed`)
- `add_sampler`: optional `sampler_mode` (default `default`)
- `add_uav`: optional `uav_kind` (`typed`/`raw`/`structured`), `dimension`, `stride`, `globally_coherent`, `has_counter`

Supported texture dimensions:

- `texture_1d`
- `texture_2d`
- `texture_2d_array`
- `texture_3d`
- `texture_cube`

Supported cbuffer access patterns:

- `immediate_indexed`
- `dynamic_indexed`

Supported sampler modes:

- `default`
- `comparison`
- `mono`

Supported UAV kinds:

- `typed`
- `raw`
- `structured`
