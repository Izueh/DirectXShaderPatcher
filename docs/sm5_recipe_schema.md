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

## Conditional Steps

Any SM5 step may define `if`.

Condition forms (exactly one per object):

- `state`
- `input`
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
- `if.input` reads from input variables.
- Comparison forms (`eq`/`ne`/`gt`/`gte`/`lt`/`lte`) require exactly one selector (`state` or `input`) plus literal `value`.
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

Notes:

- `from_handle` resolves declaration handles (`add_temp`, `add_input`, `add_output`, `add_texture`, `add_raw_resource`, `add_structured_resource`, `add_cbuffer`, `add_sampler`, `add_uav`).
- `from_handle` requires an explicit operand `type`.
- `bind_handle` was removed.
- `scratch` and `state_temp` are not part of schema version `1`.
- YAML selectors use `components.kind` + `components.value`; direct `mask`/`swizzle`/`select` YAML fields are not part of schema version `1`.
- Emit operands may use explicit `indices` or shorthand immediates arrays, but not both on the same operand.
- Shorthand immediates arrays are emit-only and rejected on match operands.

`indices` is an ordered list of index objects.

Index object fields:

- `any`
- `representation`: `immediate32`, `immediate64`, `relative`, `immediate32_plus_relative`, `immediate64_plus_relative`
- `immediate_lo`
- `immediate_hi`
- `capture`
- `match_capture`

Notes:

- `immediate_lo` and `immediate_hi` accept integer literals.
- Transitional replay-object form is accepted for `immediate_lo`: `immediate_lo: { from: <index_capture_name> }`.
- Replay-object form for `immediate_hi` is unsupported.

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
