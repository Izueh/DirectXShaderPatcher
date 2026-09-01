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

std::expected<dxp::AddResourceResults, std::string> Execute(const AddResourceStep& step, ExecutionContext& ctx) {
  dxp::AddResourceResults result;
  bool changed = false;

  const auto fail_or_warn = [&](const std::string& message) -> bool {
    if (step.required) return false;
    ctx.logger.Log(LogLevel::Warning, message + " — skipped (required: false)");
    return true;
  };

  auto add_new_binding = [&](dxp::BindingClass kind, const std::string& handle, uint32_t bind_point) {
    dxp::ResourceBinding binding;
    binding.binding_class = kind;
    binding.handle = handle;
    binding.register_index = bind_point;
    binding.space = 0;
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
      add_new_binding(kind, handle, bind_point);
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
    const uint32_t temp_base = ctx.program.temp_count;
    for (size_t i = 0; i < step.temps.size(); ++i) {
      const uint32_t bind_point = temp_base + static_cast<uint32_t>(i);
      ctx.Bindings(BindingClass::Temp)[step.temps[i]] = bind_point;
    }
    ctx.program.temp_count += static_cast<uint32_t>(step.temps.size());
    result.temps_added = static_cast<uint32_t>(step.temps.size());
    changed = true;
  }

  if (ctx.program.temp_count > 0) {
    ctx.program.EnsureTempDeclaration();
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
    if (t.empty()) {
      return std::unexpected("add_resource: temp handle must not be empty");
    }
    ctx.handles.insert(t);
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
    step.textures.push_back(std::move(decl));
  }
  step.raw_resources.reserve(raw_resources.size());
  for (const auto& d : raw_resources) {
    AddResourceStep::RawResourceDecl decl{};
    decl.handle = d.handle;
    decl.register_index = d.register_index;
    decl.reverse_bind = d.reverse_bind;
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
    step.structured_resources.push_back(std::move(decl));
  }
  step.cbuffers.reserve(cbuffers.size());
  for (const auto& d : cbuffers) {
    AddResourceStep::CBufferDecl decl{};
    decl.handle = d.handle;
    decl.register_index = d.register_index;
    decl.reverse_bind = d.reverse_bind;
    if (d.elements.has_value()) decl.elements = *d.elements;
    step.cbuffers.push_back(std::move(decl));
  }
  step.samplers.reserve(samplers.size());
  for (const auto& d : samplers) {
    AddResourceStep::SamplerDecl decl{};
    decl.handle = d.handle;
    decl.register_index = d.register_index;
    decl.reverse_bind = d.reverse_bind;
    if (d.mode.has_value()) decl.mode = *d.mode;
    step.samplers.push_back(std::move(decl));
  }
  step.uavs.reserve(uavs.size());
  for (const auto& d : uavs) {
    AddResourceStep::UavDecl decl{};
    decl.handle = d.handle;
    decl.register_index = d.register_index;
    decl.reverse_bind = d.reverse_bind;
    if (d.kind.has_value()) decl.kind = *d.kind;
    if (d.globally_coherent.has_value()) decl.globally_coherent = *d.globally_coherent;
    if (d.has_counter.has_value()) decl.has_order_preserving_counter = *d.has_counter;
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
  step.temps = temps;
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

}  // namespace dxp::sm5::step
