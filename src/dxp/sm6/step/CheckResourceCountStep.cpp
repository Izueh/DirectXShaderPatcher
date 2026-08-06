#include "dxp/sm6/step/CheckResourceCountStep.hpp"
#include <any>
#include <cstdint>
#include <expected>
#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include "dxp/Condition_impl.hpp"
#include "dxp/ResultFieldTraits.hpp"
#include "dxp/sm6/step/CheckResourceCountStep_impl.hpp"
#include "dxp/StepConcept.hpp"
#include "dxp/StepResults.hpp"
#include "dxp/ValidationContext.hpp"

#include "dxp/sm6/ExecutionContext.hpp"
#include "dxp/sm6/ShaderProgram.hpp"

namespace dxp::sm6::step {

std::expected<dxp::CheckResourceCountResults, std::string>
Execute(const CheckResourceCountStep& step, ExecutionContext& ctx) {
  auto* dxil = ctx.program.GetDxilModule();

  int32_t textures = 0;
  int32_t samplers = 0;
  int32_t cbuffers = 0;
  int32_t uavs = 0;
  int32_t other = 0;

  if (dxil != nullptr) {
    textures = static_cast<int32_t>(dxil->GetSRVs().size());
    uavs = static_cast<int32_t>(dxil->GetUAVs().size());
    cbuffers = static_cast<int32_t>(dxil->GetCBuffers().size());
    samplers = static_cast<int32_t>(dxil->GetSamplers().size());
    other = 0;
  }

  const int32_t total = textures + samplers + cbuffers + uavs + other;

  dxp::CheckResourceCountResults results{
      .textures = textures,
      .samplers = samplers,
      .cbuffers = cbuffers,
      .uavs = uavs,
      .thread_groups = 0,
      .total = total};
  ctx.state[step.name] = true;
  ctx.results[step.name] = std::any(results);

  return results;
}

std::expected<void, std::string>
Validate(const CheckResourceCountStep& step, std::string& error, ValidationContext& ctx) {
  if (!ctx.names.insert(step.name).second) {
    error = "duplicate SM6 name '" + step.name + "' reused by step";
    return std::unexpected(std::move(error));
  }

  if (auto r = ValidateCondition<CheckResourceCountStep::Results>(step.condition, ctx); !r) {
    error = r.error();
    return std::unexpected(error);
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

}  // namespace dxp::sm6::step
