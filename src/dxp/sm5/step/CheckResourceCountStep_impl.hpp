#pragma once

#include <dxp/sm5/step/CheckResourceCountStep.hpp>
#include <glaze/glaze.hpp>
#include "dxp/Condition_impl.hpp"
#include "dxp/sm5/ExecutionContext.hpp"
#include "dxp/StepConcept.hpp"
#include "dxp/StepResults.hpp"
#include "dxp/ValidationContext.hpp"

namespace dxp::sm5::step {

/// @brief Execute the CheckResourceCountStep against the shader program.
/// @param step The step to execute.
/// @param ctx Execution context containing the shader program.
/// @return Results with resource counts, or error message.
std::expected<dxp::CheckResourceCountResults, std::string> Execute(const CheckResourceCountStep& step, ExecutionContext& ctx);

/// @brief Validate the CheckResourceCountStep.
/// @param step The step to validate.
/// @param error Output error message on failure.
/// @param ctx Validation context.
/// @return void on success, error message on failure.
std::expected<void, std::string> Validate(const CheckResourceCountStep& step, std::string& error, dxp::ValidationContext& ctx);
/// @brief Formats the step's result as a Trace log message.
std::string DescribeOutcome(const CheckResourceCountStep& step, const dxp::CheckResourceCountResults& results, const ExecutionContext& ctx);

struct CheckResourceCountData {
  std::string kind = "check_resource_count";
  std::string name;
  dxp::ConditionData condition;
  bool required = true;

  /**
   * @brief Compile this YAML data into a CheckResourceCountStep.
   * @return Compiled step or error message.
   */
  auto Compile() const -> std::expected<CheckResourceCountStep, std::string> {
    auto cond = condition.Compile();
    return CheckResourceCountStep{name, required, cond};
  }
};

}  // namespace dxp::sm5::step
