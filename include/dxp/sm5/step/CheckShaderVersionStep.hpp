#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "dxp/Condition.hpp"
#include "dxp/StepResults.hpp"

namespace dxp::sm5::step {

struct CheckShaderVersionStep {
  static constexpr std::string_view kind = "check_shader_version";
  using Results = dxp::CheckShaderVersionResults;
  std::string name;
  uint32_t major_version;
  uint32_t minor_version;
  bool required = true;
  std::optional<ConditionNode> condition;

  CheckShaderVersionStep(std::string name_val, uint32_t major, uint32_t minor, bool required, std::optional<ConditionNode> condition_val)
      : name(std::move(name_val)),
        major_version(major),
        minor_version(minor),
        required(required),
        condition(std::move(condition_val)) {}
};

}  // namespace dxp::sm5::step
