#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <unordered_map>

#include "dxp/sm5/ShaderProgram.hpp"

namespace dxp::sm5 {

using namespace dxp::sm5::model;

/// @brief Fully-resolved signature (input/output) declaration payload.
struct SignatureDecl {
  uint32_t register_bind_point = 0;
  uint32_t writemask = 0;                          ///< Component mask from the declaration operand.
  std::optional<InterpolationMode> interpolation;  ///< dcl_input_ps / dcl_input_ps_siv only.
  std::optional<SignatureSemantic> semantic;       ///< SIV/SGV declarations only (NameToken).
};

/// @brief Fully-resolved resource declaration payload (textures and UAVs).
struct ResourceDeclInfo {
  uint32_t register_bind_point = 0;
  ResourceDimension dimension = ResourceDimension::Unknown;
  std::array<ResourceReturnType, 4> return_types = {ResourceReturnType::Float, ResourceReturnType::Float, ResourceReturnType::Float, ResourceReturnType::Float};
  uint32_t structure_stride = 0;
  uint32_t uav_flags = 0;
};

/// @brief Fully-resolved cbuffer declaration payload.
struct CBufferDeclInfo {
  uint32_t register_bind_point = 0;
  uint32_t elements = 0;
  CbufferAccessPattern access_pattern = CbufferAccessPattern::ImmediateIndexed;
};

/// @brief Cross-reference index from register bind point to declaration payload,
/// built from the program's dcl_* instructions.
struct DeclarationIndex {
  std::unordered_map<uint32_t, ResourceDeclInfo> textures;  ///< dcl_resource / dcl_resource_raw / dcl_resource_structured, keyed by t#.
  std::unordered_map<uint32_t, ResourceDeclInfo> uavs;      ///< dcl_unordered_access_view_*, keyed by u#.
  std::unordered_map<uint32_t, SamplerMode> samplers;       ///< dcl_sampler, keyed by s#.
  std::unordered_map<uint32_t, CBufferDeclInfo> cbuffers;   ///< dcl_constant_buffer, keyed by cb#.
  std::unordered_map<uint32_t, SignatureDecl> inputs;       ///< dcl_input*, keyed by v# (attribute axis).
  std::unordered_map<uint32_t, SignatureDecl> outputs;      ///< dcl_output*, keyed by o#.

