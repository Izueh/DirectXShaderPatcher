// DirectXShaderPatcher ReShade add-on.
//
// Drop the add-on next to ReShade together with a "dxp" folder:
//
//   dxp/recipes/sm5/*.recipe.yml        DXBC recipes for every stage
//   dxp/recipes/sm6/ps/*.recipe.yml     DXIL recipes for pixel shaders only (vs/hs/ds/gs/ps/cs/as/ms)
//   dxp/resources/*.dds                 textures referenced by add_resource handles
//   dxp/resources/samplers.ini          optional named samplers
//
// Every pipeline the game creates is patched in the background and swapped in once ready.
// See README.md for the binding rules and how to extend this into a larger mod.

#include <dxp/Version.hpp>

#include "overlay.hpp"
#include "runtime.hpp"

extern "C" __declspec(dllexport) const char* NAME = "DirectXShaderPatcher";
extern "C" __declspec(dllexport) const char* DESCRIPTION =
    "Patches game shaders with DirectXShaderPatcher recipes (SM5/SM6, D3D11/D3D12) and binds extra textures and samplers.";

// ReShade calls these outside the loader lock, which is required to start and join threads.
extern "C" __declspec(dllexport) bool AddonInit(HMODULE addon_module, HMODULE reshade_module) {
  if (!reshade::register_addon(addon_module, reshade_module)) return false;
  dxp::addon::InitializeRuntime(addon_module);
  dxp::addon::RegisterEvents();
  reshade::register_overlay(nullptr, &dxp::addon::DrawOverlay);
  dxp::addon::Log(reshade::log::level::info, "DirectXShaderPatcher {} add-on loaded", DXP_VERSION_STRING);
  return true;
}

extern "C" __declspec(dllexport) void AddonUninit(HMODULE addon_module, HMODULE reshade_module) {
  reshade::unregister_overlay(nullptr, &dxp::addon::DrawOverlay);
  dxp::addon::UnregisterEvents();
  dxp::addon::ShutdownRuntime();
  reshade::unregister_addon(addon_module, reshade_module);
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) {
  return TRUE;
}
