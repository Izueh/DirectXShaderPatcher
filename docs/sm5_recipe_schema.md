# SM5 YAML Recipe Schema

SM5 recipes use schema version `1`.

## Top-Level Shape

```yaml
version: 1
reserved_temps: 0
steps: []
temp_decls: []
```

Rules:

- `steps` is required and execution order is defined only by `steps`.
- Top-level `rewrite_rules` are rejected in schema version `1`; use `apply_rules` steps instead.
- Top-level `*_decls` are rejected in schema version `1`; use `add_*` declaration steps instead.
- `temp_decls` are still allowed at the top level.

This schema is specific to `dxp::sm5`. It is separate from the DXIL schema because SM5 patching operates on DXBC token IR.

## Steps

Supported step fields:

- `kind` optional, defaults to `apply_rules`
- `name` optional
- `if.state`, `if.all`, `if.any`, and `if.not` optional condition fields for any step
- `required` optional, defaults to `true`
- `mode` for `apply_rules` and `prefilter`
- `set` only for `prefilter`
- `checks` only for `prefilter`
- `rules` only for `apply_rules`

Supported step kinds:

- `apply_rules`
- `prefilter`
- `refresh_resources`
- `verify_program`
- `add_input`
- `add_output`
- `add_texture`
- `add_raw_resource`
- `add_structured_resource`
- `add_cbuffer`
- `add_sampler`
- `add_uav`

Step notes:

- `apply_rules` is the default when `kind` is omitted.
- `apply_rules` accepts `mode` and `rules`.
- `prefilter` accepts `mode`, optional `set`, and `checks`.
- `if` may select one of `state`, `all`, or `any`; `not: true` negates the selected condition result.
- Other step kinds must not define `mode`, `set`, `checks`, or `rules`.

## Conditional Steps

Every SM5 step may define an `if` guard that reads boolean-like values from
recipe context state and composes them.

Shape:

- Exactly one of: `state`, `all`, `any`
- Optional: `not`, defaults to `false`

Example:

```yaml
steps:
  - kind: prefilter
    name: require_expected_shape
    checks:
      - kind: check_shader_version
        major: 5
        minor: 0

  - kind: add_texture
    name: add_noise_tex
    if:
      all:
        - state: require_expected_shape
        - state: optional_probe
          not: true
    handle: noise_tex
    auto_bind: true
```

Notes:

- `if.state` reads from `RecipeContext::State`.
- `if.all` requires every nested condition to evaluate to true.
- `if.any` requires at least one nested condition to evaluate to true.
- `if.not: true` negates the result of `state`, `all`, or `any`.
- Missing state values are treated as `false`.
- `prefilter` steps write a boolean state value for later guards.

## Prefilter Steps

Prefilters are first-class steps in SM5 schema version `1`.

Supported prefilter check kinds:

- `check_shader_version`
- `check_opcode_count`
- `check_resource_count`
- `check_pattern_match`

Supported `prefilter` step fields:

- `name` optional
- `mode` optional, accepts `all` or `any`, defaults to `all`
- `set` optional, defaults to the step `name`
- `checks` required

Supported fields inside `checks[]`:

- `kind`
- `name` optional
- `required` optional, parsed for compatibility but ignored by the step executor
- `major`, `minor` for `check_shader_version`
- `opcode`, `expected_count` for `check_opcode_count`
- `expected_resources` for `check_resource_count`
- `match` for `check_pattern_match`

Example:

```yaml
steps:
  - kind: prefilter
    name: require_expected_shape
    mode: all
    set: require_expected_shape
    checks:
      - kind: check_shader_version
        major: <major>
        minor: <minor>
      - kind: check_pattern_match
        match:
          sequence:
            - opcode: <opcode_a>
            - opcode: <opcode_b>

  - kind: add_texture
    if:
      state: require_expected_shape
    handle: <handle>
    auto_bind: true
```

Notes:

- `expected_count < 0` means at most `abs(expected_count)`.
- `expected_count == 0` means exactly zero.
- `expected_count > 0` means at least that many.
- `opcode` fields accept canonical SM5 opcode names for all non-reserved D3D10, D3D10.1, D3D11, D3D11.1, and WDDM 1.3 opcodes.
- Test-boolean opcodes may also use assembly-style aliases like `discard_z`, `discard_nz`, `if_z`, and `retc_nz`.
- `check_pattern_match` requires `match.opcode` or `match.sequence`.
- `match.sequence` cannot be combined with single-instruction match fields in the same pattern.
- `prefilter` no longer stops or fails the recipe directly; it publishes a boolean probe result for later `if` guards.
- Top-level `prefilters` are rejected in schema version `1`; use `steps[].kind: prefilter` instead.

