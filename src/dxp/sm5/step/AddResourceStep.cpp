#include <any>
#include <cstddef>
#include <cstdint>
#include <dxp/sm5/step/AddResourceStep.hpp>
#include <expected>
#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include "dxp/Condition_impl.hpp"
#include "dxp/ExportTypes.hpp"
#include "dxp/ResultFieldTraits.hpp"
#include "dxp/sm5/Model.hpp"
#include "dxp/sm5/step/AddResourceStep_impl.hpp"
#include "dxp/StepConcept.hpp"
#include "dxp/StepResults.hpp"
#include "dxp/ValidationContext.hpp"

#include "d3d11TokenizedProgramFormat.hpp"
#include "dxp/sm5/ExecutionContext.hpp"
#include "dxp/sm5/ShaderProgram.hpp"

namespace dxp::sm5::step {
using namespace dxp::sm5::model;

constexpr uint32_t kMaxTextureBindPoint = 127;
constexpr uint32_t kMaxCBufferBindPoint = 14;
constexpr uint32_t kMaxSamplerBindPoint = 15;
constexpr uint32_t kMaxUavBindPoint = 63;
constexpr uint32_t kMaxInputBindPoint = 31;
constexpr uint32_t kMaxOutputBindPoint = 7;

dxp::ResourceTypeInfo TypeInfoOf(const AddResourceStep::TextureDecl& decl) {
  return {.dimension = decl.dimension};
}
dxp::ResourceTypeInfo TypeInfoOf(const AddResourceStep::RawResourceDecl&) {
  return {.dimension = dxp::ResourceDimension::RawBuffer};
}
dxp::ResourceTypeInfo TypeInfoOf(const AddResourceStep::StructuredResourceDecl& decl) {
  return {.dimension = dxp::ResourceDimension::StructuredBuffer, .structure_stride = decl.structure_stride};
}
dxp::ResourceTypeInfo TypeInfoOf(const AddResourceStep::CBufferDecl& decl) {
  return {.cbuffer = decl.layout};
}
dxp::ResourceTypeInfo TypeInfoOf(const AddResourceStep::SamplerDecl&) {
  return {};
}
dxp::ResourceTypeInfo TypeInfoOf(const AddResourceStep::UavDecl& decl) {
  switch (decl.kind) {
    case AddResourceStep::UavKind::Raw:        return {.dimension = dxp::ResourceDimension::RawBuffer};
    case AddResourceStep::UavKind::Structured: return {.dimension = dxp::ResourceDimension::StructuredBuffer, .structure_stride = decl.structure_stride};
    default:                                   return {.dimension = decl.dimension};
  }
}

std::vector<dxp::ResourceDeclaration> Declarations(const AddResourceStep& step) {
  std::vector<dxp::ResourceDeclaration> out;
  const bool conditional = step.condition.has_value() || !step.required;
  const auto add = [&](dxp::BindingClass binding_class, const auto& decls) {
    for (const auto& decl : decls) {
      out.push_back({.step = step.name, .handle = decl.handle, .binding_class = binding_class, .register_index = decl.register_index,
                     .space = 0u, .type = TypeInfoOf(decl), .conditional = conditional, .meta = decl.meta});
    }
  };
  add(dxp::BindingClass::Texture, step.textures);
  add(dxp::BindingClass::RawResource, step.raw_resources);
  add(dxp::BindingClass::StructuredResource, step.structured_resources);
  add(dxp::BindingClass::CBuffer, step.cbuffers);
  add(dxp::BindingClass::Sampler, step.samplers);
  add(dxp::BindingClass::Uav, step.uavs);
  return out;
}

std::expected<dxp::AddResourceResults, std::string> Execute(const AddResourceStep& step, ExecutionContext& ctx) {
  dxp::AddResourceResults result;
  bool changed = false;

  const auto fail_or_warn = [&](const std::string& message) -> bool {
    if (step.required) return false;
    ctx.logger.Log(LogLevel::Warning, message + " — skipped (required: false)");
    return true;
  };

  auto add_new_binding = [&](dxp::BindingClass kind, const std::string& handle, uint32_t bind_point,
                             dxp::ResourceTypeInfo type = {}, const std::optional<dxp::MetaValue>& meta = std::nullopt) {
    dxp::ResourceBinding binding;
    binding.binding_class = kind;
    binding.handle = handle;
    binding.register_index = bind_point;
    binding.space = 0;
    binding.type = std::move(type);
    binding.meta = meta;
    ctx.resource_bindings[handle] = std::move(binding);
  };

  auto resolve_and_add = [&](const auto& decls, auto add_decl, auto find_next, auto& bindings,
                             dxp::BindingClass kind, uint32_t max_bind_point, const char* kind_name,
                             uint32_t* count_ptr) -> std::expected<void, std::string> {
    for (const auto& decl : decls) {
      uint32_t bind_point = 0;
      if (decl.register_index.has_value()) {
        bind_point = *decl.register_index;
        if (bind_point > max_bind_point) {
          const std::string message = std::string("add_resource: register_index ") + std::to_string(bind_point) + std::string(" exceeds maximum ") + std::to_string(max_bind_point) + std::string(" for '") + kind_name + "'";
          if (!fail_or_warn(message)) return std::unexpected(message);
          continue;
        }
      } else {
        // Auto-bind: default takes the lowest free slot, but game shaders
        // commonly occupy the low registers, so that can collide with the game's
        // own bindings. reverse_bind takes the HIGHEST free slot (scan down
        // from the max) instead.
        const bool reverse = decl.reverse_bind.value_or(false);
        bind_point = reverse ? find_next(ctx.program, max_bind_point, true) : find_next(ctx.program, 0U, false);
        if (bind_point > max_bind_point) {
          const std::string message = std::string("add_resource: auto-bind exhausted for '") + kind_name + std::string("' — maximum bind point ") + std::to_string(max_bind_point);
          if (!fail_or_warn(message)) return std::unexpected(message);
          continue;
        }
      }
      std::string decl_error;
      if (!add_decl(decl, bind_point, max_bind_point, decl_error)) {
        const std::string message = std::string("add_resource: ") + kind_name + std::string(" '") + decl.handle + "': " + decl_error;
        if (!fail_or_warn(message)) return std::unexpected(message);
        continue;
      }
      const auto handle = decl.handle.empty() ? "" : decl.handle;
      bindings[handle] = bind_point;
      add_new_binding(kind, handle, bind_point, TypeInfoOf(decl), decl.meta);
      ++(*count_ptr);
      changed = true;
    }
    return {};
  };

  if (auto r = resolve_and_add(step.textures, [&](const auto& d, uint32_t bp, uint32_t max_bp, std::string& e) { return ctx.program.AddTextureDeclaration(d, bp, max_bp, e); }, [&](const auto& p, unsigned preferred, bool from_high) { return p.FindNextAvailableTexture(preferred, from_high); }, ctx.Bindings(BindingClass::Texture), dxp::BindingClass::Texture, kMaxTextureBindPoint, "texture", &result.textures_added); !r) {
    return std::unexpected(std::move(r.error()));
  }
  if (auto r = resolve_and_add(step.raw_resources, [&](const auto& d, uint32_t bp, uint32_t max_bp, std::string& e) { return ctx.program.AddRawResourceDeclaration(d, bp, max_bp, e); }, [&](const auto& p, unsigned preferred, bool from_high) { return p.FindNextAvailableTexture(preferred, from_high); }, ctx.Bindings(BindingClass::RawResource), dxp::BindingClass::RawResource, kMaxTextureBindPoint, "raw_resource", &result.raw_resources_added); !r) {
    return std::unexpected(std::move(r.error()));
  }
  if (auto r = resolve_and_add(step.structured_resources, [&](const auto& d, uint32_t bp, uint32_t max_bp, std::string& e) { return ctx.program.AddStructuredResourceDeclaration(d, bp, max_bp, e); }, [&](const auto& p, unsigned preferred, bool from_high) { return p.FindNextAvailableTexture(preferred, from_high); }, ctx.Bindings(BindingClass::StructuredResource), dxp::BindingClass::StructuredResource, kMaxTextureBindPoint, "structured_resource", &result.structured_resources_added); !r) {
    return std::unexpected(std::move(r.error()));
  }
  if (auto r = resolve_and_add(step.cbuffers, [&](const auto& d, uint32_t bp, uint32_t max_bp, std::string& e) { return ctx.program.AddCBufferDeclaration(d, bp, max_bp, e); }, [&](const auto& p, unsigned preferred, bool from_high) { return p.FindNextAvailableCBuffer(preferred, from_high); }, ctx.Bindings(BindingClass::CBuffer), dxp::BindingClass::CBuffer, kMaxCBufferBindPoint, "cbuffer", &result.cbuffers_added); !r) {
    return std::unexpected(std::move(r.error()));
  }
  if (auto r = resolve_and_add(step.samplers, [&](const auto& d, uint32_t bp, uint32_t max_bp, std::string& e) { return ctx.program.AddSamplerDeclaration(d, bp, max_bp, e); }, [&](const auto& p, unsigned preferred, bool from_high) { return p.FindNextAvailableSampler(preferred, from_high); }, ctx.Bindings(BindingClass::Sampler), dxp::BindingClass::Sampler, kMaxSamplerBindPoint, "sampler", &result.samplers_added); !r) {
    return std::unexpected(std::move(r.error()));
  }
  if (auto r = resolve_and_add(step.uavs, [&](const auto& d, uint32_t bp, uint32_t max_bp, std::string& e) { return ctx.program.AddUavDeclaration(d, bp, max_bp, e); }, [&](const auto& p, unsigned preferred, bool from_high) { return p.FindNextAvailableUAV(preferred, from_high); }, ctx.Bindings(BindingClass::Uav), dxp::BindingClass::Uav, kMaxUavBindPoint, "uav", &result.uavs_added); !r) {
    return std::unexpected(std::move(r.error()));
  }

  {
    for (const auto& decl : step.inputs) {
      uint32_t bind_point = 0;
      if (decl.register_index.has_value()) {
        bind_point = *decl.register_index;
        if (bind_point > kMaxInputBindPoint) {
          const std::string message = "add_resource: register_index " + std::to_string(bind_point) + " exceeds maximum " + std::to_string(kMaxInputBindPoint) + " for 'input'";
          if (!fail_or_warn(message)) return std::unexpected(message);
          continue;
        }
      } else {
        const bool reverse = decl.reverse_bind.value_or(false);
        bind_point = reverse ? ctx.program.FindNextAvailableInput(kMaxInputBindPoint, true) : ctx.program.FindNextAvailableInput(0U, false);
        if (bind_point > kMaxInputBindPoint) {
          const std::string message = "add_resource: auto-bind exhausted for 'input'";
          if (!fail_or_warn(message)) return std::unexpected(message);
          continue;
        }
      }
      std::string decl_error;
      if (!ctx.program.AddInputDeclaration(decl, bind_point, kMaxInputBindPoint, decl_error)) {
        const std::string message = "add_resource: input '" + decl.handle + "': " + decl_error;
        if (!fail_or_warn(message)) return std::unexpected(message);
        continue;
      }
      const auto handle = decl.handle.empty() ? "" : decl.handle;
      ctx.Bindings(BindingClass::Input)[handle] = bind_point;
      add_new_binding(dxp::BindingClass::Input, handle, bind_point);
      ++result.inputs_added;
    }
  }

  {
    for (const auto& decl : step.outputs) {
      uint32_t bind_point = 0;
      if (decl.register_index.has_value()) {
        bind_point = *decl.register_index;
        if (bind_point > kMaxOutputBindPoint) {
          const std::string message = "add_resource: register_index " + std::to_string(bind_point) + " exceeds maximum " + std::to_string(kMaxOutputBindPoint) + " for 'output'";
          if (!fail_or_warn(message)) return std::unexpected(message);
          continue;
        }
      } else {
        const bool reverse = decl.reverse_bind.value_or(false);
        bind_point = reverse ? ctx.program.FindNextAvailableOutput(kMaxOutputBindPoint, true) : ctx.program.FindNextAvailableOutput(0U, false);
        if (bind_point > kMaxOutputBindPoint) {
          const std::string message = "add_resource: auto-bind exhausted for 'output'";
          if (!fail_or_warn(message)) return std::unexpected(message);
          continue;
        }
      }
      std::string decl_error;
      if (!ctx.program.AddOutputDeclaration(decl, bind_point, kMaxOutputBindPoint, decl_error)) {
        const std::string message = "add_resource: output '" + decl.handle + "': " + decl_error;
        if (!fail_or_warn(message)) return std::unexpected(message);
        continue;
      }
      const auto handle = decl.handle.empty() ? "" : decl.handle;
      ctx.Bindings(BindingClass::Output)[handle] = bind_point;
      add_new_binding(dxp::BindingClass::Output, handle, bind_point);
      ++result.outputs_added;
    }
  }

  if (!step.temps.empty()) {
    // Reuse pool model: template temps occupy a fixed block between the
    // shader's original temps and the add_resource temps.
    const uint32_t temp_base = ctx.program.temp_count + ctx.template_pool_size;
    uint32_t offset = 0;
    for (const auto& h : step.temps) {
      std::visit(
          [&](const auto& val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, std::string>) {
              ctx.Bindings(BindingClass::Temp)[val] = temp_base + offset;
              ++offset;
            } else {
              ctx.Bindings(BindingClass::Temp)[val.name] = temp_base + offset;
              offset += val.count;
            }
          },
          h);
    }
    ctx.program.temp_count += offset;
    result.temps_added = offset;
    changed = true;
    // Temp budget (plan rule 7): original + template pool + added temps must
    // fit the SM5 temp register file (r0..r255). The pool is final here (all
    // declare_template steps precede add_resource steps).
    const uint32_t kMaxTempRegisters = 256;
    const uint32_t total_temps = ctx.program.temp_count + ctx.template_pool_size;
    if (total_temps > kMaxTempRegisters) {
      const std::string message = "add_resource temps exceed the SM5 temp budget: "
                                  + std::to_string(ctx.reserved_temp_base) + " original"
                                  + " + " + std::to_string(ctx.template_pool_size) + " template pool"
                                  + " + " + std::to_string(ctx.program.temp_count - ctx.reserved_temp_base) + " added"
                                  + " = " + std::to_string(total_temps) + " > " + std::to_string(kMaxTempRegisters);
      if (!fail_or_warn(message)) return std::unexpected(message);
    }
  }

