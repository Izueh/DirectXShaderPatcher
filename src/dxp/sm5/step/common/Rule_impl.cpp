#include "dxp/sm5/step/common/Rule_impl.hpp"
#include <dxp/sm5/Model.hpp>
#include <expected>
#include <string>
#include <unordered_set>
#include <utility>
#include "dxp/sm5/Model_impl.hpp"
#include "value_types/indirect.h"

namespace dxp::sm5::step {

auto CompileOperandPattern(const OperandData& operand_data, bool /*is_emit_operand*/) -> std::expected<OperandPattern, std::string> {
  std::string error;

  OperandPattern op_pattern;
  op_pattern.any = operand_data.any;
  if (operand_data.type.has_value()) {
    op_pattern.type = operand_data.type;
  }
  if (!operand_data.capture.empty()) {
    op_pattern.capture = operand_data.capture;
  }
  if (!operand_data.match_capture.empty()) {
    op_pattern.match_capture = operand_data.match_capture;
  }
  if (operand_data.modifier.has_value()) {
    op_pattern.modifier = operand_data.modifier;
  }
  // Propagate the explicit component spec (presence = the user specified
  // `components:`); unset keeps the pattern's defaults (mask/swizzle/select
  // empty, num_components -1).
  op_pattern.components = operand_data.components;
  if (const auto& components = operand_data.components) {
    if (components->num_components != NumComponents::Four) {
      op_pattern.num_components = static_cast<int32_t>(components->num_components);
    }
  }
  if (operand_data.handle) {
    op_pattern.handle = OperandPattern::Handle{
        .name = operand_data.handle->name,
        .element_index = operand_data.handle->element_index,
    };
  }
  if (operand_data.decl.has_value()) {
    OperandPattern::DeclConstraint constraint;
    constraint.dimension = operand_data.decl->dimension;
    constraint.return_type = operand_data.decl->return_type;
    constraint.structure_stride = operand_data.decl->structure_stride;
    constraint.mode = operand_data.decl->mode;
    constraint.access_pattern = operand_data.decl->access_pattern;
    constraint.semantic = operand_data.decl->semantic;
    constraint.interpolation = operand_data.decl->interpolation;
    op_pattern.decl = std::move(constraint);
  }
  op_pattern.export_as = operand_data.export_as;
  if (const auto& components = operand_data.components) {
    if (components->selection_mode == SelectionMode::Mask) {
      op_pattern.mask = components->value;
    } else if (components->selection_mode == SelectionMode::Swizzle) {
      op_pattern.swizzle = components->value;
    } else if (components->selection_mode == SelectionMode::Select) {
      op_pattern.select = components->value;
    }
  }
  for (const auto& idx : operand_data.indices) {
    OperandIndexPattern idx_pattern;
    idx_pattern.any = idx.any;
    idx_pattern.representation = idx.representation;
    const bool kHasRelativeOperand = idx.relative_operand != nullptr;
    if (idx.immediate_lo.has_value()) {
      idx_pattern.immediate_lo = idx.immediate_lo;
    }
    if (idx.immediate_hi.has_value()) {
      idx_pattern.immediate_hi = idx.immediate_hi;
    }
    if (!idx.capture.empty()) {
      idx_pattern.capture = idx.capture;
    }
    if (!idx.match_capture.empty()) {
      idx_pattern.match_capture = idx.match_capture;
    }
    {
      if (kHasRelativeOperand) {
        auto rel_pattern = CompileOperandPattern(*idx.relative_operand, false);
        if (!rel_pattern) {
          return std::unexpected(rel_pattern.error());
        }
        idx_pattern.relative_operand = xyz::indirect<OperandPattern>(std::move(*rel_pattern));
      }
    }
    op_pattern.indices.push_back(std::move(idx_pattern));
  }
  op_pattern.immediates_u32 = operand_data.immediates_u32;
  op_pattern.immediates_u64 = operand_data.immediates_u64;
  op_pattern.immediates_i32 = operand_data.immediates_i32;
  op_pattern.immediates_i64 = operand_data.immediates_i64;
  op_pattern.immediates_f32 = operand_data.immediates_f32;
  op_pattern.immediates_f64 = operand_data.immediates_f64;
  if (operand_data.handle) {
    op_pattern.handle = OperandPattern::Handle{
        .name = operand_data.handle->name,
        .element_index = operand_data.handle->element_index,
    };
  }
  return op_pattern;
}

std::expected<void, std::string> ValidateRepeatConfig(const RepeatConfig& repeat, const std::string& path) {
  if (repeat.times == 0) {
    return std::unexpected(path + ": repeat times must be >= 1");
  }
  // "Exactly one typed family" is a format guarantee enforced here (Glaze
  // cannot express it as a variant for array alternatives — see RepeatData).
  for (const auto& [name, arr] : repeat.params) {
    if (name == "iteration") {
      return std::unexpected(path + ": param name 'iteration' is reserved (implicit 0-based repeat index variable)");
    }
    bool has_values = false;
    if (!arr.u32.empty()) {
      has_values = true;
      if (arr.u32.size() != repeat.times) return std::unexpected(path + ": param '" + name + "' u32 array length " + std::to_string(arr.u32.size()) + " != times " + std::to_string(repeat.times));
    }
    if (!arr.i32.empty()) {
      has_values = true;
      if (arr.i32.size() != repeat.times) return std::unexpected(path + ": param '" + name + "' i32 array length " + std::to_string(arr.i32.size()) + " != times " + std::to_string(repeat.times));
    }
    if (!arr.u64.empty()) {
      has_values = true;
      if (arr.u64.size() != repeat.times) return std::unexpected(path + ": param '" + name + "' u64 array length " + std::to_string(arr.u64.size()) + " != times " + std::to_string(repeat.times));
    }
    if (!arr.i64.empty()) {
      has_values = true;
      if (arr.i64.size() != repeat.times) return std::unexpected(path + ": param '" + name + "' i64 array length " + std::to_string(arr.i64.size()) + " != times " + std::to_string(repeat.times));
    }
    if (!arr.f32.empty()) {
      has_values = true;
      if (arr.f32.size() != repeat.times) return std::unexpected(path + ": param '" + name + "' f32 array length " + std::to_string(arr.f32.size()) + " != times " + std::to_string(repeat.times));
    }
    if (!arr.f64.empty()) {
      has_values = true;
      if (arr.f64.size() != repeat.times) return std::unexpected(path + ": param '" + name + "' f64 array length " + std::to_string(arr.f64.size()) + " != times " + std::to_string(repeat.times));
    }
    if (!has_values) return std::unexpected(path + ": param '" + name + "' has no typed array");
  }
  return {};
}

std::optional<RepeatConfig> CompileRepeatConfig(const std::optional<RepeatData>& repeat_data) {
  if (!repeat_data.has_value()) {
    return std::nullopt;
  }
  const auto& yaml_repeat = *repeat_data;
  RepeatConfig rc{};
  rc.times = yaml_repeat.times;
  for (const auto& [param_name, arr] : yaml_repeat.params) {
    auto& param = rc.params[param_name];
    param.u32 = arr.u32;
    param.i32 = arr.i32;
    param.u64 = arr.u64;
    param.i64 = arr.i64;
    param.f32 = arr.f32;
    param.f64 = arr.f64;
  }
  return rc;
}

int ComponentIndex(char c) {
  switch (c) {
    case 'x': return 0;
    case 'y': return 1;
    case 'z': return 2;
    case 'w': return 3;
    default:  return -1;
  }
}

std::optional<uint32_t> PatternComponentMode(const OperandPattern& op) {
  if (!op.mask.empty()) {
    uint32_t mask = 0;
    for (const char c : op.mask) {
      const int idx = ComponentIndex(c);
      if (idx < 0) return std::nullopt;
      mask |= (1U << idx);
    }
    // ENCODE_..._MASK takes the mask in token position (bits 4-7), so the
    // xyzw nibble is shifted up by the mask field's bit offset.
    return ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) | ENCODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(mask << 4);
  }
  if (!op.swizzle.empty()) {
    uint32_t swizzle = 0;
    int slot = 0;
    for (const char c : op.swizzle) {
      const int idx = ComponentIndex(c);
      if (idx < 0) return std::nullopt;
      swizzle |= (static_cast<uint32_t>(idx) << (slot * 2));
      ++slot;
    }
    return ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE) | (swizzle << 4);
  }
  if (!op.select.empty()) {
    const int idx = ComponentIndex(op.select.front());
    if (idx < 0) return std::nullopt;
    return ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE) | ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECT_1(static_cast<uint32_t>(idx));
  }
  return std::nullopt;
}

