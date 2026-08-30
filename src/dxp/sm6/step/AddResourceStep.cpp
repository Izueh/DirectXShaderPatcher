#include "dxp/sm6/step/AddResourceStep.hpp"
#include <dxc/DXIL/DxilCompType.h>
#include <dxc/DXIL/DxilInterpolationMode.h>
#include <llvm-c/Target.h>
#include <llvm/IR/Value.h>
#include <format>
#include "dxp/Condition_impl.hpp"
#include "dxp/ExportTypes.hpp"
#include "dxp/ResultFieldTraits.hpp"
#include "dxp/sm6/ResourceTypes.hpp"
#include "dxp/sm6/step/AddResourceStep_impl.hpp"
#include "dxp/StepConcept.hpp"
#include "dxp/StepResults.hpp"
#include "dxp/ValidationContext.hpp"

#include <algorithm>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "dxc/DXIL/DxilConstants.h"
#include "dxp/sm6/ExecutionContext.hpp"
#include "dxp/sm6/ShaderProgram.hpp"

namespace dxp::sm6::step {

namespace {

/// @brief Resolve binding: apply space and register_index from resolved values.
inline void ResolveBinding(ResourceBindingDesc& desc, unsigned space, std::optional<unsigned> register_index) {
  desc.space = space;
  desc.register_index = register_index;
}

}  // namespace

std::expected<dxp::AddResourceResults, std::string> Execute(const AddResourceStep& step, ExecutionContext& ctx) {
  dxp::AddResourceResults result;
  bool changed = false;

  const auto fail_or_warn = [&](const std::string& message) -> bool {
    if (step.required) return false;
    ctx.logger.Log(LogLevel::Warning, message + " — skipped (required: false)");
    return true;
  };

  auto addSideEffect = [&](dxp::ResourceKind kind, const std::string& handle,
                           unsigned bind_point, unsigned space) {
    dxp::ResourceBinding binding;
    binding.resource_kind = kind;
    binding.handle = handle;
    binding.register_index = bind_point;
    binding.space = space;
    ctx.resource_bindings[handle] = std::move(binding);
  };

  if (!step.textures.empty()) {
    unsigned srv_space = 0;
    bool has_explicit_space = false;
    for (const auto& t : step.textures) {
      if (t.binding.space.has_value()) {
        srv_space = *t.binding.space;
        has_explicit_space = true;
        break;
      }
    }
    if (!has_explicit_space) {
      srv_space = ctx.program.FindNextAvailableTexture(0, 0);
    }
    for (const auto& desc : step.textures) {
      TextureResourceDesc resolved_desc = desc;
      ResolveBinding(resolved_desc.binding, srv_space, resolved_desc.binding.register_index);
      if (!resolved_desc.binding.register_index.has_value()) {
        resolved_desc.binding.register_index = ctx.program.FindNextAvailableTexture(srv_space, 0);
      }
      if (auto add_result = ctx.program.AddTextureSRV(resolved_desc); !add_result) {
        const std::string message = "add_resource: resource '" + desc.name + "': " + add_result.error();
        if (!fail_or_warn(message)) return std::unexpected(message);
        continue;
      }
      const auto& handle = desc.name.empty() ? "" : desc.name;
      ctx.textures[handle] = resolved_desc;
      addSideEffect(resolved_desc.kind == DxilResourceKind::RawBuffer          ? dxp::ResourceKind::RawResource
                    : resolved_desc.kind == DxilResourceKind::StructuredBuffer ? dxp::ResourceKind::StructuredResource
                                                                               : dxp::ResourceKind::Texture,
                    handle, *resolved_desc.binding.register_index, srv_space);
      if (resolved_desc.kind == DxilResourceKind::RawBuffer) {
        ++result.raw_resources_added;
      } else if (resolved_desc.kind == DxilResourceKind::StructuredBuffer) {
        ++result.structured_resources_added;
      } else {
        ++result.textures_added;
      }
      changed = true;

      if (!handle.empty()) {
        const auto* srv = FindResourceByRegisterIndex(*ctx.program.GetDxilModule(),
                                                      static_cast<hlsl::DXIL::ResourceClass>(ResourceClass::SRV),
                                                      *resolved_desc.binding.register_index, srv_space);
        if (srv != nullptr) {
          llvm::Value* handle_value = ctx.program.CreateResourceHandle(*srv, dxp::sm6::ShaderProgram::ToDxilBinding(resolved_desc.binding));
          if (handle_value != nullptr) {
            ctx.resource_handle_values[handle] = handle_value;
          }
          ctx.resource_handles[handle] = srv;
        }
      }
    }
  }

  if (!step.uavs.empty()) {
    unsigned uav_space = 0;
    bool has_explicit_space = false;
    for (const auto& u : step.uavs) {
      if (u.binding.space.has_value()) {
        uav_space = *u.binding.space;
        has_explicit_space = true;
        break;
      }
    }
    if (!has_explicit_space) {
      uav_space = ctx.program.FindNextAvailableUAV(0, 0);
    }
    for (const auto& desc : step.uavs) {
      TextureResourceDesc resolved_desc = desc;
      ResolveBinding(resolved_desc.binding, uav_space, resolved_desc.binding.register_index);
      if (!resolved_desc.binding.register_index.has_value()) {
        resolved_desc.binding.register_index = ctx.program.FindNextAvailableUAV(uav_space, 0);
      }
      resolved_desc.is_read_write = true;
      if (auto add_result = ctx.program.AddTextureUAV(resolved_desc); !add_result) {
        const std::string message = "add_resource: uav '" + desc.name + "': " + add_result.error();
        if (!fail_or_warn(message)) return std::unexpected(message);
        continue;
      }
      const auto& handle = desc.name.empty() ? "" : desc.name;
      ctx.uavs[handle] = resolved_desc;
      addSideEffect(resolved_desc.kind == DxilResourceKind::RawBuffer          ? dxp::ResourceKind::RawResource
                    : resolved_desc.kind == DxilResourceKind::StructuredBuffer ? dxp::ResourceKind::StructuredResource
                                                                               : dxp::ResourceKind::TextureUav,
                    handle, *resolved_desc.binding.register_index, uav_space);
      if (resolved_desc.kind == DxilResourceKind::RawBuffer) {
        ++result.raw_resources_added;
      } else if (resolved_desc.kind == DxilResourceKind::StructuredBuffer) {
        ++result.structured_resources_added;
      } else {
        ++result.uavs_added;
      }
      changed = true;

      if (!handle.empty()) {
        const auto* uav = FindResourceByRegisterIndex(*ctx.program.GetDxilModule(),
                                                      static_cast<hlsl::DXIL::ResourceClass>(ResourceClass::UAV),
                                                      *resolved_desc.binding.register_index, uav_space);
        if (uav != nullptr) {
          llvm::Value* handle_value = ctx.program.CreateResourceHandle(*uav, dxp::sm6::ShaderProgram::ToDxilBinding(resolved_desc.binding));
          if (handle_value != nullptr) {
            ctx.resource_handle_values[handle] = handle_value;
          }
          ctx.resource_handles[handle] = uav;
        }
      }
    }
  }

  if (!step.cbuffers.empty()) {
    unsigned cbuf_space = 0;
    bool has_explicit_space = false;
    for (const auto& c : step.cbuffers) {
      if (c.binding.space.has_value()) {
        cbuf_space = *c.binding.space;
        has_explicit_space = true;
        break;
      }
    }
    if (!has_explicit_space) {
      cbuf_space = ctx.program.FindNextAvailableCBuffer(0, 0);
    }
    for (const auto& desc : step.cbuffers) {
      CBufferDesc resolved_desc = desc;
      ResolveBinding(resolved_desc.binding, cbuf_space, resolved_desc.binding.register_index);
      if (!resolved_desc.binding.register_index.has_value()) {
        resolved_desc.binding.register_index = ctx.program.FindNextAvailableCBuffer(cbuf_space, 0);
      }
      if (auto add_result = ctx.program.AddCBuffer(resolved_desc); !add_result) {
        const std::string message = "add_resource: cbuffer '" + desc.name + "': " + add_result.error();
        if (!fail_or_warn(message)) return std::unexpected(message);
        continue;
      }
      const auto& handle = desc.name.empty() ? "" : desc.name;
      ctx.cbuffers[handle] = resolved_desc;
      addSideEffect(dxp::ResourceKind::CBuffer, handle, *resolved_desc.binding.register_index, cbuf_space);
      ++result.cbuffers_added;
      changed = true;

      if (!handle.empty()) {
        const auto* cbuf = FindResourceByRegisterIndex(*ctx.program.GetDxilModule(),
                                                       static_cast<hlsl::DXIL::ResourceClass>(ResourceClass::CBuffer),
                                                       *resolved_desc.binding.register_index, cbuf_space);
        if (cbuf != nullptr) {
          llvm::Value* handle_value = ctx.program.CreateResourceHandle(*cbuf, dxp::sm6::ShaderProgram::ToDxilBinding(resolved_desc.binding));
          if (handle_value != nullptr) {
            ctx.resource_handle_values[handle] = handle_value;
          }
          ctx.resource_handles[handle] = cbuf;
        }
      }
    }
  }

  if (!step.samplers.empty()) {
    unsigned sampler_space = 0;
    bool has_explicit_space = false;
    for (const auto& s : step.samplers) {
      if (s.binding.space.has_value()) {
        sampler_space = *s.binding.space;
        has_explicit_space = true;
        break;
      }
    }
    if (!has_explicit_space) {
      sampler_space = ctx.program.FindNextAvailableSampler(0, 0);
    }
    for (const auto& desc : step.samplers) {
      SamplerDesc resolved_desc = desc;
      ResolveBinding(resolved_desc.binding, sampler_space, resolved_desc.binding.register_index);
      if (!resolved_desc.binding.register_index.has_value()) {
        resolved_desc.binding.register_index = ctx.program.FindNextAvailableSampler(sampler_space, 0);
      }
      if (auto add_result = ctx.program.AddSampler(resolved_desc); !add_result) {
        const std::string message = "add_resource: sampler '" + desc.name + "': " + add_result.error();
        if (!fail_or_warn(message)) return std::unexpected(message);
        continue;
      }
      const auto& handle = desc.name.empty() ? "" : desc.name;
      ctx.samplers[handle] = resolved_desc;
      addSideEffect(dxp::ResourceKind::Sampler, handle, *resolved_desc.binding.register_index, sampler_space);
      ++result.samplers_added;
      changed = true;

      if (!handle.empty()) {
        const auto* sampler = FindResourceByRegisterIndex(*ctx.program.GetDxilModule(),
                                                          static_cast<hlsl::DXIL::ResourceClass>(ResourceClass::Sampler),
                                                          *resolved_desc.binding.register_index, sampler_space);
        if (sampler != nullptr) {
          llvm::Value* handle_value = ctx.program.CreateResourceHandle(*sampler, dxp::sm6::ShaderProgram::ToDxilBinding(resolved_desc.binding));
          if (handle_value != nullptr) {
            ctx.resource_handle_values[handle] = handle_value;
          }
          ctx.resource_handles[handle] = sampler;
        }
      }
    }
  }

  if (!step.inputs.empty()) {
    for (const auto& decl : step.inputs) {
      unsigned register_index = 0;
      if (decl.register_index.has_value()) {
        register_index = *decl.register_index;
      } else {
        register_index = ctx.program.FindNextAvailableInput();
      }
      if (!ctx.program.AddInputSignature(decl.semantic_name,
                                         static_cast<hlsl::CompType::Kind>(decl.comp_type),
                                         decl.vector_size, register_index,
                                         hlsl::InterpolationMode(static_cast<uint8_t>(decl.interp_mode)))) {
        const std::string message = "add_resource: input '" + decl.handle + "' failed";
        if (!fail_or_warn(message)) return std::unexpected(message);
        continue;
      }
      const auto& handle = decl.handle.empty() ? "" : decl.handle;
      ctx.input_bindings[handle] = register_index;
      addSideEffect(dxp::ResourceKind::Input, handle, register_index, 0);
      ++result.inputs_added;
      changed = true;
    }
  }

  if (!step.outputs.empty()) {
    for (const auto& decl : step.outputs) {
      unsigned register_index = 0;
      if (decl.register_index.has_value()) {
        register_index = *decl.register_index;
      } else {
        register_index = ctx.program.FindNextAvailableOutput();
      }
      if (!ctx.program.AddOutputSignature(decl.semantic_name,
                                          static_cast<hlsl::CompType::Kind>(decl.comp_type),
                                          decl.vector_size, register_index)) {
        const std::string message = "add_resource: output '" + decl.handle + "' failed";
        if (!fail_or_warn(message)) return std::unexpected(message);
        continue;
      }
      const auto& handle = decl.handle.empty() ? "" : decl.handle;
      ctx.output_bindings[handle] = register_index;
      addSideEffect(dxp::ResourceKind::Output, handle, register_index, 0);
      ++result.outputs_added;
      changed = true;
    }
  }

  ctx.program_modified = ctx.program_modified || changed;
  ctx.state[step.name] = true;
  return result;
}

std::expected<void, std::string> Validate(const AddResourceStep& step, ValidationContext& ctx) {
  if (step.name.empty()) {
    return std::unexpected("add_resource step requires a name");
  }

  if (!ctx.names.insert(step.name).second) {
    return std::unexpected("duplicate SM6 name '" + step.name + "' reused by step");
  }

  auto declareHandles = [&ctx](const auto& decls) {
    for (const auto& d : decls) {
      if (!d.name.empty()) ctx.handles.insert(d.name);
    }
  };
  declareHandles(step.textures);
  declareHandles(step.uavs);
  declareHandles(step.cbuffers);
  declareHandles(step.samplers);

  if (auto r = ValidateCondition<AddResourceStep::Results>(step.condition, ctx); !r) {
    return std::unexpected(r.error());
  }

  return {};
}

auto AddResourceData::Compile() const -> std::expected<AddResourceStep, std::string> {
  AddResourceStep step;
  step.name = name;
  step.condition = condition.Compile();
  step.required = required;

  step.textures.reserve(textures.size());
  for (const auto& d : textures) {
    TextureResourceDesc desc{};
    desc.name = d.handle.empty() ? "" : d.handle;
    desc.kind = d.kind;
    desc.element_kind = d.element_type;
    desc.vector_width = d.vector_width > 0 ? d.vector_width : 4;
    desc.binding = ResourceBindingDesc{.resource_class = ResourceClass::SRV, .register_index = d.register_index, .space = d.space};
    step.textures.push_back(std::move(desc));
  }

  step.uavs.reserve(uavs.size());
  for (const auto& d : uavs) {
    TextureResourceDesc desc{};
    desc.name = d.handle.empty() ? "" : d.handle;
    desc.kind = d.kind;
    desc.element_kind = d.element_type;
    desc.vector_width = d.vector_width > 0 ? d.vector_width : 4;
    desc.binding = ResourceBindingDesc{.resource_class = ResourceClass::UAV, .register_index = d.register_index, .space = d.space};
    desc.is_read_write = true;
    step.uavs.push_back(std::move(desc));
  }

  step.cbuffers.reserve(cbuffers.size());
  for (const auto& d : cbuffers) {
    CBufferDesc desc{};
    desc.name = d.handle.empty() ? "" : d.handle;
    desc.binding = ResourceBindingDesc{.resource_class = ResourceClass::CBuffer, .register_index = d.register_index, .space = d.space};
    desc.size_in_bytes = d.size;
    if (!d.fields.empty()) {
      auto* schema = new CBufferSchema{};
      schema->type_name = d.type.empty() ? desc.name : d.type;
      if (d.size == 0 && !d.fields.empty()) {
        auto end_offset = [](const auto& f) {
          return f.offset + ((f.width > 0 ? f.width : 1) * 4);
        };
        auto it = std::ranges::max_element(d.fields, {}, end_offset);
        const unsigned max_end = it != d.fields.end() ? end_offset(*it) : 0U;
        schema->size_in_bytes = max_end;
        desc.size_in_bytes = max_end;
      } else if (d.size == 0) {
        schema->size_in_bytes = 0;
        desc.size_in_bytes = 0;
      } else {
        schema->size_in_bytes = d.size;
      }
      for (const auto& f : d.fields) {
        CBufferFieldDesc fd{};
        fd.name = f.name;
        fd.comp_type = f.type;
        fd.offset = f.offset;
        fd.vector_size = f.width > 0 ? f.width : 1;
        schema->fields.push_back(fd);
      }
      desc.schema = schema;
    }
    step.cbuffers.push_back(std::move(desc));
  }

  step.samplers.reserve(samplers.size());
  for (const auto& d : samplers) {
    SamplerDesc desc{};
    desc.name = d.handle.empty() ? "" : d.handle;
    desc.binding = ResourceBindingDesc{.resource_class = ResourceClass::Sampler, .register_index = d.register_index, .space = d.space};
    step.samplers.push_back(std::move(desc));
  }

  step.inputs = inputs;
  step.outputs = outputs;

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

}  // namespace dxp::sm6::step