  if (ctx.program.temp_count > 0 || ctx.template_pool_size > 0) {
    ctx.program.EnsureTempDeclaration(ctx.template_pool_size);
  }

  if (changed) ctx.MarkProgramMutated();
  ctx.state[step.name] = true;
  ctx.results[step.name] = std::any(result);
  return result;
}

std::expected<void, std::string> Validate(const AddResourceStep& step, dxp::ValidationContext& ctx) {
  if (step.name.empty()) {
    return std::unexpected("add_resource step requires a name");
  }

  for (const auto& d : step.uavs) {
    if (d.register_index.has_value()) {
      if (d.kind == AddResourceStep::UavKind::Raw) {
        return std::unexpected("add_resource: raw UAV does not support stride");
      }
    }
  }

  for (const auto& t : step.temps) {
    std::string error;
    std::visit(
        [&](const auto& val) {
          using T = std::decay_t<decltype(val)>;
          if constexpr (std::is_same_v<T, std::string>) {
            if (val.empty()) {
              error = "add_resource: temp handle must not be empty";
            } else if (ctx.template_param_names.contains(val)) {
              error = "add_resource: temp handle '" + val + "' collides with a declared template param";
            } else {
              ctx.handles.insert(val);
            }
          } else {
            if (val.name.empty()) {
              error = "add_resource: temp array name must not be empty";
            } else if (val.count == 0) {
              error = "add_resource: temp array count must be >= 1";
            } else if (ctx.template_param_names.contains(val.name)) {
              error = "add_resource: temp array name '" + val.name + "' collides with a declared template param";
            } else {
              ctx.handles.insert(val.name);
            }
          }
        },
        t);
    if (!error.empty()) return std::unexpected(std::move(error));
  }

  if (!ctx.names.insert(step.name).second) {
    return std::unexpected("duplicate SM5 name '" + step.name + "' reused by step");
  }

  auto declareHandles = [&ctx](const auto& decls) {
    for (const auto& d : decls) {
      if (!d.handle.empty()) ctx.handles.insert(d.handle);
    }
  };

  declareHandles(step.textures);
  declareHandles(step.raw_resources);
  declareHandles(step.structured_resources);
  declareHandles(step.cbuffers);
  declareHandles(step.samplers);
  declareHandles(step.uavs);
  declareHandles(step.inputs);
  declareHandles(step.outputs);

  if (auto r = ValidateCondition<typename std::decay_t<decltype(step)>::Results>(step.condition, ctx); !r) {
    return std::unexpected(r.error());
  }
  return {};
}

