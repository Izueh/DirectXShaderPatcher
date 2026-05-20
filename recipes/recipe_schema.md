# YAML Recipe Schema

Recipes are now YAML documents with schema version `1`.

Top-level shape:

```yaml
version: 1
options:
  restore_reflection: true
  refresh_resources: false
  verify_module: true
resources:
  textures: []
  texture_uavs: []
  cbuffers: []
  samplers: []
rewrite_rules: []
steps: []
```

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

## Steps

Execution order is defined only by `steps`.

Supported step kinds:

- `add_texture`
- `add_texture_uav`
- `add_cbuffer`
- `add_sampler`
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

`apply_rules` step fields:

- `rules`: required list of rewrite rule ids
- `mode`: optional, defaults to `MatchAll`; `apply_rules` only supports `MatchAll`
- `required`: optional, defaults to `true`; when `false`, zero matches do not fail the step

Examples:

```yaml
steps:
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