# SM5 YAML Recipe Schema

JSON Schema companion: [sm5_recipe_schema.json](sm5_recipe_schema.json)

> The JSON schema is **generated** from the glaze meta that defines the parser
> surface (`tools/gen_schema.cpp`, run via the `schema_sync` ctest) and is the
> **authoritative field reference** (every key, enum value, and step shape).
> This page deliberately does **not** re-enumerate every field — it covers
> semantics, constraints, and worked examples. For any key-level question,
> read the generated JSON.
> When the YAML surface changes, regenerate with `gen_schema` and update this
> page to match.

SM5 recipes use schema version `1` and are specific to `dxp::sm5` / DXBC token
IR patching.

## Recipe Structure

Every SM5 recipe is a YAML document with three top-level keys:

| key | required | meaning |
|---|---|---|
| `version` | optional, defaults to `1` | schema version (currently only `1` is supported) |
| `env` | optional | variable definitions accessible via conditions |
| `steps` | required | non-empty array of step objects, executed in order |

Each step has a unique `name` and a `kind` that determines its behaviour.

## Step Kinds

| kind | Purpose |
|---|---|
| `add_resource` | Declares resources (textures, raw/structured buffers, cbuffers, samplers, UAVs, input/output signatures, temps) |
| `apply_rule` | Matches instruction patterns and rewrites them (or probes without mutating) |
| `check_shader_version` | Filters unless the program shader model matches `major`/`minor` (mismatch is a non-error no-match) |
| `check_opcode_count` | Counts occurrences of requested opcodes and publishes results |
| `check_resource_count` | Counts resource declarations and publishes results |

## Common Step Fields

Every step accepts:

