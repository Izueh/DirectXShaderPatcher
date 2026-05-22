# SM5 YAML Recipe Schema

SM5 recipes use schema version `1`.

## Schema

Top-level shape:

```yaml
version: 1
reserved_temps: 0
prefilters: []
steps: []
temp_decls: []
texture_decls: []
raw_resource_decls: []
structured_resource_decls: []
cbuffer_decls: []
sampler_decls: []
uav_decls: []
```

Schema rules:

- Use `prefilters`.
- Use `steps` (top-level `rewrite_rules` is not allowed).
- Emit operand capture references use `capture` (not `from_capture`).
- Component selectors use `components.kind` + `components.value`.
- Declarations support explicit `bind_point` and `auto_bind` forms.
- `handle` is optional unless referenced by `bind_handle`.

This schema is specific to `dxp::sm5`. It is separate from the DXIL schema in
`recipes/recipe_schema.md` because SM5 patching works against DXBC token IR.

## Prefilters

Supported prefilter kinds:

- `check_shader_version`
- `check_opcode_count`
- `check_resource_count`
- `check_pattern_match`

Fields:

```yaml
prefilters:
  - kind: check_shader_version
    name: ps_5_0_only
    required: true
    major: 5
    minor: 0
  - kind: check_opcode_count
    opcode: frc
    expected_count: 1
  - kind: check_resource_count
    expected_resources: 2
  - kind: check_pattern_match
    name: has_frc_mul_sequence
    required: true
    match:
      sequence:
        - opcode: frc
        - opcode: mul
```

Notes:

- `expected_count < 0` means at most `abs(expected_count)`.
- `expected_count == 0` means exactly zero.
- `expected_count > 0` means at least that many.
- `check_opcode_count` requires `opcode`.
- `check_pattern_match` requires `match.opcode` or `match.sequence`.
- `check_pattern_match` sequence form cannot be combined with single-instruction match fields.

## Steps and Rules

Rules are grouped in explicit ordered `steps`.

```yaml
steps:
  - name: rewrite_ign
    required: true
    mode: MatchAll
    rules:
      - match:
          opcode: frc
          capture: ign_frc
        emit:
          - opcode: mov
            operands:
              - capture: ign_frc
```

Supported step fields:

- `name`
- `required`
- `mode`
- `rules`

Supported rule fields:

- `match.opcode`
- `match.capture`
- `match.saturate`
- `match.interpolation_mode`
- `match.test_boolean`
- `match.operands[]`
- `match.sequence[]`
- `replace`
- `emit[].opcode`
- `emit[].saturate`
- `emit[].interpolation_mode`
- `emit[].test_boolean`
- `emit[].operands[]`
- `mode`

`mode` accepts:

- `First`
- `Last`
- `MatchAll`

`interpolation_mode` accepts:

- `undefined`
- `constant`
- `linear`
- `linear_centroid`
- `linear_noperspective`
- `linear_noperspective_centroid`
- `linear_sample`
- `linear_noperspective_sample`

Notes:

- `interpolation_mode` is valid only for `dcl_input_ps` and `dcl_input_ps_siv`.

## Operand Fields

Supported fields in `match.operands[]`:

- `type`
- `indices`
- `components.kind`
- `components.value`
- `num_components`
- `modifier`
- `immediates_u32`
- `immediates_f32`
- `capture`
- `match_capture`

Supported fields in `emit[].operands[]`:

- `capture`
- `scratch`
- `bind_handle`
- `state_temp`
- `type`
- `indices`
- `components.kind`
- `components.value`
- `num_components`
- `modifier`
- `immediates_u32`
- `immediates_f32`

Component selector kinds:

- `mask`
- `swizzle`
- `select`

Notes:

- Emit `capture` copies a previously captured operand into the emitted instruction.
- `scratch` allocates a temporary register for emitted intermediates and reuses by name.
- `state_temp` resolves a temp register index from recipe runtime state.
- `bind_handle` resolves declaration handles from `temp_decls`, `input_decls`, `output_decls`, `texture_decls`, `raw_resource_decls`, `structured_resource_decls`, `cbuffer_decls`, `sampler_decls`, or `uav_decls`.
- `bind_handle` requires explicit `type` and a matching declaration handle.
- `match_capture` requires the operand to match an earlier captured operand exactly.
- `match.sequence[]` matches a contiguous instruction window and replaces full range by default.
- When `match.sequence[]` is used with `replace`, only the named captured instruction is replaced.
- `immediates_f32` are encoded as raw SM5 immediate float tokens.

## Declaration Injection

Temp declarations:

```yaml
temp_decls:
  - handle: screen_uv_temp
```

Input declarations:

```yaml
input_decls:
  - handle: injected_input
    auto_bind: true
    interpolation_mode: linear
```

Output declarations:

```yaml
output_decls:
  - handle: injected_output
    auto_bind: true
```

Texture declarations:

```yaml
texture_decls:
  - handle: ign_texture
    auto_bind: true
    dimension: Texture2DArray
```

Raw resource declarations:

```yaml
raw_resource_decls:
  - handle: raw_srv
    auto_bind: true
```

Structured resource declarations:

```yaml
structured_resource_decls:
  - handle: structured_srv
    auto_bind: true
    stride: 16
```

Cbuffer declarations:

```yaml
cbuffer_decls:
  - handle: ign_constants
    auto_bind: true
    elements: 1
    access_pattern: immediateIndexed
```

Sampler declarations:

```yaml
sampler_decls:
  - handle: ign_sampler
    auto_bind: true
    mode: default
```

UAV declarations:

```yaml
uav_decls:
  - handle: rw_typed
    auto_bind: true
    kind: typed
    dimension: Texture2D
  - handle: rw_raw
    auto_bind: true
    kind: raw
  - handle: rw_structured
    auto_bind: true
    kind: structured
    stride: 16
    has_counter: false
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
- `temp_decls` are mapped in-order onto the reserved temp range (starting at `ReservedTempBase`).
- If both are present, effective reserved count is `max(reserved_temps, temp_decls.size())`.

## Example

```yaml
version: 1
prefilters:
  - kind: check_shader_version
    major: 5
    minor: 0
texture_decls:
  - handle: ign_texture
    auto_bind: true
sampler_decls:
  - handle: ign_sampler
    auto_bind: true
steps:
  - name: rewrite_ign
    mode: MatchAll
    rules:
      - match:
          opcode: sample_l
          operands:
            - capture: dst
            - capture: coord
        emit:
          - opcode: sample_l
            operands:
              - capture: dst
              - capture: coord
              - type: resource
                bind_handle: ign_texture
              - type: sampler
                bind_handle: ign_sampler
              - type: immediate32
                immediates_f32: [0.0]
```

Legacy compatibility forms are not supported.
