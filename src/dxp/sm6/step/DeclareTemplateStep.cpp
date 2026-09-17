#include <dxp/sm6/step/DeclareTemplateStep.hpp>
#include <expected>
#include <format>
#include <string>
#include <utility>
#include "dxp/Condition_impl.hpp"
#include "dxp/sm6/step/DeclareTemplateStep_impl.hpp"
#include "dxp/StepResults.hpp"
#include "dxp/ValidationContext.hpp"

#include "dxp/sm6/ExecutionContext.hpp"
#include "dxp/sm6/ShaderProgram.hpp"

namespace dxp::sm6::step {

std::expected<dxp::DeclareTemplateResults, std::string> Execute(const DeclareTemplateStep& step, ExecutionContext& ctx) {
  dxp::DeclareTemplateResults result;
  ctx.templates[step.name] = std::move(step.emits);
  result.emit_count = static_cast<uint32_t>(ctx.templates[step.name].size());
  ctx.state[step.name] = true;
  return result;
}

std::expected<void, std::string> Validate(const DeclareTemplateStep& step, dxp::ValidationContext& ctx) {
  if (step.name.empty()) {
    return std::unexpected("declare_template step requires a name");
  }
  if (!ctx.names.insert(step.name).second) {
    return std::unexpected("duplicate name '" + step.name + "' reused by step");
  }
  for (const auto& tn : step.temps) {
    if (tn.empty()) {
      return std::unexpected("declare_template '" + step.name + "': empty temp name");
    }
    if (!ctx.template_temp_names.insert(tn).second) {
      return std::unexpected("declare_template '" + step.name + "': temp '" + tn + "' collides with another temp");
    }
  }
  if (auto r = ValidateCondition<dxp::DeclareTemplateResults>(step.condition, ctx); !r) {
    return std::unexpected(r.error());
  }
  return {};
}

auto TemplateStepData::Compile() const -> std::expected<DeclareTemplateStep, std::string> {
  auto cond = condition.Compile();
  DeclareTemplateStep step{};
  step.name = name;
  step.condition = cond;
  step.required = required;
  step.temps = temps;
  // TODO: convert TemplateEmitData → step::ApplyRuleStep::EmitPattern for each emit
  return step;
}

std::string DescribeOutcome(const DeclareTemplateStep&, const dxp::DeclareTemplateResults& results,
                            const ExecutionContext& /*ctx*/) {
  return "declared template with " + std::to_string(results.emit_count) + " emit pattern(s)";
}

static_assert(RecipeStep<DeclareTemplateStep>);
static_assert(ExecutableStep<DeclareTemplateStep, ExecutionContext>);

}  // namespace dxp::sm6::step
