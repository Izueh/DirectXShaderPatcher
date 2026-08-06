#pragma once

#include <dxp/sm5/step/CheckShaderVersionStep.hpp>
#include <glaze/glaze.hpp>
#include "dxp/Condition_impl.hpp"
#include "dxp/sm5/ExecutionContext.hpp"
#include "dxp/StepConcept.hpp"
#include "dxp/StepResults.hpp"
#include "dxp/ValidationContext.hpp"

namespace dxp::sm5::step {

/// @brief Execute the CheckShaderVersionStep against the shader program.
/// @param step The step to execute.
/// @param ctx Execution context containing the shader program.
/// @return Results with version info, or error message if versions mismatch.
std::expected<dxp::CheckShaderVersionResults, std::string> Execute(const CheckShaderVersionStep& step, ExecutionContext& ctx);

/// @brief Validate the CheckShaderVersionStep.
/// @param step The step to validate.
/// @param error Output error message on failure.
/// @param ctx Validation context.
/// @return void on success, error message on failure.
std::expected<void, std::string> Validate(const CheckShaderVersionStep& step, std::string& error, dxp::ValidationContext& ctx);
/// @brief Formats the step's result as a Trace log message.
std::string DescribeOutcome(const CheckShaderVersionStep& step, const dxp::CheckShaderVersionResults& results, const ExecutionContext& ctx);

/// @brief YAML deserialization struct for CheckShaderVersionStep.
struct CheckShaderVersionData {
  std::string kind = "check_shader_version";
  std::string name;
  int32_t major = -1;
  int32_t minor = -1;
  dxp::ConditionData condition;
  bool required = true;

  /// @brief Compile this YAML data into a CheckShaderVersionStep.
  auto Compile() const -> std::expected<CheckShaderVersionStep, std::string> {
    auto cond = condition.Compile();
    if (major < 0) {
      return std::unexpected("check_shader_version step '" + name + "': major version is required");
    }
    if (minor < 0) {
      return std::unexpected("check_shader_version step '" + name + "': minor version is required");
    }
    return CheckShaderVersionStep{name, static_cast<uint32_t>(major), static_cast<uint32_t>(minor),
                                  required, cond};
  }
};

}  // namespace dxp::sm5::step