## Rules

Rules live inside `apply_rules` steps.

This YAML schema only describes declarative rules. Code-built recipes may also
use callback overloads for rule matching and rewriting, but those callbacks are
not serializable and therefore have no YAML representation.

```yaml
steps:
  - kind: apply_rules
    name: <step_name>
    mode: MatchAll
    rules:
      - match:
          opcode: <opcode>
          capture: <capture_name>
          rewrite_mode: Replace
        emit:
          - opcode: <opcode>
            operands:
              - capture: <capture_name>
```

Supported rule fields:

- `match`
- `emit`
- `replace`
- `mode`

Rule field notes:

- `match`, `emit`, and `replace` describe the declarative rule path.
- Callback-based matching and rewriting are builder-only API features and are
  intentionally not part of the YAML schema.

`mode` controls how a rule is applied inside the step and accepts:

- `First`
- `Last`
- `MatchAll`

Supported `match` fields:

- `opcode`
- `capture`
- `rewrite_mode`
- `range_start_offset`
- `range_end_offset`
- `saturate`
- `interpolation_mode`
- `test_boolean`
- `operands[]`
- `sequence[]`

`match.rewrite_mode` accepts:

- `None`
- `Replace`
- `Before`
- `After`
- `ReplaceRange`

Rule notes:

- Omitted `match.rewrite_mode` defaults to `Replace`.
- `Replace` rewrites the full matched instruction window.
- Rules without `emit` must use `match.rewrite_mode: None`.
- Declarative rules always match through `match`; they cannot mix declarative
  `match` and callback-based matching in YAML.
- `match.opcode` and `emit[].opcode` accept canonical SM5 opcode names for all non-reserved D3D10, D3D10.1, D3D11, D3D11.1, and WDDM 1.3 opcodes.
- Test-boolean opcodes may use `_z` and `_nz` alias spellings instead of a separate `test_boolean` field.
- Alias spellings and explicit `test_boolean` must agree; if an alias already implies zero or nonzero, a conflicting `test_boolean` is rejected.
- `test_boolean` is valid only for opcodes that actually carry the SM5 zero/nonzero control bit.
- `Before` inserts emitted instructions immediately before the matched instruction or named `replace` capture.
- `After` inserts emitted instructions immediately after the matched instruction or named `replace` capture.
- `match.sequence` matches a contiguous instruction window.
- Use `ReplaceRange` with `range_start_offset` and `range_end_offset` to replace a sub-window inside the matched instruction window.
- `range_start_offset` must be `>= 0`.
- `range_end_offset` must be `-1` or `>= 0`; `-1` means the end of the matched instruction window.
- Offset fields are valid only when `match.rewrite_mode: ReplaceRange`.
- `replace` captures are only valid with `Before` and `After` rewrite modes.

## Operands

Supported fields in `match.operands[]`:

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

Supported fields in `emit[].operands[]`:

- `any` (unsupported for emit; validation error)
- `capture`
- `capture_fields`
- `bind_handle`
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

Component selector kinds:

- `mask`
- `swizzle`
- `select`

Operand notes:

- Emit `capture` copies a previously captured operand.
- Emit `capture_fields` projects selected properties from `capture` and keeps
  other properties literal from the emit operand template.
- Literal `indices` and `immediate*` values override replayed values when
  `capture_fields.indices` or `capture_fields.immediates` are enabled.
- `bind_handle` resolves declaration handles from `temp_decls` and `add_*` declaration steps.
- `bind_handle` requires an explicit `type` and a matching declaration handle.
- `match_capture` requires the operand to match an earlier captured operand exactly.
- `match_capture_fields` restricts `match_capture` comparison to selected
  operand properties.
