# DXP ReShade add-on

A ReShade add-on that runs DirectXShaderPatcher recipes on a game's shaders at
runtime. It works with D3D11 and D3D12, and with SM5 (DXBC) and SM6 (DXIL) shaders.

The add-on captures every pipeline the game creates, patches its shaders in the
background, and swaps in the patched pipeline at the game's draws once it is ready.
Recipes can add textures (loaded from DDS files) and samplers.

It is meant to be useful as-is (drop in recipes and resources and it runs) and to
be a starting point for larger mods. See [Extending](#extending).

## Requirements

- ReShade **with full add-on support** (the "ReShade with full add-on support"
  installer). The regular build does not send the pipeline and draw events this
  add-on relies on.
- 64-bit games: `dxp.addon64`, which supports SM5 and SM6.
- 32-bit games: `dxp.addon32`, which supports SM5 only.

## Installation

Copy the add-on and a `dxp` folder next to the game executable, where ReShade's
DLL is:

```text
<game>/
  dxgi.dll (ReShade)
  dxp.addon64
  dxp/
    recipes/
      sm5/                 DXBC recipes that run on every stage
      sm5/ps/              DXBC recipes for pixel shaders only
      sm6/cs/              DXIL recipes for compute shaders only
      ...                  stage folders: vs hs ds gs ps cs as ms
    resources/
      noise.dds            textures referenced by add_resource handles
      samplers.ini         optional named samplers
    dump/                  written when "Dump patched shaders" is on
```

`example/dxp` in this folder contains this layout with sample recipes and a
sample `samplers.ini`. The sample recipes end in `.example`, so they don't load
until you rename them.

Recipes load on startup. Any `*.yml` or `*.yaml` file in a model folder is a
recipe. For each shader, the add-on runs every recipe that matches the shader's
model and stage, in file-name order. Each recipe's output becomes the next
recipe's input. Recipes that don't match a shader leave it unchanged, so one
recipe folder can cover a whole game.

## Overlay

The add-on's section in ReShade's **Add-ons** tab shows:

- **Enabled**: turns pipeline replacement on or off. Turning it off applies at the
  next draw; turning it on applies the next time the game binds each pipeline.
- **Reload recipes & resources**: re-reads `dxp/` from disk and re-patches every
  pipeline.
- **Re-patch**: runs the current recipes again, for example after changing a
  binding provider.
- Per-device counts of patched, pending, failed, and unmodified pipelines, plus
  recent errors. On D3D12 it also shows the injected descriptor ranges.
- One entry per recipe with its patch and failure counts and its last error.
- **Requirements**: a table of the resources the recipe's `add_resource` steps
  declare: handle, class, expected type (dimension, element type and width),
  register/space or `auto`, whether the step is conditional, and how the add-on
  would satisfy it (the DDS file found, with a warning when its dimension differs
  from the declaration; a built-in or `samplers.ini` sampler; "needs a binding
  provider" for UAVs and buffers). Hover a handle for its `meta.description` and
  other [hints](../../docs/resource_declarations.md#recommended-keys); invalid
  hint values are shown and logged as warnings.
- **Env variables**: an editor for every variable declared in the recipe's
  `env:` block, typed by its default value (bool, int, uint, or float). With
  **Apply env changes immediately** on, every committed edit re-patches.
  Otherwise press **Apply env changes**. **Save env values** writes the values to
  `ReShade.ini` so they apply on the next launch.
- The loaded textures and samplers.

## Resources

Each `add_resource` handle in a recipe resolves to a resource:

| Recipe declaration | Resolved from |
|---|---|
| texture (SRV) | `resources/<handle>`, `resources/<handle>.dds`, or a path relative to `resources/` |
| sampler | a `[handle]` section in `samplers.ini`, otherwise derived from the handle name (`point_wrap`, `linear_clamp`, `aniso_mirror`, ...; defaults to linear + clamp) |
| UAV, raw or structured buffer | a [binding provider](#extending) |
| constant buffer | not supported |

DDS files can use DX10 headers or the common legacy formats (BC1-BC5, RGBA8,
BGRA8, 16-bit and 32-bit float). They can be 1D, 2D, 3D, cube, or array textures,
with mip levels.

### Binding rules

**D3D11.** Resources bind to the register DXP assigned. Leave `register_index`
unset to let DXP pick a slot the shader doesn't use, or use `reverse_bind: true`
to take the highest free slot. The add-on saves the game's binding in that slot
before its first push and restores it at the next draw that doesn't use the slot,
at `FinishCommandList`, or at present. If the game binds that slot itself first,
the game's binding stays. Graphics-stage UAVs aren't supported.

**D3D12.** When the game creates a root signature, the add-on appends one
descriptor table each for SRVs, UAVs, and samplers in a dedicated register space.
By default that space is `space100`, with ranges `t0-t15`, `u0-u7`, and `s0-s7`.
Recipes must declare their resources inside those ranges:

```yaml
- kind: add_resource
  name: add_noise
  textures:
    - handle: noise
      kind: Texture2D
      space: 100
      register_index: 0
```

SM5.0 DXBC shaders on D3D12 can only use space 0. To use them, set
`InjectSpace=0` and move the bases (for example `InjectSrvBase=100`) to registers
the game doesn't use. If a root signature already uses the injected range, or the
tables would push it past the 64-DWORD limit, the add-on leaves that root
signature alone. Pipelines that use it then get no resource-adding patches, and
the reason is logged.

By default (`InjectLayouts=auto`), root signatures are only extended when a recipe
loaded at startup has an `add_resource` step. Use `InjectLayouts=always` if you
plan to add such recipes later with **Reload**.

## Settings

The add-on stores its settings in `ReShade.ini`, in the `[DXP]` section:

| Key | Default | Meaning |
|---|---|---|
| `Enabled` | `1` | Bind patched pipelines |
| `Path` | `dxp` | Root folder, relative to the add-on |
| `LogLevel` | `warning` | DXP log level: `error`, `warning`, `info`, `debug`, `trace` |
| `WorkerThreads` | `0` | Patch threads (`0` = a quarter of the cores, 1 to 4) |
| `AutoApplyEnv` | `1` | Re-patch when an env value is committed |
| `DumpShaders` | `0` | Write original and patched bytecode to `dxp/dump` |
| `InjectLayouts` | `auto` | `auto`, `always`, or `never` (D3D12) |
| `InjectSpace` | `100` | Register space of the injected tables (D3D12) |
| `InjectSrvBase` / `InjectSrvCount` | `0` / `16` | SRV range (D3D12) |
| `InjectUavBase` / `InjectUavCount` | `0` / `8` | UAV range (D3D12) |
| `InjectSamplerBase` / `InjectSamplerCount` | `0` / `8` | Sampler range (D3D12) |

Env overrides are stored in `[DXP.Env]` as `<recipe path>:<variable>=<value>`, for
example `sm6/ps/replace_noise_srv.recipe.yml:noise_mip=2`.

## How it works

| File | Role |
|---|---|
| `addon.cpp` | Add-on entry points (`AddonInit` and `AddonUninit`) |
| `runtime.cpp` | Pipeline capture, worker threads, root signature injection, bind and draw handling |
| `recipes.cpp` | Recipe loading, env overrides, chained patching with a per-generation cache |
| `resources.cpp` | DDS textures, samplers, and the binding provider registry |
| `requirements.hpp` | Parses recommended `meta:` keys into `ResourceHints`; formats declarations for the overlay |
| `dds.cpp` | DDS parser |
| `overlay.cpp` | ImGui settings UI |
| `config.cpp` | `ReShade.ini` settings |

Pipeline replacement follows the create-and-bind model that RenoDX uses:

1. `init_pipeline`: the add-on copies the pipeline description and deduplicates
   shader bytecode by hash, so a shader shared by thousands of PSOs is patched
   once.
2. Worker threads run the recipes, resolve the bindings the recipes added, and
   create the replacement with `device::create_pipeline`.
3. `bind_pipeline`: when a replacement is ready, the add-on records it as
   pending. The game's pipeline stays bound. Until a replacement is ready the
   game's pipeline is used, so patching never blocks loading.
4. `draw`/`dispatch`: the add-on resolves every resource the replacement needs
   (DDS files, samplers, binding providers). If all of them resolve, it binds the
   replacement (once per game bind) and pushes their descriptors. If one doesn't,
   it puts the game's pipeline back for that draw. The add-on never issues the
   draw itself, so the game's draw call runs as recorded.

Each reload or env change produces a new recipe *generation*. Every captured
pipeline is patched again against it, and the replacement is swapped atomically.
Old replacement pipelines and old resource sets are destroyed eight frames after
they're retired, so the GPU is never using an object when it's destroyed.

### Known limitations

- On D3D12, ReShade's `push_descriptors` binds ReShade's own descriptor heaps
  (RenoDX's view injection works the same way). The add-on switches back to the
  game's heaps by re-binding the game's most recent descriptor table. This works
  in practice, but strictly D3D12 treats tables bound before a heap switch as
  undefined.
- D3D10, D3D9, OpenGL, and Vulkan aren't supported. Ray tracing pipelines and
  pipeline libraries are skipped.
- If the game denies a stage root-signature access (the
  `DENY_*_ROOT_ACCESS` flags), that stage can't read the injected tables.

## Extending

The add-on is one DLL with no game-specific code. To build a larger mod, copy
`tools/reshade_addon` and add your logic next to it. The main extension point is
the **binding provider**. It supplies a descriptor at draw time instead of
loading a DDS file, so recipes can read game resources:

```cpp
// game_ao.cpp: lets recipes read the game's half-resolution AO texture as "game_ao".
#include "resources.hpp"

namespace {
std::atomic<uint64_t> g_ao_resource{0};
std::atomic<uint64_t> g_ao_view{0};

void OnInitResource(reshade::api::device*, const reshade::api::resource_desc& desc, const reshade::api::subresource_data*,
                    reshade::api::resource_usage, reshade::api::resource resource) {
  if (desc.type == reshade::api::resource_type::texture_2d && desc.texture.format == reshade::api::format::r8_unorm &&
      desc.texture.width == 960 && desc.texture.height == 540) {
    g_ao_resource = resource.handle;
  }
}

void OnDestroyResource(reshade::api::device* device, reshade::api::resource resource) {
  if (g_ao_resource.load() != resource.handle) return;
  g_ao_resource = 0;
  if (const uint64_t view = g_ao_view.exchange(0)) device->destroy_resource_view({view});
}
}  // namespace

void RegisterGameAo() {
  reshade::register_event<reshade::addon_event::init_resource>(OnInitResource);
  reshade::register_event<reshade::addon_event::destroy_resource>(OnDestroyResource);
  dxp::addon::RegisterBindingProvider("game_ao", [](const dxp::addon::DrawContext& ctx, const dxp::ResourceBinding&) -> uint64_t {
    if (g_ao_view == 0 && g_ao_resource != 0) {
      reshade::api::resource_view view = {0};
      ctx.cmd_list->get_device()->create_resource_view({g_ao_resource}, reshade::api::resource_usage::shader_resource,
                                                      reshade::api::resource_view_desc(reshade::api::format::r8_unorm), &view);
      g_ao_view = view.handle;
    }
    return g_ao_view;  // 0 = not available: this draw falls back to the original shader
  });
}
```

Call `RegisterGameAo()` from `AddonInit`, before any pipeline is patched. A recipe
that declares a texture with handle `game_ao` then binds the tracked texture.
Providers take precedence over files in `resources/`.

Other places to extend:

- `RecipeSet::Patch` (`recipes.cpp`) is the per-shader hook. It's the place to add
  shader filtering (for example by hash), extra `PatchOptions` env such as
  per-game constants, or bytecode caching to disk.
- `BuildBindings` and `HandleDraw` (`runtime.cpp`) decide how bindings reach the
  GPU. Constant-buffer injection would go here, as a fourth injected root
  parameter on D3D12 and a `push_constants` on D3D11.
- `DrawOverlay` (`overlay.cpp`) is where your mod's UI goes.

## Building

The add-on builds with the rest of the project, like the CLI:

```powershell
cmake --preset ninja-x64
cmake --build --preset ninja-x64-release --target dxp_reshade_addon
cmake --install out/build/ninja-x64 --config Release --component DirectXShaderPatcherReShadeAddon --prefix out/install
```

At configure time CMake downloads the ReShade and Dear ImGui headers it needs,
pinned to ReShade 6.7.1 and ImGui 1.92.5 (docking branch). ReShade's overlay header
requires that exact ImGui version. For offline builds, pass
`-DFETCHCONTENT_SOURCE_DIR_RESHADE=<path>` and
`-DFETCHCONTENT_SOURCE_DIR_IMGUI=<path>`. Pass `-DDXP_BUILD_RESHADE_ADDON=OFF` to
skip the add-on.
