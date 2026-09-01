#include <cstddef>
#include <cstdint>
#include <dxp/sm5/Model.hpp>
#include "dxp/sm5/EnumMirrors.hpp"
#include "dxp/sm5/InstructionLayout.hpp"
#include "dxp/sm5/Model_impl.hpp"

#include <algorithm>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "d3d11TokenizedProgramFormat.hpp"
#include "dxp/sm5/ShaderProgram.hpp"
#include "dxp/sm5/step/AddResourceStep.hpp"

namespace dxp::sm5::model {

auto GetOperandRole(Opcode opcode, size_t operand_index) -> OperandRole {
  const auto idx = static_cast<uint32_t>(opcode);
  if (idx >= kInstructionLayouts.size()) return OperandRole::Source;
  const auto& layout = kInstructionLayouts.at(idx);
  if (operand_index < layout.role_count && operand_index < layout.roles.size()) {
    return layout.roles.at(operand_index);
  }
  return OperandRole::Source;
}

auto GetExpectedOperandCount(Opcode opcode) -> uint32_t {
  const auto idx = static_cast<uint32_t>(opcode);
  if (idx >= kInstructionLayouts.size()) return 0;
  return kInstructionLayouts.at(idx).role_count;
}

auto GetExpectedOperandType(Opcode opcode, size_t operand_index) -> OperandScalarType {
  const auto idx = static_cast<uint32_t>(opcode);
  if (idx >= kInstructionLayouts.size()) return OperandScalarType::Unknown;
  const auto& layout = kInstructionLayouts.at(idx);
  if (operand_index < layout.role_count && operand_index < layout.types.size()) {
    return layout.types.at(operand_index);
  }
  return OperandScalarType::Unknown;
}

namespace {

auto EncodeOperandToken0(const Operand& operand) -> uint32_t {
  uint32_t token0 = 0;

  token0 |=
      ENCODE_D3D10_SB_OPERAND_NUM_COMPONENTS(static_cast<D3D10_SB_OPERAND_NUM_COMPONENTS>(operand.components.num_components));

  // Splice the component mode (bits 2-11: selection mode + mask/swizzle/select
  // value) directly into the token — the same bit layout ParseDecodeComponentMode
  // extracts, so parse/encode round-trip losslessly.
  token0 |= (operand.component_mode & kComponentModeBits);

  token0 |= ENCODE_D3D10_SB_OPERAND_TYPE(static_cast<uint32_t>(operand.type));

  size_t index_dims = 0;
  if (operand.type == OperandType::Immediate32 || operand.type == OperandType::Immediate64) {
    index_dims = 0;
  } else if (!operand.index_entries.empty()) {
    index_dims = std::min(operand.index_entries.size(), static_cast<size_t>(3));
  } else if (operand.relative_operand) {
    index_dims = 1;
  }

  token0 |= ENCODE_D3D10_SB_OPERAND_INDEX_DIMENSION(
      static_cast<D3D10_SB_OPERAND_INDEX_DIMENSION>(static_cast<uint32_t>(index_dims)));

  if (!operand.index_entries.empty()) {
    for (size_t dim = 0; dim < index_dims; ++dim) {
      uint32_t rep = D3D10_SB_OPERAND_INDEX_IMMEDIATE32;
      switch (operand.index_entries[dim].representation) {
        case Operand::IndexRepresentation::Immediate32:
          rep = D3D10_SB_OPERAND_INDEX_IMMEDIATE32;
          break;
        case Operand::IndexRepresentation::Immediate64:
          rep = D3D10_SB_OPERAND_INDEX_IMMEDIATE64;
          break;
        case Operand::IndexRepresentation::Relative:
          rep = D3D10_SB_OPERAND_INDEX_RELATIVE;
          break;
        case Operand::IndexRepresentation::Immediate32PlusRelative:
          rep = D3D10_SB_OPERAND_INDEX_IMMEDIATE32_PLUS_RELATIVE;
          break;
        case Operand::IndexRepresentation::Immediate64PlusRelative:
          rep = D3D10_SB_OPERAND_INDEX_IMMEDIATE64_PLUS_RELATIVE;
          break;
      }
      token0 |=
          ENCODE_D3D10_SB_OPERAND_INDEX_REPRESENTATION(static_cast<D3D10_SB_OPERAND_INDEX_DIMENSION>(dim),
                                                       static_cast<D3D10_SB_OPERAND_INDEX_REPRESENTATION>(rep));
    }
  } else if (operand.relative_operand) {
    token0 |= ENCODE_D3D10_SB_OPERAND_INDEX_REPRESENTATION(D3D10_SB_OPERAND_INDEX_1D,
                                                           D3D10_SB_OPERAND_INDEX_RELATIVE);
  }

  if (operand.modifier != OperandModifier::None) {
    token0 |= ENCODE_D3D10_SB_OPERAND_EXTENDED(1);
  }

  return token0;
}

auto EncodeDeclarationOperand(OperandType type, const std::vector<uint32_t>& indices) -> std::vector<uint32_t> {
  Operand operand;
  operand.type = type;
  operand.components.num_components = static_cast<NumComponents>(D3D10_SB_OPERAND_0_COMPONENT);
  operand.component_mode = 0;
  for (uint32_t idx : indices) {
    Operand::Index entry;
    entry.representation = Operand::IndexRepresentation::Immediate32;
    entry.immediate_lo = idx;
    operand.index_entries.push_back(std::move(entry));
  }
  return operand.Encode();
}

auto EncodeFloatResourceReturnTypeToken() -> uint32_t {
  return ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, 0) | ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, 1) | ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, 2) | ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, 3);
}

