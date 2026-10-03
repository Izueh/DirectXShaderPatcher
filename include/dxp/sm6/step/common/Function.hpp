#pragma once

#include <string>
#include <vector>
#include "dxp/ExportTypes.hpp"

namespace dxp::sm6::step {

/// @brief A function signature type; scalar and void are supported initially.
struct FunctionType {
  enum class Kind : uint8_t { Void,
                              Scalar };
  Kind kind = Kind::Void;
  dxp::ComponentType component_type = dxp::ComponentType::Invalid;

  FunctionType() = default;
  explicit FunctionType(dxp::ComponentType component) : kind(Kind::Scalar), component_type(component) {}
};

/// @brief One ordered, named function parameter.
struct FunctionParameter {
  std::string name;
  FunctionType type;
};

/// @brief Recipe-level signature, resolved separately from the function body.
struct FunctionSignature {
  std::vector<FunctionParameter> params;
  FunctionType return_type;
};

}  // namespace dxp::sm6::step
