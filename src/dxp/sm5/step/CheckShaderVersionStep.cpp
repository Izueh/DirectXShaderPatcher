#include <dxp/sm5/step/CheckShaderVersionStep.hpp>
#include <expected>
#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include "dxp/Condition_impl.hpp"
#include "dxp/ResultFieldTraits.hpp"
#include "dxp/sm5/step/CheckShaderVersionStep_impl.hpp"
#include "dxp/StepConcept.hpp"
#include "dxp/StepResults.hpp"
#include "dxp/ValidationContext.hpp"

#include "dxp/sm5/ExecutionContext.hpp"
#include "dxp/sm5/ShaderProgram.hpp"

namespace dxp::sm5::step {

std::expected<dxp::CheckShaderVersionResults, std::string>
Execute(const CheckShaderVersionStep& step, ExecutionContext& ctx) {
  const bool matched = ctx.program.major_version == step.major_version && ctx.program.minor_version == step.minor_version;
  ctx.state[step.name] = matched;
  return dxp::CheckShaderVersionResults{.major_version = step.major_version, .minor_version = step.minor_version};
}

std::expected<void, std::string>
Validate(const CheckShaderVersionStep& step, std::string& error, dxp::ValidationContext& ctx) {
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

std::string DescribeOutcome(const CheckShaderVersionStep&, const dxp::CheckShaderVersionResults& results,
                            const ExecutionContext& ctx) {
  const bool matched = ctx.major_version == results.major_version && ctx.minor_version == results.minor_version;
  if (matched) {
    return std::format("verified SM{}.{}", results.major_version, results.minor_version);
  }
  return std::format("expected SM{}.{}, got SM{}.{} — no match", results.major_version, results.minor_version,
                     ctx.major_version, ctx.minor_version);
}

static_assert(RecipeStep<CheckShaderVersionStep>);
static_assert(ExecutableStep<CheckShaderVersionStep, ExecutionContext>);

}  // namespace dxp::sm5::step