auto EncodeResourceDeclaration(const Instruction& instruction) -> std::vector<uint32_t> {
  if (instruction.operands.empty() || instruction.operands.front().index_entries.empty()) {
    return {};
  }

  // Extract dimension and return type from extended_op_codes if present
  // (recipe-specified overrides); otherwise fall back to parsed controls.
  std::optional<ResourceDimension> dimension;
  std::array<std::optional<ResourceReturnType>, 4> return_type;
  return_type.fill(std::nullopt);
  for (const auto& ext : instruction.controls.extended_op_codes) {
    const uint32_t kType = ext.value & kExtendedOpcodeTypeMask;
    if (kType == static_cast<uint32_t>(ExtendedOpcodeType::ResourceDim)) {
      dimension = static_cast<ResourceDimension>(DECODE_D3D11_SB_EXTENDED_RESOURCE_DIMENSION(ext.value));
    } else if (kType == static_cast<uint32_t>(ExtendedOpcodeType::ResourceType)) {
      for (uint32_t component = 0; component < 4; ++component) {
        return_type[component] = static_cast<ResourceReturnType>(
            DECODE_D3D11_SB_EXTENDED_RESOURCE_RETURN_TYPE(ext.value, component));
      }
    }
  }

  // Fall back to parsed controls if not overridden by extended_op_codes.
  if (!dimension.has_value()) {
    dimension = instruction.controls.resource_dimension.has_value()
                    ? instruction.controls.resource_dimension
                    : ResourceDimension::Texture2D;
  }
  bool any_return_type = false;
  for (uint32_t component = 0; component < 4; ++component) {
    if (!return_type[component].has_value()) {
      if (instruction.controls.resource_return_type[component].has_value()) {
        return_type[component] = instruction.controls.resource_return_type[component];
      } else {
        return_type[component] = ResourceReturnType::Float;
      }
    }
    if (return_type[component].has_value()) {
      any_return_type = true;
    }
  }
  (void)any_return_type;

  const auto resource_operand =
      EncodeDeclarationOperand(OperandType::Resource, {*instruction.operands.front().index_entries[0].immediate_lo});
  const uint32_t kLength = 1U + static_cast<uint32_t>(resource_operand.size()) + 1U;

  std::vector<uint32_t> encoded;
  encoded.reserve(kLength);
  encoded.push_back(ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_DCL_RESOURCE) | ENCODE_D3D10_SB_RESOURCE_DIMENSION(static_cast<D3D10_SB_RESOURCE_DIMENSION>(*dimension)) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(kLength));
  encoded.insert(encoded.end(), resource_operand.begin(), resource_operand.end());
  uint32_t kReturnTypeToken = 0;
  for (uint32_t component = 0; component < 4; ++component) {
    kReturnTypeToken |= ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(static_cast<D3D10_SB_RESOURCE_RETURN_TYPE>(static_cast<uint8_t>(*return_type[component])), component);
  }
  encoded.push_back(kReturnTypeToken);
  return encoded;
}