auto AddResourceData::Compile() const -> std::expected<AddResourceStep, std::string> {
  auto cond = condition.Compile();
  AddResourceStep step{};
  step.name = name;
  step.condition = cond;
  step.required = required;

  step.textures.reserve(textures.size());
  for (const auto& d : textures) {
    AddResourceStep::TextureDecl decl{};
    decl.handle = d.handle;
    decl.register_index = d.register_index;
    decl.reverse_bind = d.reverse_bind;
    if (d.dimension.has_value()) decl.dimension = *d.dimension;
    decl.meta = dxp::detail::ToOptionalMetaValue(d.meta);
    step.textures.push_back(std::move(decl));
  }
  step.raw_resources.reserve(raw_resources.size());
  for (const auto& d : raw_resources) {
    AddResourceStep::RawResourceDecl decl{};
    decl.handle = d.handle;
    decl.register_index = d.register_index;
    decl.reverse_bind = d.reverse_bind;
    decl.meta = dxp::detail::ToOptionalMetaValue(d.meta);
    step.raw_resources.push_back(std::move(decl));
  }
  step.structured_resources.reserve(structured_resources.size());
  for (const auto& d : structured_resources) {
    if (d.stride == 0) {
      return std::unexpected("structured resource '" + d.handle + "' stride must be non-zero");
    }
    AddResourceStep::StructuredResourceDecl decl{};
    decl.handle = d.handle;
    decl.register_index = d.register_index;
    decl.reverse_bind = d.reverse_bind;
    decl.structure_stride = d.stride;
    decl.meta = dxp::detail::ToOptionalMetaValue(d.meta);
    step.structured_resources.push_back(std::move(decl));
  }
  step.cbuffers.reserve(cbuffers.size());
  for (const auto& d : cbuffers) {
    AddResourceStep::CBufferDecl decl{};
    decl.handle = d.handle;
    decl.register_index = d.register_index;
    decl.reverse_bind = d.reverse_bind;
    if (d.elements.has_value()) decl.elements = *d.elements;
    decl.meta = dxp::detail::ToOptionalMetaValue(d.meta);
    if (!d.fields.empty()) {
      // Fields are recipe-side layout only: DXBC addresses cb#[row].component, so the
      // layout sizes the declaration and resolves `{ name, field }` emit operands.
      auto layout = dxp::detail::BuildCBufferLayout(d.handle, {}, d.elements.value_or(0) * 16, d.fields);
      if (!layout) return std::unexpected("add_resource '" + name + "': " + layout.error());
      decl.elements = layout->size_in_bytes / 16;
      decl.layout = std::move(*layout);
    }
    step.cbuffers.push_back(std::move(decl));
  }
  step.samplers.reserve(samplers.size());
  for (const auto& d : samplers) {
    AddResourceStep::SamplerDecl decl{};
    decl.handle = d.handle;
    decl.register_index = d.register_index;
    decl.reverse_bind = d.reverse_bind;
    if (d.mode.has_value()) decl.mode = *d.mode;
    decl.meta = dxp::detail::ToOptionalMetaValue(d.meta);
    step.samplers.push_back(std::move(decl));
  }
  step.uavs.reserve(uavs.size());
  for (const auto& d : uavs) {
    AddResourceStep::UavDecl decl{};
    decl.handle = d.handle;
    decl.register_index = d.register_index;
    decl.reverse_bind = d.reverse_bind;
    if (d.kind.has_value()) decl.kind = *d.kind;
    if (d.dimension.has_value()) decl.dimension = *d.dimension;
    decl.structure_stride = d.stride;
    if (d.globally_coherent.has_value()) decl.globally_coherent = *d.globally_coherent;
    if (d.has_counter.has_value()) decl.has_order_preserving_counter = *d.has_counter;
    decl.meta = dxp::detail::ToOptionalMetaValue(d.meta);
    step.uavs.push_back(std::move(decl));
  }
  step.inputs.reserve(inputs.size());
  for (const auto& d : inputs) {
    AddResourceStep::InputDecl decl{};
    decl.handle = d.handle;
    decl.register_index = d.register_index;
    decl.reverse_bind = d.reverse_bind;
    if (d.interpolation.has_value()) {
      decl.interpolation_mode = *d.interpolation;
    }
    step.inputs.push_back(std::move(decl));
  }
  step.outputs.reserve(outputs.size());
  for (const auto& d : outputs) {
    AddResourceStep::OutputDecl decl{};
    decl.handle = d.handle;
    decl.register_index = d.register_index;
    decl.reverse_bind = d.reverse_bind;
    step.outputs.push_back(std::move(decl));
  }
  step.temps.reserve(temps.size());
  for (const auto& t : temps) {
    std::visit(
        [&](const auto& val) {
          AddResourceStep::TempHandle h;
          if constexpr (std::is_same_v<std::decay_t<decltype(val)>, std::string>) {
            h = val;
          } else {
            h = AddResourceStep::TempArrayDecl{val.name, val.count};
          }
          step.temps.push_back(std::move(h));
        },
        t);
  }
  return step;
}

