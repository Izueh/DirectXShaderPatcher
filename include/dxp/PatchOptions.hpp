#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include "dxp/ExportTypes.hpp"
#include "dxp/Logging.hpp"

namespace dxp {

/// @brief Runtime options for recipe execution.
struct PatchOptions {
  /// @brief Set an environment variable.
  void SetEnv(std::string key, PrimitiveValue value) {
    env_[std::move(key)] = value;
  }

  /// @brief Get an environment variable, or std::nullopt if not set.
  [[nodiscard]] std::optional<PrimitiveValue> GetEnv(const std::string& key) const {
    auto iter = env_.find(key);
    if (iter != env_.end()) {
      return iter->second;
    }
    return std::nullopt;
  }

  /// @brief Merges env vars into an existing map; later values win.
  /// Used to seed recipe/execution-context variables from this options object.
  void MergeEnvInto(std::unordered_map<std::string, PrimitiveValue>& target) const {
    for (const auto& [k, v] : env_) {
      target[k] = v;
    }
  }

  /// @brief Logging sink; when set, receives severity-tagged execution messages
  /// (shader load, per-step progress, serialization, warnings). Per-call object:
  /// each Execute invocation may install its own sink. Null discards messages.
  LogSink logger;

  /// @brief Logging level filter (messages at or below this level are emitted).
  LogLevel log_level = LogLevel::Warning;

 private:
  /// @brief Environment variables — dynamic key/value, used by conditions and immediates.
  std::unordered_map<std::string, PrimitiveValue> env_;
};

}  // namespace dxp