auto EncodeConstantBufferDeclaration(const Instruction& instruction) -> std::vector<uint32_t> {
  if (instruction.operands.empty() || instruction.operands.front().index_entries.size() < 2) {
    return {};
  }

  uint32_t access_pattern = D3D10_SB_CONSTANT_BUFFER_IMMEDIATE_INDEXED;
  if (instruction.controls.access_pattern.has_value()) {
    access_pattern = static_cast<uint32_t>(static_cast<uint8_t>(*instruction.controls.access_pattern));
  } else if (instruction.controls.access_pattern_raw != 0) {
    access_pattern = instruction.controls.access_pattern_raw;
  }

  // Encode the parsed operand directly (component selection, num_components,
  // indices) rather than rebuilding a canonical form: the source convention is
  // num_components Four + no-swizzle (e.g. 00208e46), which Flugan requires.
  const auto cbuffer_operand = instruction.operands.front().Encode();
  const uint32_t kLength = 1U + static_cast<uint32_t>(cbuffer_operand.size());

  std::vector<uint32_t> encoded;
  encoded.reserve(kLength);
  encoded.push_back(ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER) | ENCODE_D3D10_SB_D3D10_SB_CONSTANT_BUFFER_ACCESS_PATTERN(access_pattern) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(kLength));
  encoded.insert(encoded.end(), cbuffer_operand.begin(), cbuffer_operand.end());
  return encoded;
}

auto EncodeTempDeclaration(const Instruction& instruction) -> std::vector<uint32_t> {
  uint32_t temp_count = 0;
  if (!instruction.operands.empty() && !instruction.operands.front().index_entries.empty()) {
    temp_count = *instruction.operands.front().index_entries[0].immediate_lo;
  }

  return {
      ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_DCL_TEMPS) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(2),
      temp_count,
  };
}

