#pragma once
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "dxp/Condition.hpp"
#include "dxp/StepResults.hpp"

namespace dxp::sm6::step {

/// @brief Counts resource declarations (DXIL resource classes) in the shader program.
struct CheckResourceCountStep {
  static constexpr std::string_view kind = "check_resource_count";
  using Results = dxp::CheckResourceCountResults;
  std::string name;
  bool required = true;
  std::optional<ConditionNode> condition;

  CheckResourceCountStep(std::string name_val, bool required, std::optional<ConditionNode> condition_val)
      : name(std::move(name_val)), required(required), condition(std::move(condition_val)) {}
};

}  // namespace dxp::sm6::step
