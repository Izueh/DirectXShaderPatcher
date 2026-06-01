# SM6 YAML Recipe Schema

JSON Schema companion: [sm6_recipe_schema.json](sm6_recipe_schema.json)

DXIL recipes use schema version `1`.

## Top-Level Shape

```yaml
version: 1
options:
  restore_reflection: true
resources:
  textures: []
  texture_uavs: []
  cbuffers: []
  samplers: []
prefilters: []
rewrite_rules: []
steps: []
```

Notes:

- Execution order is defined only by `steps`.
- `resources`, `prefilters` (probe definitions), and `rewrite_rules` declare named objects that steps reference later.
- `options.restore_reflection` defaults to `true`.
- Final resource refresh runs automatically after resource-binding mutations unless the recipe already performs an explicit `refresh_resources` step.
- Final LLVM verification and DXC container validation run automatically at the end of patching.

## Resources

Resource collections:

- `textures`
- `texture_uavs`
- `cbuffers`
- `samplers`

Resource shapes:

- Texture: `id`, `name`, `kind`, `element`, `width`, optional `binding`
- CBuffer: `id`, `name`, `type`, `size`, `fields`, optional `binding`
- CBuffer field: `name`, `type`, `width`, `offset`
- Sampler: `id`, `name`, optional `binding`

`binding` is optional on every resource and defaults to:

```yaml
binding:
  bind: auto
  space: 0
```

Example:

```yaml
resources:
  textures:
    - id: <texture_id>
      name: <texture_name>
      kind: <resource_kind>
      element: <component_type>
      width: <vector_width>
      binding:
        bind: <bind_or_auto>
        space: <space>
  cbuffers:
    - id: <cbuffer_id>
      name: <cbuffer_name>
      type: <cbuffer_type>
      size: <size_in_bytes>
      binding:
        bind: <bind_or_auto>
        space: <space>
      fields:
        - name: <field_name>
          type: <field_type>
          width: <field_width>
          offset: <byte_offset>
  samplers:
    - id: <sampler_id>
      name: <sampler_name>
```

## Probe Definitions (`prefilters`)

Prefilters are named probe patterns used to gate later mutating steps.

Shape:

- Required: `id`, `opcode`
- Optional: `name`, `capture`, `operands`

Example:

```yaml
prefilters:
  - id: <probe_id>
    opcode: <opcode>
    capture: <capture_name>
    operands:
      - index: <operand_index>
        kind: <operand_kind>
        opcode: <nested_opcode>
```

## Rewrite Rules

Rule shape:

- Rule required: `id`, `match`
- Rule optional: `name`, `bindings`, `emit`, `replace_with`, `replace_with_capture`
- Match required: `opcode`
- Match optional: `capture`, `mode`, `range_start_offset`, `range_end_offset`, `prune_dead`, `prune_captures`, `operands`

Supported rewrite modes:

- `None`
- `Replace`
- `ReplaceRange`

Rule payload constraints:

- A rule with `mode: None` must not define `emit`, `replace_with`, `replace_with_capture`, or `prune_captures`.
- A non-`None` rule with no rewrite payload must use `mode: None`.
- A rule with `emit` values must provide exactly one of `replace_with` or `replace_with_capture`.
- Omitted `prune_dead` defaults to `true`.
- `Replace` rewrites the full matched instruction window.
- `ReplaceRange` rewrites only the sub-window selected by `range_start_offset` and `range_end_offset` inside that matched window.
- `range_start_offset` and `range_end_offset` are valid only when `mode: ReplaceRange`.

Example:

```yaml
rewrite_rules:
  - id: <rule_id>
    name: <rule_name>
    match:
      opcode: <opcode>
      capture: <capture_name>
      mode: Replace
      prune_dead: <bool>
      prune_captures: []
      operands: []
    bindings: []
    emit: []
    replace_with: <emitted_value_id>
```

## Operands And Emits

Match operand shape:

- Required: `index`, `kind`
- Optional: `capture`, `value`, `opcode`, `resource_class`, `resource_kind`, `resource_name`, `resource_name_like`, `bind`, `space`, `operands`

Supported operand kinds:

- `any`
- `constant_int`
- `instruction`
- `dxop`
- `resource_handle`

Emit operand kinds:

- `capture`
- `temporary`
- `constant_int`
- `resource`
- `undef`

Supported emit kinds:

- `create_handle`
- `annotate_handle`
- `call`
- `extract`
- `binop`
- `cast`

Notes:

- `resource_name_like` uses LLVM regex syntax and invalid patterns are rejected while parsing.
- `resource_handle` operands may additionally constrain `resource_class`, `resource_kind`, `resource_name`, `resource_name_like`, `bind`, and `space`.
- `resource_kind` accepts canonical DXIL names (for example `Texture2DArray`) and also accepts `texture_2d_array` as an alias for `Texture2DArray`.
- Auxiliary `bindings` currently support only `kind: dxop`.
- `range_start_offset` is zero-based from the matched anchor instruction.
- `range_end_offset` is zero-based from the matched anchor instruction; `-1` defaults to the same instruction as `range_start_offset` in the current declarative matcher.

## Steps

Any step may define:

- `if.state`, `if.all`, `if.any`, and `if.not` optional condition fields

Supported step kinds:

- `add_texture`
- `add_texture_uav`
- `add_cbuffer`
- `add_sampler`
- `prefilter`
- `apply_rule`
- `apply_rules`
- `refresh_resources`
- `prune_dead_code`

Step semantics:

- Resource steps (`add_texture`, `add_texture_uav`, `add_cbuffer`, `add_sampler`) use `id` to reference an entry under `resources`.
- `apply_rule` uses `rule`, defaults `mode` to `first`, and accepts `first`, `last`, or `match_all`.
- `apply_rules` uses `rules`, defaults `mode` to `match_all`, and only supports `match_all`.
- `prefilter` requires exactly one of `pattern` or `patterns` and may optionally define `set` to choose the state key it writes.
- `required` defaults to `true` on `apply_rule` and `apply_rules`.
- `refresh_resources` and `prune_dead_code` take no extra fields.
- `if` may select one of `state`, `all`, or `any`; `not: true` negates the selected condition result.

Conditional step example:

```yaml
steps:
  - kind: prefilter
    pattern: <probe_id>
    set: expected_shader

  - kind: add_texture
    if:
      any:
        - state: expected_shader
        - state: fallback_shader
    id: <resource_id>
```

Example:

```yaml
steps:
  - kind: prefilter
    set: expected_shader
    patterns:
      - <probe_id_a>
      - <probe_id_b>
  - kind: add_texture
    if:
      state: expected_shader
    id: <resource_id>
  - kind: apply_rule
    if:
      all:
        - state: expected_shader
        - state: skip_rewrite
          not: true
    rule: <rule_id>
    mode: match_all
    required: false
  - kind: apply_rules
    if:
      state: expected_shader
    rules:
      - <rule_id_a>
      - <rule_id_b>
    mode: match_all
    required: false
```

Notes:

- `if.state` reads from `DxilRecipeContext::state`.
- `if.all` requires every nested condition to evaluate to true.
- `if.any` requires at least one nested condition to evaluate to true.
- `if.not: true` negates the result of `state`, `all`, or `any`.
- Missing state values are treated as `false`.
- `kind: prefilter` is a probe step; it does not stop the recipe directly and instead publishes a boolean state value for later guards.
- For simple identity-style matcher probes, prefer `mode: first` or `mode: last` on `apply_rule` so the step selects one stable match instead of depending on whole-pass side effects.
