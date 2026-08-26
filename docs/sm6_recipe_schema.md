# SM6 YAML Recipe Schema

JSON Schema companion: [sm6_recipe_schema.json](sm6_recipe_schema.json)

> The JSON schema is **generated** from the glaze meta that defines the parser
> surface (`tools/gen_schema.cpp`, run via the `schema_sync` ctest) and is the
> **authoritative field reference** (every key, enum value, and step shape).
> This page deliberately does **not** re-enumerate every field — it covers
> semantics, constraints, and worked examples. For any key-level question,
> read the generated JSON.
> When the YAML surface changes, regenerate with `gen_schema` and update this
> page to match.

SM6 recipes use schema version `1` and are specific to `dxp::sm6` / DXIL (LLVM
IR) patching.

## Recipe Structure

Every SM6 recipe is a YAML document with three top-level keys:

| key | required | meaning |
|---|---|---|
| `version` | optional, defaults to `1` | schema version (currently only `1` is supported) |
| `env` | optional | variable definitions accessible via conditions |
| `steps` | required | non-empty array of step objects, executed in order |

Each step has a unique `name` and a `kind` that determines its behaviour.

## Step Kinds

| kind | Purpose |
|---|---|
| `add_resource` | Declares textures, UAVs, cbuffers, samplers, and input/output signatures |
| `apply_rule` | Matches DXIL instructions and rewrites them (or probes without mutating) |
| `check_shader_version` | Filters unless the program shader model matches `major`/`minor` (mismatch is a non-error no-match) |
| `check_opcode_count` | Counts DXIL/LLVM opcodes in the entry function and publishes results |
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
| `is` | Truthy check on a named variable/state/result |
| `eq` / `ne` / `gt` / `gte` / `lt` / `lte` | Comparison — `lhs` vs `rhs`, both typed literals or variable references |
| `and` | All children must be true |
| `or` | At least one child must be true |
| `not` | Inverts the containing condition (default `false`) |

Comparison operands are typed: string values are treated as variable references
(resolved via dot notation or the variable store), everything else as a literal,
so both `eq: {lhs: count_ops.Frc, rhs: 3}` and `eq: {lhs: 3, rhs: count_ops.Frc}`
are valid. Missing values resolve to `false`.

```yaml
steps:
  - kind: check_opcode_count
    name: count_ops
    llvm_opcodes: [Frc, Trunc]
  - kind: apply_rule
    name: gate_on_frc
    condition:
      gt:
        lhs: count_ops.Frc
        rhs: 0
    rule:
      name: my_rule
      match:
        - opcode: Frc
```

## `add_resource`

| array key | element type | notable fields |
|---|---|---|
| `textures` | texture desc | `kind`, `element_type`, `vector_width`, `space`, `is_read_write` |
| `uavs` | texture desc | same as textures (`is_read_write: true`) |
| `cbuffers` | cbuffer desc | `size`, `type`, `fields[]` |
| `samplers` | sampler desc | — |
| `inputs` | input signature decl | `semantic_name`, `comp_type`, `vector_size`, `register_index`, `interp_mode` |
| `outputs` | output signature decl | `semantic_name`, `comp_type`, `vector_size`, `register_index` |

Common declaration fields:

- `handle` — unique identifier; referenced by `apply_rule` emit operands.
- `kind` — `DxilResourceKind` key (`Texture2D`, `Texture2DArray`, `RawBuffer`,
  `StructuredBuffer`, `CBuffer`, `Sampler`, …).
- `element_type` — `ComponentType` key (`F32`, `U32`, …).
- `vector_width` — element vector width (default 4).
- `space` — register space (default 0).
- `register_index` — explicit bind point, or omitted for auto-assign.

Cbuffer desc fields:

- `name` / `handle` — unique identifier.
- `size_in_bytes` / `size` — buffer size.
- `type` — schema type name.
- `fields` — list of `{ name, type, width, offset }`.

<details>
<summary>Example</summary>

```yaml
steps:
  - kind: add_resource
    name: add_resources
    textures:
      - handle: fast_noise
        kind: Texture2DArray
        space: 50
        element_type: F32
        vector_width: 2
    cbuffers:
      - handle: frame_constants
        space: 0
        size: 16
        type: ISFastFrameConstants
        fields:
          - name: FrameIndex
            type: U32
            width: 1
            offset: 0
```

</details>

## `apply_rule`

