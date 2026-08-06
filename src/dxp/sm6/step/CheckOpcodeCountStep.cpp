#include "dxp/sm6/step/CheckOpcodeCountStep.hpp"
#include <any>
#include <expected>
#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include "dxp/Condition_impl.hpp"
#include "dxp/ResultFieldTraits.hpp"
#include "dxp/sm6/step/CheckOpcodeCountStep_impl.hpp"
#include "dxp/StepConcept.hpp"
#include "dxp/StepResults.hpp"
#include "dxp/ValidationContext.hpp"

#include "dxp/sm6/ExecutionContext.hpp"
#include "dxp/sm6/ShaderProgram.hpp"

namespace dxp::sm6::step {

std::expected<dxp::CheckOpcodeCountResults, std::string>
Execute(const CheckOpcodeCountStep& step, ExecutionContext& ctx) {
  auto [all_dxil, all_llvm] = ctx.program.GetOpcodeCounts();

  dxp::CheckOpcodeCountResults results;
  for (const auto& op_name : step.dxil_opcodes) {
    auto it = all_dxil.find(op_name);
    results.dxil_opcode_counts[op_name] = (it != all_dxil.end()) ? it->second : 0;
  }
  for (const auto& op_name : step.llvm_opcodes) {
    auto it = all_llvm.find(op_name);
    results.llvm_opcode_counts[op_name] = (it != all_llvm.end()) ? it->second : 0;
  }

  ctx.state[step.name] = true;
  ctx.results[step.name] = std::any(results);

  return results;
}

std::expected<void, std::string>
Validate(const CheckOpcodeCountStep& step, std::string& error, ValidationContext& ctx) {
  if (step.dxil_opcodes.empty() && step.llvm_opcodes.empty()) {
    error = "check_opcode_count step '" + step.name + "': at least one of dxil_opcodes or llvm_opcodes must be non-empty";
    return std::unexpected(std::move(error));
  }

  if (!ctx.names.insert(step.name).second) {
    error = "duplicate SM6 name '" + step.name + "' reused by step";
    return std::unexpected(std::move(error));
  }

  if (auto r = ValidateCondition<CheckOpcodeCountStep::Results>(step.condition, ctx); !r) {
    error = r.error();
    return std::unexpected(error);
  }
  return {};
}

std::string DescribeOutcome(const CheckOpcodeCountStep&, const dxp::CheckOpcodeCountResults& results,
                            const ExecutionContext& /*ctx*/) {
  const size_t count = results.dxil_opcode_counts.size() + results.llvm_opcode_counts.size();
  return std::format("counted {} opcode{}", count, count == 1 ? "" : "s");
}

static_assert(RecipeStep<CheckOpcodeCountStep>);
static_assert(ExecutableStep<CheckOpcodeCountStep, ExecutionContext>);

}  // namespace dxp::sm6::step