auto EncodeInstructionToken0(const Instruction& instruction, uint32_t total_dwords, bool has_extended_tokens) -> uint32_t {
  uint32_t token0 = 0;
  const auto opcode = static_cast<uint32_t>(instruction.opcode);

  token0 |= ENCODE_D3D10_SB_OPCODE_TYPE(opcode);

  token0 |= ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(total_dwords);

  // Extended-bit must reflect the tokens actually emitted: parsed instructions
  // carry theirs in controls, but resource-access synthesis appends the
  // canonical pair without touching controls — callers pass that in.
  if (has_extended_tokens) {
    token0 |= ENCODE_D3D10_SB_OPCODE_EXTENDED(1);
  }

  if (instruction.controls.saturate) {
    token0 |= ENCODE_D3D10_SB_INSTRUCTION_SATURATE(1);
  }

  if (instruction.controls.test_boolean.has_value()) {
    token0 |= ENCODE_D3D10_SB_INSTRUCTION_TEST_BOOLEAN(
        static_cast<D3D10_SB_INSTRUCTION_TEST_BOOLEAN>(*instruction.controls.test_boolean));
  }

  if (instruction.controls.precise_values != 0U) {
    token0 |= ENCODE_D3D11_SB_INSTRUCTION_PRECISE_VALUES(instruction.controls.precise_values);
  }

  if (instruction.controls.resinfo_return_type != 0U) {
    token0 |= ENCODE_D3D10_SB_RESINFO_INSTRUCTION_RETURN_TYPE(
        static_cast<D3D10_SB_RESINFO_INSTRUCTION_RETURN_TYPE>(instruction.controls.resinfo_return_type));
  }

  if ((opcode == D3D10_SB_OPCODE_DCL_INPUT_PS || opcode == D3D10_SB_OPCODE_DCL_INPUT_PS_SIV) && instruction.controls.input_interpolation_mode.has_value()) {
    token0 |= ENCODE_D3D10_SB_INPUT_INTERPOLATION_MODE(
        static_cast<D3D10_SB_INTERPOLATION_MODE>(*instruction.controls.input_interpolation_mode));
  }

  if (opcode == D3D10_SB_OPCODE_DCL_SAMPLER) {
    uint32_t sampler_mode = static_cast<uint32_t>(instruction.sampler_mode);
    if (instruction.controls.mode.has_value()) {
      sampler_mode = static_cast<uint32_t>(static_cast<uint8_t>(*instruction.controls.mode));
    }
    token0 |= ENCODE_D3D10_SB_SAMPLER_MODE(static_cast<D3D10_SB_SAMPLER_MODE>(sampler_mode));
  }

  if (opcode == D3D10_SB_OPCODE_DCL_GLOBAL_FLAGS && instruction.controls.sync_flags != 0) {
    token0 |= ENCODE_D3D10_SB_GLOBAL_FLAGS(instruction.controls.sync_flags);
  }

  if (opcode == D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER && instruction.controls.access_pattern_raw != 0) {
    token0 |= ENCODE_D3D10_SB_D3D10_SB_CONSTANT_BUFFER_ACCESS_PATTERN(instruction.controls.access_pattern_raw);
  }

  if (opcode == D3D10_SB_OPCODE_DCL_RESOURCE) {
    if (instruction.controls.resource_dimension.has_value()) {
      token0 |= ENCODE_D3D10_SB_RESOURCE_DIMENSION(static_cast<D3D10_SB_RESOURCE_DIMENSION>(static_cast<uint8_t>(*instruction.controls.resource_dimension)));
    }
    bool any_return_type = false;
    for (uint32_t component = 0; component < 4; ++component) {
      if (instruction.controls.resource_return_type[component].has_value()) {
        token0 |= ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(static_cast<D3D10_SB_RESOURCE_RETURN_TYPE>(static_cast<uint8_t>(*instruction.controls.resource_return_type[component])), component);
        any_return_type = true;
      }
    }
    if (any_return_type) {
      token0 |= D3D10_SB_OPCODE_EXTENDED_MASK;
    }
  }

  if ((opcode == D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW || opcode == D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED || opcode == D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED) && instruction.controls.uav_flags != 0) {
    token0 |= ENCODE_D3D11_SB_ACCESS_COHERENCY_FLAGS(instruction.controls.uav_flags);
    token0 |= ENCODE_D3D11_SB_UAV_FLAGS(instruction.controls.uav_flags);
  }

  return token0;
}

}  // anonymous namespace

auto Operand::Index::operator==(const Index& rhs) const -> bool {
  return representation == rhs.representation && immediate_lo == rhs.immediate_lo && immediate_hi == rhs.immediate_hi && relative_operand == rhs.relative_operand;
}

auto Operand::operator==(const Operand& rhs) const -> bool {
  if (type != rhs.type) return false;
  if (component_mode != rhs.component_mode) return false;
  if (components.num_components != rhs.components.num_components) return false;
  if (modifier != rhs.modifier) return false;
  if (index_entries != rhs.index_entries) return false;
  if (relative_operand.has_value() != rhs.relative_operand.has_value()) return false;
  if (relative_operand.has_value() && **relative_operand != **rhs.relative_operand) return false;
  return true;
}

