#pragma once

#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>

#include <dxp/ExportTypes.hpp>

namespace dxp {

/// @brief Shared variable/state storage for execution contexts (SM5 + SM6).
///
/// `variables` holds user-provided inputs (recipe env) and `state` holds
/// inter-step communication published by steps. Both backends' execution
/// contexts derive from this to avoid duplicating the accessor surface.
struct VariableStore {
  /// Variables (user-provided inputs, matches SM5 naming).
  std::unordered_map<std::string, PrimitiveValue> variables;
  /// State (inter-step communication, set by steps).
  std::unordered_map<std::string, PrimitiveValue> state;

  void SetVariable(const std::string& name, PrimitiveValue value) {
    variables[name] = std::move(value);
  }

  bool UnsetVariable(const std::string& name) {
    return variables.erase(name) != 0;
  }

  [[nodiscard]] bool HasVariable(const std::string& name) const {
    return variables.contains(name);
  }

  [[nodiscard]] const PrimitiveValue* FindVariable(const std::string& name) const {
    auto it = variables.find(name);
    if (it == variables.end()) {
      return nullptr;
    }
    return &it->second;
  }

  PrimitiveValue* FindVariable(const std::string& name) {
    auto it = variables.find(name);
    if (it == variables.end()) {
      return nullptr;
    }
    return &it->second;
  }

  template <typename TValue>
  void SetState(const std::string& name, TValue value) {
    using T = std::decay_t<decltype(value)>;
    if constexpr (std::is_same_v<T, bool>) {
      state[name] = value;
    } else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
      state[name] = static_cast<int64_t>(value);
    } else if constexpr (std::is_floating_point_v<T>) {
      state[name] = static_cast<double>(value);
    } else {
      static_assert(!std::is_same_v<T, T>,
                    "SetState: unsupported value type — supported: bool, integral, floating-point");
    }
  }
};

}  // namespace dxp