| field | meaning |
|---|---|
| `name` | required, unique |
| `required` | optional, default `true` — stop-fast on no-match; the step is reported failed (success=false) and remaining steps do not run |
| `match_mode` | optional: `first`, `last`, `match_all` (default `first`) |
| `rewrite_mode` | optional: `none`, `replace`, `before`, `after`, `replace_range` (default `replace`). `replace` swaps the **entire matched sequence** with the emit block (uses of the first matched instruction's result are pointed at the first emitted value unless `replace_captured` is wired); `replace_range` replaces a custom sub-range via `range_start_offset`/`range_end_offset` (default end `-1` = whole window); `before`/`after` insert the emit relative to `insert_index` without erasing |
| `insert_index` | insertion position (used with `before`/`after`; defaults to `0` for `before`, last match for `after`) |
| `range_start_offset` / `range_end_offset` | rewrite range (used with `rewrite_mode: replace_range`), relative to the first matched instruction |
| `rule` | required — the rule object |
| `condition` | optional guard |

`rule` fields:

| field | meaning |
|---|---|
| `name` | required |
| `match` | required, non-empty list of instruction match patterns — see **sequence matching** below |
| `emit` | optional list of emit instruction patterns |
| `prune` | optional, default `true` — prune dead instructions after the step |

### Sequence matching

When `match` contains **more than one** pattern, the rule matches a **consecutive
sequence** of instructions (same semantics as sm5): pattern 1 at position *i*,
pattern 2 at *i+1*, etc. Sequences are **block-local** — a sequence cannot span a
basic-block boundary (a terminator ends the block). Overlapping sequences are
all reported; `match_mode` selects among them. Captures are shared across the
whole sequence, so a later pattern can constrain a value captured by an earlier
one via `match_capture:`.

Capture semantics (match patterns): `capture:` stores the matched instruction
or operand under the given name; `match_capture:` constrains the match — the
matched value must equal a previously captured one (a prior step's global store,
or a same-match capture). Emit operands/instructions replay captures with
`capture:`; `match_capture` is match-only.

To match several *independent* instruction kinds, use separate rule steps —
`match` is a sequence, not an OR-list.

### Terminators and control flow

Matching a terminator (`ret`, `br`, …) is allowed with `rewrite_mode: before`
(e.g. "insert before `ret`"). `replace`/`replace_range`/`after` on a terminator
is refused with a clear error — a block cannot gain or lose a terminator.

### Replacement semantics (SSA)

`replace`/`replace_range` point uses of the **first matched instruction's result**
at the first emitted value (mirroring sm5's register semantics), then erase the
matched instructions. `emit[].replace_captured` overrides this: the named capture's
uses are rewired to the emitted value instead. `replace_captured` may reference
any capture from the rule's match **or a prior step's** (see cross-step captures).

### Cross-step captures

Each applied match merges its captures into a global capture store. Later steps
resolve `capture`/`replace_captured` names from the current match first, then
from the store — so a value captured in step 1 can be emitted or replaced in
step 2. Capture names shared with a prior step act as a match constraint (the
matched value must equal the stored one).

### Pruning

After all matches of a rule step are applied, dead instructions are pruned once
(never mid-iteration), for every rewrite mode. Instructions referenced by the
cross-step capture store are never pruned. `prune: false` disables this step's
pruning.

### Match instruction pattern

Fields: `opcode` (DXIL opcode name, e.g. `Frc`), `capture` (stores the matched
instruction), `operands` (list of operand patterns).

### Match operand pattern

Full field list (`index`/`kind`/`capture`/`instruction`/`constant_int_values`/
`constant_float_values`/`component_type`/`resource_class`/`resource_kind`/
`resource_name`/`resource_name_like`/`register_index`/`space`/`export_as`) is
in the generated JSON. Key semantics:

- `kind` selects the operand family: `call`, `constant`, or `resource`.
- `component_type` (optional) restricts constant matching to a specific
  `ComponentType` (e.g. `I32`); with no constant values it matches any constant
  of that type.
- `resource_*` fields describe resource handles; `register_index`/`space` match
  the binding.
- `export_as` publishes matched resource/immediate data into the patch report.

### Emit instruction pattern

Fields: `opcode`, `name`, `operands`, `result_component_type` (result
`ComponentType`), `cast_opcode`, `aggregate`/`extract_index`,
`capture`/`replace_captured`.

### Emit operand pattern

Fields: `index`, `kind` (`call`/`constant`/`resource`/`undefined`), `capture`,
`handle`, `instruction`, `constant_int_values`/`constant_float_values`,
`component_type`. `component_type` controls the emitted constant's type
(default: signature-derived; float shorthand defaults to `F32`).

<details>
<summary>Example</summary>

```yaml
steps:
  - kind: apply_rule
    name: emit_blue_noise
    rewrite_mode: before
    rule:
      name: emit_blue_noise_rule
      match:
        - opcode: Ret
      emit:
        - opcode: Call
          name: blue_noise
          operands:
            - index: 0
              handle: fast_noise
```

</details>

## `check_shader_version`

| field | meaning |
|---|---|
| `name` | required |
| `major` | required — expected major version |
| `minor` | required — expected minor version |

Fails when the program's shader model does not match. Publishes `major_version` /
`minor_version` results under the step name.

<details>
<summary>Example</summary>

```yaml
steps:
  - kind: check_shader_version
    name: require_sm6
    major: 6
    minor: 0
```

</details>

## `check_opcode_count`

| field | meaning |
|---|---|
| `name` | required |
| `dxil_opcodes` | optional list of DXIL opcode names |
| `llvm_opcodes` | optional list of LLVM opcode names |

Publishes per-opcode counts under the step name (accessible via dot notation,
e.g. `count_ops.Frc`).

<details>
<summary>Example</summary>

```yaml
steps:
  - kind: check_opcode_count
    name: count_ops
    llvm_opcodes: [Frc, Trunc, Call]
```

</details>

## `check_resource_count`

| field | meaning |
|---|---|
| `name` | required |

Counts resource declarations; publishes `textures` / `samplers` / `cbuffers` /
`uavs` / `total`.

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
- Captures persist across steps.
- Steps publish a boolean under their `name` and their results under the same
  name for dot-notation conditions.