- For `type: immediate32` and `type: immediate64`, literal payload words are provided via ordered `indices` entries (`immediate_lo`/`immediate_hi`) or emit shorthand arrays (`immediates_u32` / `immediates_u64` / `immediates_i32` / `immediates_i64` / `immediates_f32` / `immediates_f64`).
- Immediate float payloads are expressed as their raw IEEE-754 bit patterns in `immediate_lo`.
- `immediates_u32`, `immediates_u64`, `immediates_i32`, `immediates_i64`, `immediates_f32`, and `immediates_f64` are emit-only and rejected on match operands.
- Emit operands may define either explicit `indices` or shorthand arrays (`immediates_u32` / `immediates_u64` / `immediates_i32` / `immediates_i64` / `immediates_f32` / `immediates_f64`), but not both on the same operand.
- Each `immediates_u64` entry is one integer literal that is split into `immediate_lo`/`immediate_hi` on a single `immediate64` index entry.
- Each `immediates_i32` entry is encoded using signed two's-complement payload bits and mapped to one `immediate32` index entry (`immediate_lo`).
- Each `immediates_i64` entry is encoded using signed two's-complement payload bits and split into `immediate_lo`/`immediate_hi` on one `immediate64` index entry.
- Each `immediates_f32` entry is encoded to its IEEE-754 payload and mapped to one `immediate32` index entry (`immediate_lo`).
- Each `immediates_f64` entry is encoded to its IEEE-754 payload and split into `immediate_lo`/`immediate_hi` on one `immediate64` index entry.
- `scratch` and `state_temp` are unsupported and rejected.

`capture_fields` and `match_capture_fields` flags:

- `type`
- `components`
- `modifier`
- `indices`
- `immediates`

Field projection notes:

- `capture_fields` requires emit operand `capture`.
- `match_capture_fields` requires match operand `match_capture`.
- If no projection flags are set, `capture` and `match_capture` use full-operand
  replay/compare semantics.

`indices` is an ordered list of index objects. Scalar index lists are not
supported.

Emit shorthand arrays:

- `immediates_u32` is an ordered list of integer literals that map to
  `immediate32` index entries (`immediate_lo` only).
- `immediates_u64` is an ordered list of integer literals that map to
  `immediate64` index entries (`immediate_lo` + `immediate_hi`).
