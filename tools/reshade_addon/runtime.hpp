#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "common.hpp"
#include "config.hpp"
#include "recipes.hpp"
#include "resources.hpp"

namespace dxp::addon {

/// Loads settings and recipes. Call once after reshade::register_addon.
void InitializeRuntime(HMODULE addon_module);
/// Stops worker threads. Call before reshade::unregister_addon.
void ShutdownRuntime();

void RegisterEvents();
void UnregisterEvents();

// ── Overlay-facing API (render thread) ────────────────────────────────────────

[[nodiscard]] std::shared_ptr<const Settings> GetSettings();
void UpdateSettings(const Settings& settings);

[[nodiscard]] std::shared_ptr<const RecipeSet> GetRecipes();

/// Re-reads recipes and resources from disk and re-patches every captured pipeline.
void ReloadAll();
/// Publishes the same recipes with new env overrides and re-patches every pipeline.
void ApplyEnvOverrides(const std::unordered_map<std::string, EnvMap>& overrides);

struct DeviceInfo {
  api::device_api api = api::device_api::d3d11;
  bool layouts_injected = false;
  InjectionLayout injection;
  size_t pipelines = 0;
  size_t pending = 0;
  size_t patched = 0;
  size_t failed = 0;
  size_t unmodified = 0;
  uint32_t layouts_extended = 0;
  uint32_t layouts_skipped = 0;
  std::shared_ptr<const ResourceSet> resources;
  std::vector<std::string> recent_errors;
};
[[nodiscard]] std::vector<DeviceInfo> GetDeviceInfo();

}  // namespace dxp::addon
