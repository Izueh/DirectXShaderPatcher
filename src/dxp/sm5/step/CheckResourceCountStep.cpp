#include <any>
#include <cstdint>
#include <dxp/sm5/step/CheckResourceCountStep.hpp>
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
#include "dxp/sm5/step/CheckResourceCountStep_impl.hpp"
#include "dxp/StepConcept.hpp"
#include "dxp/StepResults.hpp"
#include "dxp/ValidationContext.hpp"

#include "dxp/sm5/ExecutionContext.hpp"
#include "dxp/sm5/ShaderProgram.hpp"

namespace dxp::sm5::step {
using namespace dxp::sm5::model;

std::expected<dxp::CheckResourceCountResults, std::string>
Execute(const CheckResourceCountStep& step, ExecutionContext& ctx) {
  int32_t textures = 0;
  int32_t samplers = 0;
  int32_t cbuffers = 0;
  int32_t uavs = 0;
  int32_t thread_groups = 0;

  for (const auto& instr : ctx.program.instructions) {
    if (!OpcodeIsDeclaration(instr.opcode)) continue;
    const auto op = instr.opcode;
    if (op == dxp::sm5::Opcode::DclResource || op == dxp::sm5::Opcode::DclResourceRaw || op == dxp::sm5::Opcode::DclResourceStructured) {
      textures++;
    } else if (op == dxp::sm5::Opcode::DclSampler) {
      samplers++;
    } else if (op == dxp::sm5::Opcode::DclConstantBuffer) {
      cbuffers++;
    } else if (op == dxp::sm5::Opcode::DclUnorderedAccessViewTyped || op == dxp::sm5::Opcode::DclUnorderedAccessViewRaw || op == dxp::sm5::Opcode::DclUnorderedAccessViewStructured) {
      uavs++;
    } else if (op == dxp::sm5::Opcode::DclThreadGroup || op == dxp::sm5::Opcode::DclThreadGroupSharedMemoryRaw || op == dxp::sm5::Opcode::DclThreadGroupSharedMemoryStructured) {
      thread_groups++;
    }
  }

  const int32_t total = textures + samplers + cbuffers + uavs + thread_groups;
  dxp::CheckResourceCountResults results{.textures = textures, .samplers = samplers, .cbuffers = cbuffers, .uavs = uavs, .thread_groups = thread_groups, .total = total};
  ctx.state[step.name] = true;
  ctx.results[step.name] = std::any(results);
  return results;
}

std::expected<void, std::string>
Validate(const CheckResourceCountStep& step, dxp::ValidationContext& ctx) {
  if (!ctx.names.insert(step.name).second) {
    return std::unexpected("duplicate SM5 name '" + step.name + "' reused by step");
  }

  if (auto r = ValidateCondition<typename std::decay_t<decltype(step)>::Results>(step.condition, ctx); !r) {
    return std::unexpected(r.error());
  }
  return {};
}

std::string DescribeOutcome(const CheckResourceCountStep&, const dxp::CheckResourceCountResults& results,
                            const ExecutionContext& /*ctx*/) {
  std::vector<std::string> parts;
  const auto add_part = [&parts](int32_t count, std::string_view name) {
    if (count > 0) parts.push_back(std::format("{} {}{}", count, name, count == 1 ? "" : "s"));
  };
  add_part(results.textures, "texture");
  add_part(results.samplers, "sampler");
  add_part(results.cbuffers, "cbuffer");
  add_part(results.uavs, "uav");
  add_part(results.thread_groups, "thread group");
  if (parts.empty()) {
    return "counted 0 resources";
  }
  std::string message = "counted ";
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i > 0) message += (i + 1 == parts.size()) ? " and " : ", ";
    message += parts[i];
  }
  return message;
}

static_assert(RecipeStep<CheckResourceCountStep>);
static_assert(ExecutableStep<CheckResourceCountStep, ExecutionContext>);

}  // namespace dxp::sm5::step
