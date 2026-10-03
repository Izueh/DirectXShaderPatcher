#pragma once

#include "dxp/Condition.hpp"
#include "dxp/sm6/step/common/Function.hpp"
#include "dxp/sm6/step/common/Rule.hpp"
#include "dxp/StepResults.hpp"

namespace dxp::sm6::step {

/// @brief Defines a callable DXIL helper using the shared emit instruction language.
struct DeclareFunctionStep {
  static constexpr std::string_view kind = "declare_function";
  using Results = dxp::DeclareFunctionResults;

  std::string name;
  bool required = true;
  std::optional<ConditionNode> condition;
  FunctionSignature signature;
  std::vector<EmitPattern> emits;
  /// @brief Required for a scalar return; omitted for void. Uses body-local values.
  std::optional<EmitOperand> return_value;
};

}  // namespace dxp::sm6::step