- `name` (required) — must be unique across the recipe.
- `condition` (optional) — conditional guard; see [Conditions](#conditions).
- `required` (optional, default `true`) — stop-fast mechanism, **not** an error
  mechanism:
  - `apply_rule` with zero matches, or `check_shader_version` with a version
    mismatch, publishes `state[step] = false` (dependent steps gated on the
    step name skip) and logs an Info no-match outcome. When `required` is
    `true` the recipe run stops early — the remaining steps do not execute and
    the output is an unmodified pass-through, but `Execute` still succeeds.
  - `add_resource` failures (e.g. bind-point overflow) are hard errors that
    abort the run when `required` is `true`; when `false` the failing
    declaration is skipped with a Warning and execution continues.

## Conditions

Exactly one of the following keys per condition object (plus optional `not`):

| key | meaning |
|---|---|
| `is` | Truthy check — the named variable/state/result resolves to a truthy value |
| `eq` / `ne` / `gt` / `gte` / `lt` / `lte` | Comparison — `lhs` vs `rhs`, both typed literals or variable references |
| `and` | All children must be true |
| `or` | At least one child must be true |
| `not` | Inverts the containing condition (default `false`) |

Comparison operands are typed: string values are treated as variable references
(resolved via dot notation or the variable store), everything else as a literal,
so both `eq: {lhs: count_ops.mov, rhs: 0}` and `eq: {lhs: 0, rhs: count_ops.mov}`
are valid. Missing values resolve to `false`.

## `add_resource`

Declares resources (textures, raw/structured buffers, cbuffers, samplers, UAVs,
input/output signatures, temps). All declaration arrays use the same element
shape; only the relevant fields are honored per declaration type:

| field | applies to | meaning |
|---|---|---|
| `handle` | all | unique identifier; referenced by `apply_rule` emit operands via `handle` |
| `register_index` | all | explicit bind point, or omitted for auto-assign |
| `reverse_bind` | all | auto-assign in reverse order: take the **highest** free slot (scan down from the max) instead of the lowest. Game shaders commonly occupy the low registers, so taking the lowest free slot can collide with the game's own bindings (and those overrides are never restored) — reverse order lands the patched resource far from the game's usage (e.g. t127 for textures). |
| `elements` | cbuffers | number of 16-byte elements |
| `interpolation` | inputs | `InterpolationMode` key (`linear`, `constant`, …) |
| `mode` | samplers | `SamplerMode` key (`default`, `comparison`, `mono`) |
| `kind` | uavs | `typed`, `raw`, or `structured` |
| `stride` | structured_resources, uavs | byte stride (required for structured resources) |
| `dimension` | textures | `ResourceDimension` key (`texture3d`, `texture2darray`, etc.) |
| `globally_coherent` | uavs | bool |
| `has_counter` | uavs | bool |

`temps` is a flat list of string handles.

Register limits: textures/raw/structured ≤ 127, cbuffers ≤ 14, samplers ≤ 15,
uavs ≤ 63, inputs ≤ 31, outputs ≤ 7.

<details>
<summary>Example</summary>

```yaml
steps:
  - kind: add_resource
    name: my_declarations
    textures:
      - handle: my_texture
    samplers:
      - handle: my_sampler
        register_index: 11
        mode: comparison
    temps: [temp0, temp1]
```

</details>

## `apply_rule`

Matches instruction patterns and rewrites them (or probes without mutating).

| field | meaning |
|---|---|
| `name` | required, unique |
| `required` | optional, default `true` — stop-fast on no-match; the step is reported failed (success=false) and remaining steps do not run |
| `match_mode` | optional: `first`, `last`, `match_all` (default `first`) |
| `rewrite_mode` | optional: `none`, `replace`, `before`, `after`, `replace_range` (default `replace`). `replace` swaps the **entire matched sequence** with the emit block; `replace_range` replaces a custom sub-range via `range_start_offset`/`range_end_offset` (default end `-1` = whole window); `before`/`after` insert the emit relative to `insert_index` without erasing |
| `insert_index` | insertion position (used with `before`/`after`; defaults to `0` for `before`, last match for `after`) |
| `range_start_offset` / `range_end_offset` | rewrite sub-range for `rewrite_mode: replace_range`, relative to the first matched instruction; `range_end_offset: -1` (default) extends to the last matched instruction |
| `rule` | required — the rule object |
| `condition` | optional guard |

`rule` fields:

| field | meaning |
|---|---|
| `match` | required, non-empty list of instruction match patterns |
| `emit` | optional list of emit instruction patterns |

### Match instruction pattern

| field | meaning |
|---|---|
| `opcode` | opcode to match (SM5 canonical name, e.g. `mov`) |
| `capture` | stores the matched instruction under this name |
| `saturate` | optional bool |
| `interpolation` | optional `InterpolationMode` |
| `test_boolean` | optional int |
| `operands` | list of operand patterns |
| `dimension` | optional `ResourceDimension` — matches declaration dimension |
| `return_type` | optional array of 4 `ResourceReturnType` — matches per-component return types |
| `structure_stride` | optional uint — matches structured buffer stride |
| `access_pattern` | optional `CbufferAccessPattern` — matches cbuffer access pattern |
| `mode` | optional `SamplerMode` — matches sampler mode |
| `uav_flags` | optional uint — matches UAV flags |

### Emit instruction pattern

| field | meaning |
|---|---|
| `opcode` | opcode to emit (mutually exclusive with `capture`) |
| `capture` | emits a previously captured instruction |
| `saturate` / `interpolation` / `test_boolean` | optional overrides |
| `operands` | list of emit operand patterns |
| `dimension` | optional `ResourceDimension` — emitted declaration dimension |
| `return_type` | optional array of 4 `ResourceReturnType` — emitted per-component return types |
| `structure_stride` | optional uint — emitted structured buffer stride |
| `access_pattern` | optional `CbufferAccessPattern` — emitted cbuffer access pattern |
| `mode` | optional `SamplerMode` — emitted sampler mode |
| `uav_flags` | optional uint — emitted UAV flags |

### Operand pattern (match and emit)

Full field list (any/type/indices/immediates_u32–f64/handle/components/modifier/
capture/match_capture/export_as) is in the generated JSON. Key semantics:

- An operand may use explicit `indices` OR shorthand immediates arrays
  (`immediates_u32` … `immediates_f64`), not both.
- Shorthand entries are literals or variable names (resolved from recipe env).
- `handle` resolves a declaration handle from an `add_resource` step (emit only)
  and requires an explicit `type`.
- `capture` names the operand: in a match pattern it stores the matched operand
  under that name; in an emit operand it replays a previously captured operand
  (same-match first, then the cross-step global store).
- `match_capture` (match patterns only) constrains the match: the operand must
  equal a previously captured operand (a prior step's global store, or a
  same-match capture). It is rejected in emit patterns.
- `export_as` publishes the operand's resource/immediate data into the patch report.

### Index pattern

Fields: `any`, `representation` (`immediate32` … `immediate64_plus_relative`),
`immediate_lo`/`immediate_hi`, `capture`/`match_capture`, `relative_operand`
(nested operand pattern for relative addressing). `relative_operand` requires a
`relative`-style representation and cannot nest; it must use explicit `indices`
(no shorthand arrays).

### Instruction-level fields for declaration opcodes

Declaration opcodes (`dcl_resource`, `dcl_constant_buffer`, `dcl_sampler`,
`dcl_unordered_access_view_*`) encode dimension, return type, access pattern,
and mode directly in their instruction tokens (Token0, Token2). Recipes can
specify these via dedicated instruction-level fields instead of extended
opcodes:

| field | valid opcodes | meaning |
|---|---|---|
| `dimension` | `dcl_resource`, `dcl_unordered_access_view_*` | `ResourceDimension` key (`texture2d`, `texture2darray`, `texture3d`, etc.) |
| `return_type` | `dcl_resource`, `dcl_unordered_access_view_*` | array of 4 `ResourceReturnType` keys (`float`, `uint`, `snorm`, etc.) |
| `structure_stride` | `dcl_resource`, `dcl_unordered_access_view_*` | byte stride for structured buffers |
| `access_pattern` | `dcl_constant_buffer` | `CbufferAccessPattern` key (`immediate_indexed`, `dynamic_indexed`) |
| `mode` | `dcl_sampler` | `SamplerMode` key (`default`, `comparison`, `mono`) |
| `uav_flags` | `dcl_unordered_access_view_*` | raw uint32 (coherency/UAV flags) |

When any instruction-level field is present, the operand count validation is
relaxed: only the register operand is required (the rest is encoded via the
fields). Fields are validated at parse time — `dimension` on a non-resource
declaration, `mode` on a non-sampler, etc. are rejected.

### Extended opcodes

SM5 resource-access opcodes (`ld`, `sample` family, `gather4` family, `resinfo`,
and `ld_raw`/`ld_structured`) canonically carry chained extended-opcode tokens
(`sample_controls` offsets, `resource_dim`, `resource_return_type`). The engine
keeps emitted chains canonical automatically; `extended_opcodes` lets recipes
match and control them explicitly.

**Match** (on `match` entries): absent = wildcard (any chain, including none);
empty list = the instruction must carry no extended tokens; otherwise the list
must match the instruction's chain exactly (count + per-entry rule). Entries:

| entry | meaning |
|---|---|
| `- any: true` | position wildcard (matches any token at this position) |
| `- raw: <uint32>` | exact 32-bit token |
| `- type: sample_controls` | type-only match |
| `- type: sample_controls` + `sample_controls: {u,v,w}` | type + offsets |
| `- type: resource_dim` + `resource_dim: {dimension, structure_stride}` | type + dim/stride |
| `- type: resource_type` + `resource_return_type: [t0,t1,t2,t3]` | type + return types |

**Emit** (on `emit` entries): entries are verbatim (chain bit 31 is assigned by
the engine from the final position). `raw: <uint32>` emits the token exactly;
`type:` entries carry one of `sample_controls` / `resource_dim` /
`resource_return_type` with the matching payload. Members of the canonical
ResourceDim + ResourceReturnType pair omitted from the chain are synthesized
from the resource declaration (or fixed metadata for `ld_raw`/`ld_structured`)
in canonical order; a resource-access emit with no resolvable declaration is a
hard error. Typed entries must be canonical-ordered and non-duplicated.
`sample_controls` offsets are 4-bit two's complement (−8..7) and only valid on
sample/gather4-family opcodes; extended opcodes are rejected on opcodes whose
canonical chain carries none (e.g. `mov`, `store_raw`).

<details>
<summary>Example</summary>

```yaml
steps:
  - kind: apply_rule
    name: replace_frc_with_mov
    match_mode: match_all
    rewrite_mode: replace
    rule:
      match:
        - opcode: frc
          operands:
            - type: temp
              capture: dst
            - type: temp
              capture: src
      emit:
        - opcode: mov
          operands:
            - type: temp
              capture: dst
            - type: temp
              capture: src
```

</details>

## `check_shader_version`

Filters unless the program shader model matches `major`/`minor` (mismatch is a
non-error no-match).

| field | meaning |
|---|---|
| `major` | required — expected major version |
| `minor` | required — expected minor version |

Fails when the program's shader model does not match. Publishes `major_version` /
`minor_version` results under the step name.

<details>
<summary>Example</summary>

```yaml
steps:
  - kind: check_shader_version
    name: require_sm5
    major: 5
    minor: 0
```

</details>

## `check_opcode_count`

Counts occurrences of requested opcodes and publishes results.

| field | meaning |
|---|---|
| `opcodes` | required, non-empty list of SM5 opcode names |

Publishes per-opcode counts under the step name (accessible via dot notation,
e.g. `count_ops.mov`).

<details>
<summary>Example</summary>

```yaml
steps:
  - kind: check_opcode_count
    name: count_ops
    opcodes: [mov, add, mul]
```

</details>

## `check_resource_count`

Counts resource declarations and publishes results.

| field | meaning |
|---|---|

Counts textures, samplers, cbuffers, uavs, and thread groups; publishes
`textures` / `samplers` / `cbuffers` / `uavs` / `thread_groups` / `total`.

<details>
<summary>Example</summary>

```yaml
steps:
  - kind: check_resource_count
    name: resource_counts
```

</details>

## Captures and State

- All match/emit captures share one global namespace across the recipe;
  duplicates are rejected at parse time.
- Captures persist across steps (operands, instructions, index values).
- Steps publish a boolean under their `name` and their results under the same
  name for dot-notation conditions.