auto Instruction::Encode() const -> std::vector<uint32_t> {
  if (opcode == Opcode::CustomData) {
    std::vector<uint32_t> encoded;
    encoded.reserve(1 + custom_data.size());
    // Emit the preserved raw opcode token (custom-data class lives in its high bits).
    const uint32_t token0 = custom_data_opcode_token != 0
                                ? custom_data_opcode_token
                                : (ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_CUSTOMDATA)
                                   | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(static_cast<uint32_t>(1 + custom_data.size())));
    encoded.push_back(token0);
    encoded.insert(encoded.end(), custom_data.begin(), custom_data.end());
    return encoded;
  }

  if (opcode == Opcode::DclResource) {
    return EncodeResourceDeclaration(*this);
  }

  if (opcode == Opcode::DclConstantBuffer) {
    return EncodeConstantBufferDeclaration(*this);
  }

  if (opcode == Opcode::DclTemps) {
    return EncodeTempDeclaration(*this);
  }

  if (opcode == Opcode::DclResourceStructured || opcode == Opcode::DclUnorderedAccessViewStructured) {
    std::vector<uint32_t> encoded;
    uint32_t total_length = 1;
    for (const auto& operand : operands) {
      total_length += static_cast<uint32_t>(operand.Encode().size());
    }
    if (controls.structure_stride != 0) {
      total_length += 1;
    }
    const uint32_t token0 = EncodeInstructionToken0(*this, total_length, !controls.extended_op_codes.empty());
    encoded.push_back(token0);
    for (const auto& operand : operands) {
      auto operand_tokens = operand.Encode();
      encoded.insert(encoded.end(), operand_tokens.begin(), operand_tokens.end());
    }
    if (controls.structure_stride != 0) {
      encoded.push_back(controls.structure_stride);
    }
    return encoded;
  }

  // SM5 resource-access opcodes (ld, sample family) canonically carry a
  // ResourceDim + ResourceReturnType extended-opcode pair; the HLSL compiler
  // always emits them. Instructions parsed from source carry their raw
  // extended tokens; instructions synthesized from recipe emits don't, so
  // synthesize the canonical pair from the resource declaration's dimension
  // and return type (stamped by the recipe engine at emit time). ld_raw /
  // ld_structured use a fixed dimension + MIXED return type (their
  // declarations carry no return types).
  const ExtendedChainSpec kChain = RequiredExtendedChainForOpcode(opcode);
  std::vector<ExtendedOpcode> extended_op_codes = controls.extended_op_codes;
  const bool synthesize_resource_ext = extended_op_codes.empty()
                                       && kChain.RequiresResourcePair()
                                       && (controls.resource_dimension.has_value() || kChain.HasFixedMetadata());
  if (synthesize_resource_ext) {
    extended_op_codes.emplace_back(0U);  // ResourceDim
    extended_op_codes.emplace_back(0U);  // ResourceReturnType
  }

  // SIV/SGV declarations synthesize their trailing NameToken from controls when
  // only the register operand is present; parsed instructions round-trip verbatim.
  const bool synthesize_name_token = OpcodeUsesSemanticName(opcode)
                                     && controls.semantic_name.has_value()
                                     && operands.size() < GetExpectedOperandCount(opcode);

  uint32_t total_length = 1 + static_cast<uint32_t>(extended_op_codes.size()) + (synthesize_name_token ? 1U : 0U);
  for (const auto& operand : operands) {
    total_length += static_cast<uint32_t>(operand.Encode().size());
  }

  const uint32_t token0 = EncodeInstructionToken0(*this, total_length, !extended_op_codes.empty());

  std::vector<uint32_t> encoded;
  encoded.push_back(token0);

  // Extended opcode tokens sit between the opcode token and the operands
  // (e.g. sample controls); re-emit the raw tokens parsed from the source.
  if (synthesize_resource_ext) {
    // ResourceDim (bit 31 set: another extended opcode follows), then
    // ResourceReturnType (bit 31 clear: last). Matches the HLSL compiler's
    // canonical encoding for ld/sample instructions.
    encoded.push_back(ENCODE_D3D10_SB_EXTENDED_OPCODE_TYPE(D3D11_SB_EXTENDED_OPCODE_RESOURCE_DIM)
                      | ENCODE_D3D11_SB_EXTENDED_RESOURCE_DIMENSION(kChain.HasFixedMetadata() ? kChain.fixed_dimension : static_cast<uint32_t>(*controls.resource_dimension))
                      | ENCODE_D3D11_SB_EXTENDED_RESOURCE_DIMENSION_STRUCTURE_STRIDE(controls.structure_stride)
                      | D3D10_SB_OPCODE_EXTENDED_MASK);
    uint32_t return_type_token = ENCODE_D3D10_SB_EXTENDED_OPCODE_TYPE(D3D11_SB_EXTENDED_OPCODE_RESOURCE_RETURN_TYPE);
    for (uint32_t component = 0; component < 4; ++component) {
      uint32_t return_type;
      if (kChain.HasFixedMetadata()) {
        return_type = (kChain.fixed_return_type >> (component * D3D10_SB_RESOURCE_RETURN_TYPE_NUMBITS)) & D3D10_SB_RESOURCE_RETURN_TYPE_MASK;
      } else if (controls.resource_return_type[component].has_value()) {
        return_type = static_cast<uint32_t>(static_cast<uint8_t>(*controls.resource_return_type[component]));
      } else {
        return_type = D3D10_SB_RETURN_TYPE_FLOAT;
      }
      return_type_token |= ENCODE_D3D11_SB_EXTENDED_RESOURCE_RETURN_TYPE(return_type, component);
    }
    encoded.push_back(return_type_token);
  } else {
    for (const auto& ext : extended_op_codes) {
      encoded.push_back(ext.value);
    }
  }

  for (const auto& operand : operands) {
    auto operand_tokens = operand.Encode();
    encoded.insert(encoded.end(), operand_tokens.begin(), operand_tokens.end());
  }

  if (synthesize_name_token) {
    encoded.push_back(ENCODE_D3D10_SB_NAME(static_cast<D3D10_SB_NAME>(static_cast<uint8_t>(*controls.semantic_name))));
  }

  return encoded;
}

