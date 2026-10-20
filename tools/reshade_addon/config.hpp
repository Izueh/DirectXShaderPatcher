#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "common.hpp"
#include "dxp/ExportTypes.hpp"

namespace dxp::addon {

/// When D3D12 root signatures get the injected descriptor tables.
enum class InjectMode : uint8_t {
  kAuto,    // only when a loaded recipe has an add_resource step
  kAlways,  // always (lets recipes added by a later reload bind resources)
  kNever,
};

/// Fixed descriptor layout appended to D3D12 root signatures. Recipe-added
/// resources must be declared inside these ranges (same register space).
/// D3D11 binds by slot and ignores this.
struct InjectionLayout {
  uint32_t space = 100;
  uint32_t srv_base = 0;
  uint32_t srv_count = 16;
  uint32_t uav_base = 0;
  uint32_t uav_count = 8;
  uint32_t sampler_base = 0;
  uint32_t sampler_count = 8;
};

struct Settings {
  bool enabled = true;
  bool auto_apply_env = true;
  bool dump_shaders = false;
  dxp::LogLevel log_level = dxp::LogLevel::Warning;
  uint32_t worker_threads = 0;  // 0 = pick from hardware concurrency
  InjectMode inject_mode = InjectMode::kAuto;
  InjectionLayout injection;

  /// Root folder holding recipes/, resources/ and dump/. Defaults to "dxp" next to the add-on.
  std::filesystem::path root;

  [[nodiscard]] std::filesystem::path RecipeDir() const { return root / "recipes"; }
  [[nodiscard]] std::filesystem::path ResourceDir() const { return root / "resources"; }
  [[nodiscard]] std::filesystem::path DumpDir() const { return root / "dump"; }
};

/// Settings live in the global ReShade.ini, section [DXP]. Missing keys keep defaults.
Settings LoadSettings(HMODULE addon_module);
void SaveSettings(const Settings& settings);

/// Recipe env overrides live in section [DXP.Env] as "<recipe name>:<variable>=<value>".
/// The stored text is parsed with the type of the recipe's default value.
std::optional<dxp::PrimitiveValue> LoadEnvOverride(const std::string& recipe, const std::string& variable,
                                                  const dxp::PrimitiveValue& default_value);
void SaveEnvOverride(const std::string& recipe, const std::string& variable, const std::optional<dxp::PrimitiveValue>& value);

std::string FormatValue(const dxp::PrimitiveValue& value);
const char* LogLevelName(dxp::LogLevel level);

}  // namespace dxp::addon
