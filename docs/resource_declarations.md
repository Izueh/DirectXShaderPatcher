# Resource declarations, cbuffer fields and `meta`

Applies to SM5 and SM6 recipes. The schema pages
([sm5](sm5_recipe_schema.md), [sm6](sm6_recipe_schema.md)) link here.

DXP reports what a patched shader expects to be bound. Hosts (a ReShade add-on,
ShaderInjector, a build tool) decide how to provide it. DXP never creates
resources and never interprets `meta`.

## Reporting

| API | When | What |
|---|---|---|
| `Recipe::GetDeclaredResources()` | right after parsing, no shader needed | every `add_resource` declaration: step, handle, class, type, explicit register/space, cbuffer layout, `meta`, and `conditional` (the step has a `condition` or `required: false`) |
| `RecipeReport::new_bindings` | after `Execute` | what the run actually added, with the assigned register and space plus the same type info and `meta` |

Type info (`dxp::ResourceTypeInfo`) carries `dimension`, `element_type` and
`vector_width` (SM6 textures and UAVs), `structure_stride` (structured buffers)
and `cbuffer` (the field layout). Anything a shader model doesn't express is left
unset; for example, SM5 textures have no element type.

`dxp sm5|sm6 inspect <recipe>` prints the same information.

## Cbuffer fields

Both models accept a `fields` list on cbuffer declarations:

```yaml
cbuffers:
  - handle: grading
    fields:
      - name: exposure
        type: F32          # F32, U32 or I32
        default: 1.0       # number/bool broadcast to every component, or one number per component
        meta: { label: Exposure, min: 0, max: 4 }
      - name: tint
        type: F32
        width: 3           # components, 1-4 (default 1)
        default: [1, 0.9, 0.8]
      - name: frame_index
        type: U32
        offset: 16         # optional; see packing
```

Packing follows HLSL constant-buffer rules: 4-byte components in 16-byte rows,
and a field never straddles a row.
- A field without an `offset` goes right after the previous one, or at the next
  row when it doesn't fit.
- Explicit offsets must be multiples of 4, ascending, and must not straddle a row.
- The size defaults to the packed size rounded up to 16 bytes.
- An explicit SM6 `size`, or SM5 `elements` (16-byte rows), must be large enough
  for the fields. With fields, SM5 derives `elements` when it's omitted.

Shader bytecode has no field concept: SM5 reads `cb#[row].component` and SM6
reads `CBufferLoadLegacy(row)` followed by `extractvalue(component)`. Fields are
a recipe-side layout, used to resolve field references and to report the layout
to hosts.

### Reading a field by name

References are resolved when the recipe is loaded, against cbuffers declared
with `fields` in an *earlier* `add_resource` step. Unknown cbuffers or fields
are load errors.

SM5: an operand's `handle` takes `field` instead of `element_index`:

```yaml
- type: constant_buffer
  handle: { name: grading, field: tint }   # cb#[0].yzww when tint is at offset 4
```

The row becomes the element index. When the operand has no `components:`, a
scalar field is selected (`select`) and a vector field is swizzled with its last
component repeated. Explicit `components:` are used as written.

SM6: an emit entry with `cbuffer_field` and a `name`:

```yaml
- name: exposure
  cbuffer_field: { handle: grading, field: exposure }   # optional component: N for vector fields
```

It expands to a `CBufferLoadLegacy` named `<name>__cbuffer_row` and an
`extractvalue` named `<name>`. `replace_captured` is allowed and applies to the
extracted value.

Field references are a YAML convenience. Steps built through the C++ API must
use the row/component forms directly.

## `meta`

Every resource declaration (textures, UAVs, raw and structured buffers,
cbuffers, samplers) and every cbuffer field may carry a `meta:` value. Any YAML
is allowed (scalars, lists, nested maps). DXP returns it verbatim as
`dxp::MetaValue` and never acts on it, so patching is identical with or without it.

### Recommended keys

These are a convention for interoperability. DXP doesn't enforce them, and a
host may ignore or override any of them.

| key | applies to | meaning |
|---|---|---|
| `description` | any | human-readable purpose |
| `format` | textures, UAVs | DXGI-style format name, e.g. `r16g16b16a16_float` |
| `size` | textures, UAVs | `full`, `half`, `quarter`, `eighth` or `fixed` |
| `size_of` | textures, UAVs | what `full` refers to: `render_target` (default) or `backbuffer` |
| `width`, `height` | textures, UAVs | pixel size when `size: fixed` |
| `clear` | textures, UAVs | contents before the first write, e.g. `[0, 0, 0, 1]` |
| `label`, `min`, `max`, `step`, `widget`, `options` | cbuffer fields | UI hints for runtime-tunable values |

Put host-specific keys under a namespace, for example `meta: { reshade: { ... } }`.

Example of a recipe-produced output for later passes:

```yaml
uavs:
  - handle: hdr_scene
    kind: Texture2D
    element_type: F32
    vector_width: 4
    meta:
      description: Scene color before tonemapping
      format: r16g16b16a16_float
      size: full
      size_of: render_target
```
