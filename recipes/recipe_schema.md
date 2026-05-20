# YAML Recipe Schema

Recipes are now YAML documents with schema version `1`.

Top-level shape:

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

Top-level `prefilters` are optional lean probe patterns used only for fast shader triage.

Top-level `options` currently support only `restore_reflection`.

- Final resource refresh runs automatically after resource-binding mutations if the recipe has not already executed an explicit `refresh_resources` step.
- Final LLVM verification and DXC container validation run automatically after module mutations if the recipe has not already executed an explicit `verify_module` step.
- Use `refresh_resources` and `verify_module` as step kinds only when you need those operations to happen at a specific point inside the recipe.

## Resources

Texture resource:

```yaml
- id: fast_noise
  name: FASTNoiseTexture
  kind: Texture2DArray
  element: F32
  width: 2
  binding:
    bind: auto
    space: 50
```

CBuffer resource:

```yaml
- id: frame_constants
  name: ISFastFrameConstantsCB
  type: ISFastFrameConstants
  size: 16
  binding:
    bind: auto
    space: 0
  fields:
    - name: FrameIndex
      type: U32
      width: 1
      offset: 0
```

Sampler resource:

```yaml
- id: linear_sampler
  name: LinearSampler
  binding:
    bind: auto
    space: 0
```

## Rewrite Rules

Each rule contains:

```yaml
- id: rule_id
  name: optional_step_name
  match:
    opcode: Frc
    capture: root_capture
    replace: replaced_capture
    mode: Replace
    prune_dead: true
    prune_captures: []
    operands: []
  bindings: []
  emit: []
  replace_with: emitted_value_id
```

Use `replace_with_capture` instead of `replace_with` for pure capture-to-capture rewrites. Exactly one of those two fields is required.

Operand nodes are nested directly:

```yaml
- index: 1
  kind: dxop
  opcode: Dot2
  capture: dot_call
  operands:
    - index: 1
      kind: any
    - index: 2
      kind: constant_int
      value: 0
```

Supported operand kinds:

- `any`
- `constant_int`
- `instruction`
- `dxop`
- `resource_handle`

`resource_handle` operands may further constrain the resolved resource:

- `resource_class`: `SRV`, `UAV`, `CBuffer`, `Sampler`
- `resource_kind`: one of `Texture1D`, `Texture2D`, `Texture2DMS`, `Texture3D`, `TextureCube`, `Texture1DArray`, `Texture2DArray`, `Texture2DMSArray`, `TextureCubeArray`, `TypedBuffer`, `RawBuffer`, `StructuredBuffer`, `CBuffer`, `Sampler`, `TBuffer`, `RTAccelerationStructure`, `FeedbackTexture2D`, `FeedbackTexture2DArray`
- `resource_name`: exact `GetGlobalName()` match
- `resource_name_like`: LLVM regular expression applied to `GetGlobalName()`
- `bind`: expected register index
- `space`: expected register space

Notes:

- `resource_name` and `resource_name_like` are applied after the handle is resolved to a real DXIL resource.
- `resource_name_like` uses LLVM regex syntax. Invalid patterns are rejected while parsing the recipe.
- `resource_kind` may be used for textures, buffers, cbuffers, samplers, and other supported DXIL resource kinds, not only `Texture2D` and `Texture2DArray`.

Auxiliary bindings currently support only `dxop`:

```yaml
bindings:
  - kind: dxop
    capture: group_id_x
    opcode: GroupId
    operands:
      - index: 1
        kind: constant_int
        value: 0
```

Supported emit kinds:

- `create_handle`
- `annotate_handle`
- `call`
- `extract`
- `binop`
- `cast`

Emit operand kinds:

- `capture`
- `temporary`
- `constant_int`
- `resource`
- `undef`

## Prefilters

Prefilters are intentionally lighter than rewrite rules. They only describe a call pattern to probe for; they do not carry replacement state, bindings, or emit sequences.

```yaml
prefilters:
  - id: ign_signature_probe
    opcode: Frc
    capture: probe_root
    operands:
      - index: 1
        kind: dxop
        opcode: Dot2
```

Operand matching inside `prefilters` uses the same operand schema as rewrite-rule `match.operands`.

## Steps

Execution order is defined only by `steps`.

Supported step kinds:

- `add_texture`
- `add_texture_uav`
- `add_cbuffer`
- `add_sampler`
- `prefilter`
- `apply_rule`
- `apply_rules`
- `expect_texture`
- `expect_texture_uav`
- `expect_cbuffer`
- `refresh_resources`
- `prune_dead_code`
- `verify_module`

`apply_rule` step fields:

- `rule`: required rewrite rule id
- `mode`: optional, defaults to `First`; accepted values are `First`, `Last`, and `MatchAll`
- `required`: optional, defaults to `true`; when `false`, zero matches do not fail the step

`prefilter` step fields:

- `pattern`: exactly one of `pattern` or `patterns` is required
- `patterns`: exactly one of `pattern` or `patterns` is required

`prefilter` probes the referenced prefilter pattern or patterns without mutating the module. If any probe matches, execution continues with the next step. If no probe matches, the recipe stops successfully and skips all remaining steps. For multiple probes, `patterns` uses OR semantics: the recipe continues when any referenced prefilter matches.

`apply_rules` step fields:

- `rules`: required list of rewrite rule ids
- `mode`: optional, defaults to `MatchAll`; `apply_rules` only supports `MatchAll`
- `required`: optional, defaults to `true`; when `false`, zero matches do not fail the step

Examples:

```yaml
steps:
  - kind: prefilter
    patterns:
      - ign_signature_probe
      - blue_noise_signature_probe
  - kind: add_texture
    id: fast_noise
  - kind: apply_rule
    rule: ign_noise_rhs_inner
    mode: MatchAll
    required: false
  - kind: apply_rules
    rules:
      - ign_noise_rhs_inner
      - ign_noise_lhs_inner
    mode: MatchAll
    required: false
  - kind: verify_module
```

For simple identity-style matcher probes, prefer `mode: First` or `mode: Last` so the step selects one stable match instead of relying on a whole-pass side effect.