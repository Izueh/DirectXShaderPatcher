#include "config.hpp"

#include <algorithm>
#include <charconv>
#include <thread>

namespace dxp::addon {

namespace {

constexpr const char* kSection = "DXP";
constexpr const char* kEnvSection = "DXP.Env";

std::optional<std::string> GetString(const char* section, const char* key) {
  char buffer[1024] = {};
  size_t size = sizeof(buffer);
  if (!reshade::get_config_value(nullptr, section, key, buffer, &size)) return std::nullopt;
  return std::string(buffer);
}

template <typename T>
void GetNumber(const char* key, T& value) {
  if (const auto text = GetString(kSection, key)) {
    T parsed{};
    const auto [ptr, ec] = std::from_chars(text->data(), text->data() + text->size(), parsed);
    if (ec == std::errc()) value = parsed;
  }
}

void GetBool(const char* key, bool& value) {
  if (const auto text = GetString(kSection, key)) value = (*text == "1" || *text == "true");
}

void SetString(const char* section, const char* key, const std::string& value) {
  reshade::set_config_value(nullptr, section, key, value.c_str());
}

std::filesystem::path ModuleDirectory(HMODULE module) {
  wchar_t path[MAX_PATH] = {};
  GetModuleFileNameW(module, path, MAX_PATH);
  return std::filesystem::path(path).parent_path();
}

}  // namespace

const char* LogLevelName(dxp::LogLevel level) {
  switch (level) {
    case dxp::LogLevel::Error:   return "error";
    case dxp::LogLevel::Warning: return "warning";
    case dxp::LogLevel::Info:    return "info";
    case dxp::LogLevel::Debug:   return "debug";
    case dxp::LogLevel::Trace:   return "trace";
  }
  return "warning";
}

Settings LoadSettings(HMODULE addon_module) {
  Settings settings;
  GetBool("Enabled", settings.enabled);
  GetBool("AutoApplyEnv", settings.auto_apply_env);
  GetBool("DumpShaders", settings.dump_shaders);
  GetNumber("WorkerThreads", settings.worker_threads);
  if (const auto level = GetString(kSection, "LogLevel")) {
    for (uint8_t i = 0; i <= static_cast<uint8_t>(dxp::LogLevel::Trace); ++i) {
      if (*level == LogLevelName(static_cast<dxp::LogLevel>(i))) settings.log_level = static_cast<dxp::LogLevel>(i);
    }
  }
  if (const auto mode = GetString(kSection, "InjectLayouts")) {
    if (*mode == "always") settings.inject_mode = InjectMode::kAlways;
    if (*mode == "never") settings.inject_mode = InjectMode::kNever;
  }
  GetNumber("InjectSpace", settings.injection.space);
  GetNumber("InjectSrvBase", settings.injection.srv_base);
  GetNumber("InjectSrvCount", settings.injection.srv_count);
  GetNumber("InjectUavBase", settings.injection.uav_base);
  GetNumber("InjectUavCount", settings.injection.uav_count);
  GetNumber("InjectSamplerBase", settings.injection.sampler_base);
  GetNumber("InjectSamplerCount", settings.injection.sampler_count);

  settings.root = ModuleDirectory(addon_module) / "dxp";
  if (const auto root = GetString(kSection, "Path"); root && !root->empty()) {
    std::filesystem::path configured(*root);
    settings.root = configured.is_absolute() ? configured : ModuleDirectory(addon_module) / configured;
  }

  if (settings.worker_threads == 0) {
    // DXIL patching runs LLVM passes and is memory hungry; a few workers keep load
    // screens fast without starving the game.
    settings.worker_threads = std::clamp(std::thread::hardware_concurrency() / 4, 1u, 4u);
  }
  return settings;
}

void SaveSettings(const Settings& settings) {
  SetString(kSection, "Enabled", settings.enabled ? "1" : "0");
  SetString(kSection, "AutoApplyEnv", settings.auto_apply_env ? "1" : "0");
  SetString(kSection, "DumpShaders", settings.dump_shaders ? "1" : "0");
  SetString(kSection, "LogLevel", LogLevelName(settings.log_level));
}

std::string FormatValue(const dxp::PrimitiveValue& value) {
  return std::visit(
      [](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, bool>) {
          return v ? "true" : "false";
        } else {
          return std::format("{}", v);
        }
      },
      value);
}

std::optional<dxp::PrimitiveValue> LoadEnvOverride(const std::string& recipe, const std::string& variable,
                                                  const dxp::PrimitiveValue& default_value) {
  const std::string key = recipe + ":" + variable;
  const auto text = GetString(kEnvSection, key.c_str());
  if (!text || text->empty()) return std::nullopt;

  return std::visit(
      [&](const auto& def) -> std::optional<dxp::PrimitiveValue> {
        using T = std::decay_t<decltype(def)>;
        if constexpr (std::is_same_v<T, bool>) {
          return dxp::PrimitiveValue(*text == "true" || *text == "1");
        } else {
          T parsed{};
          const auto [ptr, ec] = std::from_chars(text->data(), text->data() + text->size(), parsed);
          if (ec != std::errc()) return std::nullopt;
          return dxp::PrimitiveValue(parsed);
        }
      },
      default_value);
}

void SaveEnvOverride(const std::string& recipe, const std::string& variable, const std::optional<dxp::PrimitiveValue>& value) {
  const std::string key = recipe + ":" + variable;
  // An empty value means "use the recipe default".
  SetString(kEnvSection, key.c_str(), value ? FormatValue(*value) : std::string());
}

}  // namespace dxp::addon