auto Operand::Index::Encode() const -> std::vector<uint32_t> {
  std::vector<uint32_t> encoded;
  switch (representation) {
    case IndexRepresentation::Immediate32:
      if (immediate_lo.has_value()) encoded.push_back(*immediate_lo);
      break;
    case IndexRepresentation::Immediate64:
      if (immediate_lo.has_value()) encoded.push_back(*immediate_lo);
      if (immediate_hi.has_value()) encoded.push_back(*immediate_hi);
      break;
    case IndexRepresentation::Immediate32PlusRelative:
      if (immediate_lo.has_value()) encoded.push_back(*immediate_lo);
      break;
    case IndexRepresentation::Immediate64PlusRelative:
      if (immediate_lo.has_value()) encoded.push_back(*immediate_lo);
      if (immediate_hi.has_value()) encoded.push_back(*immediate_hi);
      break;
    case IndexRepresentation::Relative:
      break;
  }
  if (relative_operand) {
    auto rel_tokens = (*relative_operand)->Encode();
    encoded.insert(encoded.end(), rel_tokens.begin(), rel_tokens.end());
  }
  return encoded;
}

auto Operand::Encode() const -> std::vector<uint32_t> {
  std::vector<uint32_t> encoded;

  const uint32_t token0 = EncodeOperandToken0(*this);
  encoded.push_back(token0);

  if (modifier != OperandModifier::None) {
    encoded.push_back(
        ENCODE_D3D10_SB_EXTENDED_OPERAND_MODIFIER(static_cast<D3D10_SB_OPERAND_MODIFIER>(modifier)));
  }

  if (type == OperandType::Immediate32 || type == OperandType::Immediate64) {
    for (const Index& index : index_entries) {
      if (index.immediate_lo.has_value()) encoded.push_back(*index.immediate_lo);
      if (index.immediate_hi.has_value()) encoded.push_back(*index.immediate_hi);
    }
  } else if (!index_entries.empty()) {
    for (const Index& index : index_entries) {
      auto idx_tokens = index.Encode();
      encoded.insert(encoded.end(), idx_tokens.begin(), idx_tokens.end());
    }
  }

  if (relative_operand && index_entries.empty()) {
    auto rel_tokens = (*relative_operand)->Encode();
    encoded.insert(encoded.end(), rel_tokens.begin(), rel_tokens.end());
  }

  return encoded;
}