std::string DescribeOutcome(const AddResourceStep&, const dxp::AddResourceResults& results,
                            const ExecutionContext& /*ctx*/) {
  std::vector<std::string> parts;
  const auto add_part = [&parts](uint32_t count, std::string_view name) {
    if (count > 0) parts.push_back(std::format("{} {}{}", count, name, count == 1 ? "" : "s"));
  };
  add_part(results.textures_added, "texture");
  add_part(results.raw_resources_added, "raw resource");
  add_part(results.structured_resources_added, "structured resource");
  add_part(results.cbuffers_added, "cbuffer");
  add_part(results.samplers_added, "sampler");
  add_part(results.uavs_added, "uav");
  add_part(results.inputs_added, "input");
  add_part(results.outputs_added, "output");
  add_part(results.temps_added, "temp");
  if (parts.empty()) {
    return "added nothing";
  }
  std::string message = "added ";
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i > 0) message += (i + 1 == parts.size()) ? " and " : ", ";
    message += parts[i];
  }
  return message;
}

static_assert(RecipeStep<AddResourceStep>);
static_assert(ExecutableStep<AddResourceStep, ExecutionContext>);

std::expected<void, std::string> ResolveCBufferReferences(AddResourceStep& step, dxp::detail::CBufferLayouts& layouts) {
  for (const auto& cbuffer : step.cbuffers) {
    if (cbuffer.layout) {
      layouts[cbuffer.handle] = *cbuffer.layout;
    } else {
      layouts.erase(cbuffer.handle);  // a redeclaration without fields hides the earlier layout
    }
  }
  return {};
}

}  // namespace dxp::sm5::step