- `immediates_i32` is an ordered list of signed integer literals that map to
  `immediate32` index entries (`immediate_lo` two's-complement payload bits).
- `immediates_i64` is an ordered list of signed integer literals that map to
  `immediate64` index entries (`immediate_lo` + `immediate_hi`
  two's-complement payload bits).
- `immediates_f32` is an ordered list of float literals that map to
  `immediate32` index entries (`immediate_lo` payload bits).
- `immediates_f64` is an ordered list of float literals that map to
  `immediate64` index entries (`immediate_lo` + `immediate_hi` payload bits).
- Shorthand arrays are normalized as if equivalent explicit `indices` entries
  were authored.

Index object fields:

- `any` optional wildcard for a single ordered index slot
- `representation` optional; one of:
  - `immediate32`
  - `immediate64`
  - `relative`
  - `immediate32_plus_relative`
  - `immediate64_plus_relative`
- `immediate_lo` optional immediate value for low 32 bits
- `immediate_hi` optional immediate value for high 32 bits
- `immediate_lo` and `immediate_hi` accept integer literals only (decimal or hex)
- transitional replay-object form is also accepted for `immediate_lo`:
  `immediate_lo: { from: <index_capture_name> }`
- `capture` optional (match only): captures the current matched index immediate value
- `match_capture` optional:
  - in `match`: compares the current index immediate value with a previously captured index value
  - in `emit`: resolves the emitted index immediate value from a previously captured index value

Index notes:

- Index entries are matched and emitted in list order.
- `capture` on emit index entries is invalid.
- `any` on emit index entries is invalid.
- replay-object form on `immediate_hi` is currently unsupported in v1.
- If `representation` is omitted, `immediate32` is assumed.
- `representation` is per index entry (slot), not per operand.
- Shorthand arrays preserve operand-level declaration order: all
  `immediates_u32` entries are emitted first, followed by `immediates_u64`,
  then `immediates_i32`, then `immediates_i64`, then `immediates_f32`, then
  `immediates_f64` entries.
- `capture` and `match_capture` are independent; an entry may set both to capture
  a value and simultaneously compare it against an earlier capture.
- Capture names are kind-specific:
  - operand `match_capture` and emit operand `capture` must reference operand captures
  - index `match_capture` must reference index captures
  - rule `replace` must reference instruction captures

Index capture round-trip example — rewrite `mul r0.xyzw, r0.xyzw, r1.xyzw` to
`mov r0.xyzw, r1.xyzw` while preserving the exact temp register number:

```yaml
steps:
  - name: mul_to_mov
    rules:
      - match:
          opcode: mul
          operands:
            - type: temp
              capture: dst
              indices:
                - representation: immediate32
                  capture: dst_reg   # stores r0's register number
            - capture: src
        emit:
          - opcode: mov
            operands:
              - capture: dst         # replay captured destination operand
              - capture: src         # replay captured source operand
```

Emit shorthand examples for immediate literals:

```yaml
steps:
  - name: emit_immediates
    rules:
      - match:
          opcode: mov
          operands:
            - capture: dst
            - capture: src
        emit:
          - opcode: mov
            operands:
              - capture: dst
              - type: immediate32
                immediates_u32: [0x3F000000]   # 0.5f payload bits

          - opcode: mov
            operands:
              - capture: dst
              - type: immediate64
                immediates_u64: [0x1122334455667788]

          - opcode: mov
            operands:
              - capture: dst
              - type: immediate32
                immediates_i32: [-1]

          - opcode: mov
            operands:
              - capture: dst
              - type: immediate64
                immediates_i64: [-2]

          - opcode: mov
            operands:
              - capture: dst
              - type: immediate32
                immediates_f32: [0.5]

          - opcode: mov
            operands:
              - capture: dst
              - type: immediate64
                immediates_f64: [1.25]
```

Invalid mix (rejected):

```yaml
steps:
  - name: invalid_mix
    rules:
      - match:
          opcode: mov
          operands:
            - capture: dst
            - capture: src
        emit:
          - opcode: mov
            operands:
              - type: immediate32
                indices:
                  - immediate_lo: 1
                immediates_u32: [2]
```

To reconstruct the destination from its captured index instead of replaying the
entire operand (useful when the component mask must differ):

```yaml
        emit:
          - opcode: mov
            operands:
              - type: temp
                indices:
                  - representation: immediate32
                    match_capture: dst_reg   # resolved from captured index
              - capture: src

To replay selected operand fields while overriding specific literals:

```yaml
        emit:
          - opcode: mov
            operands:
              - capture: dst
                capture_fields:
                  type: true
                  indices: true
                modifier: neg
              - capture: src
```
```

## Declaration Steps

Declaration steps support either explicit `bind_point` or `auto_bind: true`.

Common declaration fields:

- `handle`
- `bind_point` optional
- `auto_bind` optional, defaults to `false`

Step-specific fields:

- `add_input`: `interpolation_mode` optional, defaults to `linear`
- `add_texture`: `dimension` optional, defaults to `Texture2D`
- `add_structured_resource`: `stride` optional, defaults to `16`
- `add_cbuffer`: `elements` optional, defaults to `1`; `access_pattern` optional, defaults to `immediateIndexed`
- `add_sampler`: `sampler_mode` optional, defaults to `default`
- `add_uav`: `uav_kind` optional, defaults to `typed`; `dimension` optional, defaults to `Texture2D`; `stride` optional, defaults to `16`; `globally_coherent` optional, defaults to `false`; `has_counter` optional, defaults to `false`

Examples:

```yaml
temp_decls:
  - handle: <temp_handle>

steps:
  - kind: add_input
    handle: <input_handle>
    auto_bind: true
    interpolation_mode: linear

  - kind: add_texture
    handle: <texture_handle>
    auto_bind: true
    dimension: <texture_dimension>

  - kind: add_cbuffer
    handle: <cbuffer_handle>
    auto_bind: true
    elements: <element_count>
    access_pattern: <access_pattern>

  - kind: add_uav
    handle: <uav_handle>
    auto_bind: true
    uav_kind: <uav_kind>
    stride: <stride>
    has_counter: <bool>
```

Supported texture dimensions:

- `Texture1D`
- `Texture2D`
- `Texture2DArray`
- `Texture3D`
- `TextureCube`

Supported cbuffer access patterns:

- `immediateIndexed`
- `dynamicIndexed`

Supported sampler modes:

- `default`
- `comparison`
- `mono`

Supported UAV kinds:

- `typed`
- `raw`
- `structured`

## Reserved Temps

Recipes may reserve a temp register range up front:

```yaml
reserved_temps: 2
```

Notes:

- `reserved_temps` increases `dcl_temps` before rule execution.
- `temp_decls` are mapped in order onto the reserved temp range starting at `ReservedTempBase`.
- When both are present, the effective reserved count is `max(reserved_temps, temp_decls.size())`.