std::unordered_set<std::string> CollectRequiredCaptures(const std::vector<EmitPattern>& emits) {
  std::unordered_set<std::string> required;
  for (const auto& emit : emits) {
    if (!emit.capture.empty()) {
      required.insert(emit.capture);
    }
    for (const auto& op : emit.operands) {
      if (!op.capture.empty()) {
        required.insert(op.capture);
      }
      for (const auto& idx : op.IndexPatterns()) {
        if (!idx.capture.empty()) {
          required.insert(idx.capture);
        }
      }
    }
  }
  return required;
}

std::expected<void, std::string> ValidateEmitPatterns(const std::vector<EmitPattern>& emits, const std::string& path_prefix) {
  // Shared emit-pattern validation (Validate phase only — not Compile/parse).
  // Common to rule emits and template emits: operand count against the
  // opcode's instruction layout, per-operand completeness (including the
  // temp-handle component requirement), destination mask-only rule, per-slot
  // expected operand type, and declaration-field opcode restrictions.
  auto validateEmitOperand = [](const OperandPattern& op, const std::string& path) -> std::expected<void, std::string> {
    if (!op.match_capture.empty()) {
      return std::unexpected(path + ": match_capture is match-only; use capture to replay a captured operand in emit");
    }
    if (op.decl.has_value()) {
      return std::unexpected(path + ": decl is match-only — declaration constraints cannot be used in emit operands");
    }
    if (op.any) {
      return std::unexpected(path + ": any is not valid in emit operands");
    }
    if (!op.mask.empty()) {
      if (op.mask.size() > 4) {
        return std::unexpected(path + ": mask value '" + op.mask + "' has more than 4 components");
      }
      for (const char c : op.mask) {
        if (ComponentIndex(c) < 0) {
          return std::unexpected(path + ": mask value '" + op.mask + "' contains invalid component (expected xyzw)");
        }
      }
    }
    if (!op.swizzle.empty()) {
      if (op.swizzle.size() != 4) {
        return std::unexpected(path + ": swizzle value '" + op.swizzle + "' must have exactly 4 components (e.g. xyzw)");
      }
      for (const char c : op.swizzle) {
        if (ComponentIndex(c) < 0) {
          return std::unexpected(path + ": swizzle value '" + op.swizzle + "' contains invalid component (expected xyzw)");
        }
      }
    }
    if (!op.select.empty()) {
      if (op.select.size() != 1 || ComponentIndex(op.select.front()) < 0) {
        return std::unexpected(path + ": select value '" + op.select + "' must be a single component (x, y, z, or w)");
      }
    }
    const bool is_immediate =
        op.type.has_value() && (*op.type == OperandType::Immediate32 || *op.type == OperandType::Immediate64);
    // Sampler operands carry no component selection in DXBC (e.g. the sampler
    // operand of sample_l is a bare s#); their encoding is derived.
    const bool is_sampler = op.type.has_value() && *op.type == OperandType::Sampler;
    const bool is_temp_handle = op.handle && (op.type == OperandType::Temp || op.type == OperandType::IndexableTemp);
    // Temp/indexable-temp handles carry no component mode — the user must
    // specify components explicitly (a capture operand inherits its mode
    // from the captured operand at runtime).
    if (is_temp_handle && op.capture.empty() && !op.components.has_value()) {
      return std::unexpected(path + ": temp handle operand has no component selection; temp handles carry no component mode — specify components: or capture: a previously matched operand");
    }
    if (PatternComponentMode(op).has_value() == false && op.capture.empty() && !is_immediate && !is_sampler) {
      return std::unexpected(path + ": emit operand has no component selection; specify components: or capture: a previously matched operand");
    }
    if (is_immediate) {
      if (op.num_components >= 0) {
        return std::unexpected(path + ": immediate operands derive num_components from the immediates count");
      }
      // Count values from either form: typed immediates arrays (immediates_u32 etc.)
      // or the manual indices: form — IndexPatterns() resolves both.
      const size_t value_count = op.IndexPatterns().size();
      if (value_count != 1 && value_count != 4) {
        return std::unexpected(path + ": immediate operands must carry exactly 1 or 4 values (got " + std::to_string(value_count) + ")");
      }
    }
    return {};
  };
  for (size_t ei = 0; ei < emits.size(); ++ei) {
    const auto& emit = emits[ei];
    if (!emit.opcode.has_value()) continue;
    const std::string emitPath = path_prefix + "[" + std::to_string(ei) + "]";
    uint32_t kExpectedOperands = GetExpectedOperandCount(*emit.opcode);
    // Declaration opcodes present as multiple "operands" in ASM but are encoded
    // with extended opcodes in DXBC. When extended_opcodes are present, subtract
    // the extended-opcode count from the expected operand count.
    if (!emit.extended_opcodes.empty()) {
      const Opcode kOpcode = *emit.opcode;
      const bool kIsDeclaration =
          kOpcode == Opcode::DclResource || kOpcode == Opcode::DclResourceRaw
          || kOpcode == Opcode::DclResourceStructured
          || kOpcode == Opcode::DclConstantBuffer || kOpcode == Opcode::DclSampler;
      if (kIsDeclaration) {
        kExpectedOperands =
            kExpectedOperands > static_cast<uint32_t>(emit.extended_opcodes.size())
                ? kExpectedOperands - static_cast<uint32_t>(emit.extended_opcodes.size())
                : 0;
      }
    }
    // Declaration opcodes with instruction-level fields only require the register operand.
    bool has_any_fields = emit.dimension.has_value() || emit.structure_stride != 0 || emit.access_pattern.has_value() || emit.mode.has_value() || emit.uav_flags != 0;
    for (uint32_t component = 0; component < 4; ++component) {
      if (emit.return_type[component].has_value()) {
        has_any_fields = true;
        break;
      }
    }
    if (has_any_fields) {
      const Opcode kOpcode = *emit.opcode;
      const bool kIsDeclaration =
          kOpcode == Opcode::DclResource || kOpcode == Opcode::DclResourceStructured
          || kOpcode == Opcode::DclUnorderedAccessViewTyped
          || kOpcode == Opcode::DclUnorderedAccessViewRaw || kOpcode == Opcode::DclUnorderedAccessViewStructured
          || kOpcode == Opcode::DclConstantBuffer || kOpcode == Opcode::DclSampler;
      if (kIsDeclaration && emit.operands.size() > 0) {
        kExpectedOperands = static_cast<uint32_t>(emit.operands.size());
      }
    }
    if (kExpectedOperands > 0 && emit.operands.size() != kExpectedOperands) {
      return std::unexpected(emitPath + ": opcode " + std::to_string(static_cast<uint32_t>(*emit.opcode)) + " expects " + std::to_string(kExpectedOperands) + " operands, recipe provides " + std::to_string(emit.operands.size()));
    }
    for (size_t oi = 0; oi < emit.operands.size(); ++oi) {
      const OperandPattern& op = emit.operands[oi];
      const std::string opPath = emitPath + ".operands[" + std::to_string(oi) + "]";
      if (auto r = validateEmitOperand(op, opPath); !r) return r;
      const OperandRole kRole = GetOperandRole(*emit.opcode, oi);
      if (kRole == OperandRole::Destination && (!op.swizzle.empty() || !op.select.empty())) {
        return std::unexpected(opPath + ": destination operand must use mask selection mode (not swizzle/select)");
      }
      const OperandScalarType kExpectedType = GetExpectedOperandType(*emit.opcode, oi);
      if (op.type.has_value()) {
        const bool kTypeOk =
            kExpectedType == OperandScalarType::Unknown || (kExpectedType == OperandScalarType::Texture && *op.type == OperandType::Resource) || (kExpectedType == OperandScalarType::Sampler && *op.type == OperandType::Sampler) || (kExpectedType == OperandScalarType::Uav && *op.type == OperandType::UAV) || (kExpectedType == OperandScalarType::CBuffer && *op.type == OperandType::CBuffer) || (kExpectedType == OperandScalarType::F32 || kExpectedType == OperandScalarType::U32 || kExpectedType == OperandScalarType::I32 || kExpectedType == OperandScalarType::F64 || kExpectedType == OperandScalarType::Bool);
        if (!kTypeOk) {
          return std::unexpected(opPath + ": operand type does not match the opcode's expected slot type (" + std::to_string(static_cast<uint32_t>(kExpectedType)) + ")");
        }
      }
    }
    const Opcode kEmitOpcode = *emit.opcode;
    bool has_resource_fields = emit.dimension.has_value() || emit.structure_stride != 0;
    for (uint32_t component = 0; component < 4; ++component) {
      if (emit.return_type[component].has_value()) {
        has_resource_fields = true;
        break;
      }
    }
    if (has_resource_fields) {
      if (kEmitOpcode != Opcode::DclResource && kEmitOpcode != Opcode::DclUnorderedAccessViewTyped && kEmitOpcode != Opcode::DclUnorderedAccessViewRaw && kEmitOpcode != Opcode::DclUnorderedAccessViewStructured) {
        return std::unexpected(emitPath + ": dimension/return_type/structure_stride fields are only valid for resource declaration opcodes");
      }
    }
    if (emit.access_pattern.has_value()) {
      if (kEmitOpcode != Opcode::DclConstantBuffer) {
        return std::unexpected(emitPath + ": access_pattern field is only valid for dcl_constant_buffer");
      }
    }
    if (emit.mode.has_value()) {
      if (kEmitOpcode != Opcode::DclSampler) {
        return std::unexpected(emitPath + ": mode field is only valid for dcl_sampler");
      }
    }
    if (emit.uav_flags != 0) {
      if (kEmitOpcode != Opcode::DclUnorderedAccessViewRaw && kEmitOpcode != Opcode::DclUnorderedAccessViewStructured && kEmitOpcode != Opcode::DclUnorderedAccessViewTyped) {
        return std::unexpected(emitPath + ": uav_flags field is only valid for UAV declaration opcodes");
      }
    }
  }
  return {};
}

}  // namespace dxp::sm5::step
