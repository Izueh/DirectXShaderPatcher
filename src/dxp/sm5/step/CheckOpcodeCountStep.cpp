#include <any>
#include <cstddef>
#include <cstdint>
#include <dxp/sm5/step/CheckOpcodeCountStep.hpp>
#include <expected>
#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include "dxp/Condition_impl.hpp"
#include "dxp/ResultFieldTraits.hpp"
#include "dxp/sm5/Model.hpp"
#include "dxp/sm5/Model_impl.hpp"
#include "dxp/sm5/step/CheckOpcodeCountStep_impl.hpp"
#include "dxp/StepConcept.hpp"
#include "dxp/StepResults.hpp"
#include "dxp/ValidationContext.hpp"

#include "dxp/sm5/ExecutionContext.hpp"
#include "dxp/sm5/ShaderProgram.hpp"

namespace dxp::sm5::step {
using namespace dxp::sm5::model;

std::expected<dxp::CheckOpcodeCountResults, std::string>
Execute(const CheckOpcodeCountStep& step, ExecutionContext& ctx) {
  auto all_counts = ctx.program.GetOpcodeCounts();

  dxp::CheckOpcodeCountResults results;
  for (const auto op : step.opcodes) {
    const auto idx = static_cast<size_t>(static_cast<uint32_t>(op));
    if (idx < glz::meta<Opcode>::keys.size()) {
      const auto* const op_name = glz::meta<Opcode>::keys.at(idx);
      auto it = all_counts.find(op_name);
      results.opcode_counts[op_name] = (it != all_counts.end()) ? it->second : 0;
    }
  }

  ctx.state[step.name] = true;
  ctx.results[step.name] = std::any(results);

  return results;
}

std::expected<void, std::string>
Validate(const CheckOpcodeCountStep& step, std::string& error, dxp::ValidationContext& ctx) {
  if (step.opcodes.empty()) {
    error = "check_opcode_count step '" + step.name + "': opcodes must not be empty";
    return std::unexpected(std::move(error));
  }

  if (!ctx.names.insert(step.name).second) {
    error = "duplicate SM5 name '" + step.name + "' reused by step";
    return std::unexpected(std::move(error));
  }

  if (auto r = ValidateCondition<typename std::decay_t<decltype(step)>::Results>(step.condition, ctx); !r) {
    error = r.error();
    return std::unexpected(error);
  }
  return {};
}

std::string DescribeOutcome(const CheckOpcodeCountStep&, const dxp::CheckOpcodeCountResults& results,
                            const ExecutionContext& /*ctx*/) {
  const size_t count = results.opcode_counts.size();
  return std::format("counted {} opcode{}", count, count == 1 ? "" : "s");
}

static_assert(RecipeStep<CheckOpcodeCountStep>);
static_assert(ExecutableStep<CheckOpcodeCountStep, ExecutionContext>);

}  // namespace dxp::sm5::step