auto CapturedOperand::ResolveForRole(OperandRole new_role) const -> Operand {
  if (static_cast<uint32_t>(role) == static_cast<uint32_t>(new_role)) {
    return operand_data;
  }

  Operand result = operand_data;

  const uint32_t kFromSelectionMode =
      DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(operand_data.component_mode);

  if (static_cast<uint32_t>(role) == static_cast<uint32_t>(OperandRole::Source) && static_cast<uint32_t>(new_role) == static_cast<uint32_t>(OperandRole::Destination)) {
    uint32_t mask = 0;
    const auto selMode =
        static_cast<D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE>(kFromSelectionMode);

    switch (selMode) {
      case D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE: {
        const uint32_t m = DECODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(operand_data.component_mode);
        mask = m >> 4;
        break;
      }
      case D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE: {
        const uint32_t sel = DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECT_1(operand_data.component_mode);
        mask = 1U << sel;
        break;
      }
      case D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE: {
        for (int c = 0; c < 4; ++c) {
          const uint32_t src =
              DECODE_D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_SOURCE(operand_data.component_mode, c);
          mask |= (1U << src);
        }
        break;
      }
      default:
        return operand_data;
    }

    if (mask != 0) {
      result.component_mode =
          ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) | ENCODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(mask << 4);
    }
  } else if (static_cast<uint32_t>(role) == static_cast<uint32_t>(OperandRole::Destination) && static_cast<uint32_t>(new_role) == static_cast<uint32_t>(OperandRole::Source)) {
    const uint32_t kFromMask = DECODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(operand_data.component_mode);
    if (kFromMask != 0) {
      const uint32_t kMaskBits = kFromMask >> 4;
      uint32_t swizzle = 0;
      int masked_components = 0;
      uint32_t last_component = 0;
      for (int comp = 0; comp < 4; ++comp) {
        if ((kMaskBits & (1U << comp)) != 0U) {
          swizzle |= (static_cast<uint32_t>(comp) << (masked_components * 2));
          last_component = static_cast<uint32_t>(comp);
          ++masked_components;
        }
      }
      // Fill the remaining swizzle slots with the last masked component (the
      // DXBC convention for mask-to-swizzle conversion, e.g. .w -> .wwww).
      for (int slot = masked_components; slot < 4; ++slot) {
        swizzle |= (last_component << (slot * 2));
      }
      result.component_mode =
          ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE) | (swizzle << 4);
    }
  }

  return result;
}

bool Operand::ValidateForRole(OperandRole expected_role, const std::string& path, std::string& error) const {
  if (components.num_components != NumComponents::Four) {
    return true;
  }
  const uint32_t kSelMode = DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(component_mode);
  if (expected_role == OperandRole::Destination) {
    if (kSelMode != static_cast<uint32_t>(D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE)) {
      error = path + ": destination operand uses non-mask selection mode";
      return false;
    }
  }
  if (kSelMode == static_cast<uint32_t>(D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE)) {
    for (int comp_idx = 0; comp_idx < 4; ++comp_idx) {
      const int kSrc = DECODE_D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_SOURCE(component_mode, comp_idx);
      if (kSrc > 3) {
        error = path + ": operand swizzle selector " + std::to_string(kSrc) + " out of range";
        return false;
      }
    }
  }
  if (expected_role == OperandRole::Destination) {
    if (type == OperandType::UAV) {
      error = path + ": UAV cannot be used as destination operand";
      return false;
    }
  }
  return true;
}
}  // namespace dxp::sm5::model
