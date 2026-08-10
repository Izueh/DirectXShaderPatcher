#pragma once

#include <dxp/sm5/step/CheckOpcodeCountStep.hpp>
#include <glaze/glaze.hpp>
#include "dxp/Condition_impl.hpp"
#include "dxp/sm5/ExecutionContext.hpp"
#include "dxp/StepConcept.hpp"
#include "dxp/StepResults.hpp"
#include "dxp/ValidationContext.hpp"

namespace dxp::sm5::step {
using namespace dxp::sm5::model;

/// @brief Execute the CheckOpcodeCountStep against the shader program.
/// @param step The step to execute.
/// @param ctx Execution context containing the shader program.
/// @return Results with opcode counts, or error message.
std::expected<dxp::CheckOpcodeCountResults, std::string> Execute(const CheckOpcodeCountStep& step, ExecutionContext& ctx);

/// @brief Validate the CheckOpcodeCountStep.
/// @param step The step to validate.
/// @param error Output error message on failure.
/// @param ctx Validation context.
/// @return void on success, error message on failure.
std::expected<void, std::string> Validate(const CheckOpcodeCountStep& step, std::string& error, dxp::ValidationContext& ctx);
/// @brief Formats the step's result as a Trace log message.
std::string DescribeOutcome(const CheckOpcodeCountStep& step, const dxp::CheckOpcodeCountResults& results, const ExecutionContext& ctx);

struct CheckOpcodeCountData {
  std::string kind = "check_opcode_count";
  std::string name;
  std::vector<Opcode> opcodes;
  dxp::ConditionData condition;
  bool required = true;

  /**
   * @brief Compile this YAML data into a CheckOpcodeCountStep.
   * @return Compiled step or error message.
   */
  auto Compile() const -> std::expected<CheckOpcodeCountStep, std::string> {
    auto cond = condition.Compile();
    return CheckOpcodeCountStep{name, opcodes, required, cond};
  }
};

}  // namespace dxp::sm5::step
