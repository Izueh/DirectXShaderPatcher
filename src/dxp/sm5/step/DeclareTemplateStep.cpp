#include <dxp/sm5/step/DeclareTemplateStep.hpp>
#include <expected>
#include <format>
#include <string>
#include <utility>
#include "dxp/Condition_impl.hpp"
#include "dxp/sm5/step/common/Rule_impl.hpp"
#include "dxp/sm5/step/DeclareTemplateStep_impl.hpp"
#include "dxp/StepResults.hpp"
#include "dxp/ValidationContext.hpp"

#include "d3d11TokenizedProgramFormat.hpp"
#include "dxp/sm5/ExecutionContext.hpp"
#include "dxp/sm5/Model.hpp"
#include "dxp/sm5/ShaderProgram.hpp"

namespace dxp::sm5::step {
using namespace dxp::sm5::model;

std::expected<dxp::DeclareTemplateResults, std::string> Execute(const DeclareTemplateStep& step, ExecutionContext& ctx) {
  dxp::DeclareTemplateResults result;
  auto& tpl = ctx.templates[step.name];
  // Flatten Template wrappers into the context's template registry
  for (const auto& t : step.emits) {
    for (const auto& ep : t.emits) {
      tpl.emits.push_back(ep);
    }
  }
  // Reuse pool model: the pool block is fixed at the reserved temp base (after
  // the shader's original temps) and reused by every instantiation; its width
  // is the maximum temp count across all templates. Re-setting the base while
  // the pool is still zero-width is idempotent (base always equals
  // reserved_temp_base).
  if (ctx.template_pool_base == 0 && ctx.template_pool_size == 0) {
    ctx.template_pool_base = ctx.reserved_temp_base;
  }
  tpl.temps = step.temps;
  if (static_cast<uint32_t>(step.temps.size()) > ctx.template_pool_size) {
    ctx.template_pool_size = static_cast<uint32_t>(step.temps.size());
  }
  // Colocated required captures — registered at declare time (resolved against
  // the global capture store at expansion; cross-step captures persist).
  tpl.required_captures = CollectRequiredCaptures(tpl.emits);
  // Declared params (handle scope): the instantiating emit provides the
  // registers; bound during expansion (caller-owned — pool stays private
  // scratch).
  tpl.params = step.params;
  result.emit_count = static_cast<uint32_t>(tpl.emits.size());
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
  // Register the template name so later rule emits can reference it (unknown
  // references are rejected at validate time).
  ctx.template_names.insert(step.name);
  for (const auto& tn : step.temps) {
    if (tn.empty()) {
      return std::unexpected("declare_template '" + step.name + "': empty temp name");
    }
    if (tn == "iteration") {
      return std::unexpected("declare_template '" + step.name + "': temp name 'iteration' is reserved (implicit 0-based repeat index variable)");
    }
    if (!ctx.template_temp_names.insert(tn).second) {
      return std::unexpected("declare_template '" + step.name + "': temp '" + tn + "' collides with another temp");
    }
  }
  // Declared params — caller-provided handle scope: bound to caller-provided
  // registers (add_resource temps or match captures) at instantiation.
  std::unordered_set<std::string> own_temps_set(step.temps.begin(), step.temps.end());
  std::unordered_set<std::string> seen_params;
  for (const auto& pn : step.params) {
    if (pn.empty()) {
      return std::unexpected("declare_template '" + step.name + "': empty param name");
    }
    if (pn == "iteration") {
      return std::unexpected("declare_template '" + step.name + "': param name 'iteration' is reserved (implicit 0-based repeat index variable)");
    }
    if (own_temps_set.contains(pn)) {
      return std::unexpected("declare_template '" + step.name + "': param '" + pn + "' collides with a template temp");
    }
    if (!seen_params.insert(pn).second) {
      return std::unexpected("declare_template '" + step.name + "': duplicate param name '" + pn + "'");
    }
    if (ctx.template_param_names.contains(pn)) {
      return std::unexpected("declare_template '" + step.name + "': param '" + pn + "' collides with another template param");
    }
    if (ctx.handles.contains(pn)) {
      return std::unexpected("declare_template '" + step.name + "': param '" + pn + "' collides with a known handle name");
    }
  }
  ctx.template_param_names.insert(step.params.begin(), step.params.end());
  ctx.template_params[step.name] = step.params;
  // Validate repeat config on compiled emits (per-emit repeat, template-set).
  for (const auto& t : step.emits) {
    for (const auto& ep : t.emits) {
      if (ep.repeat.has_value()) {
        if (auto r = ValidateRepeatConfig(*ep.repeat, "declare_template '" + step.name + "'"); !r) {
          return std::unexpected(r.error());
        }
      }
    }
  }
  // Flatten Template wrappers (shared with Execute for the registry shape).
  std::vector<EmitPattern> flat_emits;
  for (const auto& t : step.emits) {
    for (const auto& ep : t.emits) {
      if (ep.blob.empty() && ep.capture.empty() && !ep.opcode.has_value()) {
        return std::unexpected("declare_template '" + step.name + "': emit has no opcode, capture, or blob");
      }
      flat_emits.push_back(ep);
    }
  }
  // Shared emit-pattern validation (operand count + components + role/type
  // checks) — template emits are validated like rule emits.
  if (auto r = ValidateEmitPatterns(flat_emits, "template '" + step.name + "' emits"); !r) {
    return std::unexpected(r.error());
  }
  // Register the template's required captures (name-level; availability is
  // enforced at the invoking apply_rule step's Validate — cross-step global
  // store + named runtime failure cover "known but not produced").
  ctx.template_required_captures[step.name] = CollectRequiredCaptures(flat_emits);
  // Plan rule 8: template emit handle names must be one of the template's own
  // temps or a known global handle (declared so far; add_resource steps are
  // ordered after declare_template, so forward references are rejected).
  const std::unordered_set<std::string> own_temps(step.temps.begin(), step.temps.end());
  const std::unordered_set<std::string> own_params(step.params.begin(), step.params.end());
  for (const auto& t : step.emits) {
    for (const auto& ep : t.emits) {
      for (const auto& op : ep.operands) {
        if (!op.handle) {
          continue;
        }
        const bool is_temp_type = op.type == OperandType::Temp || op.type == OperandType::IndexableTemp;
        if (is_temp_type && !own_temps.contains(op.handle->name) && !own_params.contains(op.handle->name)) {
          return std::unexpected("declare_template '" + step.name + "': temp handle '" + op.handle->name + "' is neither one of the template's temps nor one of its declared params");
        }
        if (!is_temp_type && op.type != OperandType::CBuffer && !own_temps.contains(op.handle->name) && !ctx.handles.contains(op.handle->name)) {
          return std::unexpected("declare_template '" + step.name + "': handle '" + op.handle->name + "' is neither a template temp nor a known global handle");
        }
      }
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
  step.params = params;

  // Each EmitInstructionData becomes a Template wrapper
  Template tmpl;
  for (const auto& emit_data : emit) {
    if (!emit_data.template_name.empty()) {
      return std::unexpected("declare_template '" + name + "': nested template instantiation is not supported (template emits cannot reference templates)");
    }
    EmitPattern ep{};
    ep.opcode = emit_data.opcode;
    ep.capture = emit_data.capture;
    ep.blob = emit_data.blob;
    ep.saturate = emit_data.saturate;
    ep.interpolation_mode = emit_data.interpolation;
    ep.test_boolean = emit_data.test_boolean;
    for (const auto& ext : emit_data.extended_opcodes) {
      EmitExtendedOpcode e{};
      e.kind = EmitExtendedOpcode::Kind::Type;
      e.type = ext.type.value_or(ExtendedOpcodeType::Empty);
      e.raw = ext.raw.value_or(0);
      e.sample_controls = ext.sample_controls;
      e.resource_dim = ext.resource_dim;
      if (ext.resource_return_type.has_value()) {
        ResourceReturnTypePayload rtp{};
        rtp.component_types = *ext.resource_return_type;
        e.resource_return_type = std::move(rtp);
      }
      ep.extended_opcodes.push_back(std::move(e));
    }
    // Per-emit repeat (template-set): repeat + params live on the template's
    // emits; bound per iteration during expansion. Structural validation
    // (times >= 1, param families, reserved names) is enforced by
    // ValidateRepeatConfig during Validate.
    if (emit_data.repeat.has_value()) {
      ep.repeat = CompileRepeatConfig(emit_data.repeat);
    }
    for (const auto& op_data : emit_data.operands) {
      auto op = CompileOperandPattern(op_data, true);
      if (!op) return std::unexpected(op.error());
      ep.operands.push_back(std::move(op.value()));
    }
    tmpl.emits.push_back(std::move(ep));
  }
  step.emits.push_back(std::move(tmpl));

  return step;
}

std::string DescribeOutcome(const DeclareTemplateStep&, const dxp::DeclareTemplateResults& results,
                            const ExecutionContext& /*ctx*/) {
  return "declared template with " + std::to_string(results.emit_count) + " emit pattern(s)";
}

static_assert(RecipeStep<DeclareTemplateStep>);
static_assert(ExecutableStep<DeclareTemplateStep, ExecutionContext>);

}  // namespace dxp::sm5::step