  /// @brief Resource declaration lookup across textures and UAVs (an operand's
  /// register family is disambiguated by its operand type at the call site).
  [[nodiscard]] const ResourceDeclInfo* FindResourceDecl(OperandType type, uint32_t register_index) const {
    if (type == OperandType::Resource) {
      const auto it = textures.find(register_index);
      return it != textures.end() ? &it->second : nullptr;
    }
    if (type == OperandType::UAV) {
      const auto it = uavs.find(register_index);
      return it != uavs.end() ? &it->second : nullptr;
    }
    return nullptr;
  }
};

/// @brief Builds the declaration index from a program's instruction stream.
[[nodiscard]] DeclarationIndex BuildDeclarationIndex(const ShaderProgram& program);

inline DeclarationIndex BuildDeclarationIndex(const ShaderProgram& program) {
  DeclarationIndex index;
  for (const auto& instr : program.instructions) {
    if (instr.operands.empty() || instr.operands.front().index_entries.empty()
        || !instr.operands.front().index_entries.front().immediate_lo.has_value()) {
      continue;
    }
    // Register is the FIRST index entry for all declarations (cbuffer appends
    // an element count, GS inputs prepend a vertex axis).
    const uint32_t register_index = *instr.operands.front().index_entries.front().immediate_lo;
    switch (instr.opcode) {
      case Opcode::DclResource:
      case Opcode::DclResourceRaw:
      case Opcode::DclResourceStructured: {
        ResourceDeclInfo decl;
        decl.register_bind_point = register_index;
        switch (instr.opcode) {
          case Opcode::DclResourceRaw:
            decl.dimension = ResourceDimension::RawBuffer;
            break;
          case Opcode::DclResourceStructured:
            decl.dimension = ResourceDimension::StructuredBuffer;
            decl.structure_stride = instr.controls.structure_stride;
            break;
          default:
            decl.dimension = instr.controls.resource_dimension.value_or(ResourceDimension::Unknown);
            break;
        }
        for (uint32_t component = 0; component < 4; ++component) {
          if (instr.controls.resource_return_type[component].has_value()) {
            decl.return_types[component] = *instr.controls.resource_return_type[component];
          }
        }
        index.textures.insert_or_assign(register_index, std::move(decl));
        break;
      }
      case Opcode::DclUnorderedAccessViewTyped:
      case Opcode::DclUnorderedAccessViewRaw:
      case Opcode::DclUnorderedAccessViewStructured: {
        ResourceDeclInfo decl;
        decl.register_bind_point = register_index;
        decl.uav_flags = instr.controls.uav_flags;
        switch (instr.opcode) {
          case Opcode::DclUnorderedAccessViewRaw:
            decl.dimension = ResourceDimension::RawBuffer;
            break;
          case Opcode::DclUnorderedAccessViewStructured:
            decl.dimension = ResourceDimension::StructuredBuffer;
            decl.structure_stride = instr.controls.structure_stride;
            break;
          default:
            decl.dimension = instr.controls.resource_dimension.value_or(ResourceDimension::Unknown);
            break;
        }
        for (uint32_t component = 0; component < 4; ++component) {
          if (instr.controls.resource_return_type[component].has_value()) {
            decl.return_types[component] = *instr.controls.resource_return_type[component];
          }
        }
        index.uavs.insert_or_assign(register_index, std::move(decl));
        break;
      }
      case Opcode::DclSampler:
        index.samplers.insert_or_assign(register_index, instr.controls.mode.value_or(instr.sampler_mode));
        break;
      case Opcode::DclConstantBuffer: {
        CBufferDeclInfo decl;
        decl.register_bind_point = register_index;
        if (instr.controls.access_pattern.has_value()) {
          decl.access_pattern = *instr.controls.access_pattern;
        } else if (instr.controls.access_pattern_raw != 0) {
          decl.access_pattern = static_cast<CbufferAccessPattern>(instr.controls.access_pattern_raw);
        }
        // The declaration operand carries [register, element_count] index entries.
        if (instr.operands.front().index_entries.size() >= 2
            && instr.operands.front().index_entries[1].immediate_lo.has_value()) {
          decl.elements = *instr.operands.front().index_entries[1].immediate_lo;
        }
        index.cbuffers.insert_or_assign(register_index, std::move(decl));
        break;
      }
      case Opcode::DclInput:
      case Opcode::DclInputSgv:
      case Opcode::DclInputSiv:
      case Opcode::DclInputPs:
      case Opcode::DclInputPsSgv:
      case Opcode::DclInputPsSiv:
      case Opcode::DclOutput:
      case Opcode::DclOutputSgv:
      case Opcode::DclOutputSiv:  {
        SignatureDecl decl;
        decl.register_bind_point = register_index;
        const auto& operand = instr.operands.front();
        const auto selection = DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(operand.component_mode);
        if (operand.components.num_components == NumComponents::Four) {
          if (selection == static_cast<uint32_t>(D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE)) {
            decl.writemask = operand.component_mode & D3D10_SB_OPERAND_4_COMPONENT_MASK_MASK;
          } else {
            decl.writemask = 0xF;  // swizzle/select declarations read all four components.
          }
        }
        if (instr.controls.semantic_name.has_value()) {
          decl.semantic = instr.controls.semantic_name;
        }
        if (instr.controls.input_interpolation_mode.has_value()) {
          decl.interpolation = static_cast<InterpolationMode>(*instr.controls.input_interpolation_mode);
        }
        const bool is_input = instr.opcode != Opcode::DclOutput && instr.opcode != Opcode::DclOutputSgv && instr.opcode != Opcode::DclOutputSiv;
        (is_input ? index.inputs : index.outputs).insert_or_assign(register_index, std::move(decl));
        break;
      }
      default:
        break;
    }
  }
  return index;
}

}  // namespace dxp::sm5
