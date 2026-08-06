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

namespace dxp::sm5 {

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

auto ShaderProgram::GetOpcodeCounts() const -> std::unordered_map<std::string, int32_t> {
  std::unordered_map<std::string, int32_t> counts;
  for (const auto& instr : instructions) {
    const auto val = static_cast<uint32_t>(instr.opcode);
    if (val < glz::meta<Opcode>::keys.size()) {
      counts[glz::meta<Opcode>::keys.at(val)]++;
    }
  }
  return counts;
}

auto ShaderProgram::GetInstructionOpcodes() const -> std::vector<Opcode> {
  std::vector<Opcode> opcodes;
  opcodes.reserve(instructions.size());
  for (const auto& instr : instructions) {
    opcodes.push_back(instr.opcode);
  }
  return opcodes;
}

auto ShaderProgram::FindNextAvailableTexture(unsigned preferred) const -> unsigned {
  std::unordered_set<uint32_t> occupied;
  for (const auto& r : resources) occupied.insert(r.register_bind_point);
  unsigned bp = preferred;
  while (occupied.contains(bp)) ++bp;
  return bp;
}

auto ShaderProgram::FindNextAvailableSampler(unsigned preferred) const -> unsigned {
  std::unordered_set<uint32_t> occupied;
  for (const auto& s : samplers) occupied.insert(s.register_bind_point);
  unsigned bp = preferred;
  while (occupied.contains(bp)) ++bp;
  return bp;
}

auto ShaderProgram::FindNextAvailableCBuffer(unsigned preferred) const -> unsigned {
  std::unordered_set<uint32_t> occupied;
  for (const auto& c : cbuffers) occupied.insert(c.register_bind_point);
  unsigned bp = preferred;
  while (occupied.contains(bp)) ++bp;
  return bp;
}

auto ShaderProgram::FindNextAvailableUAV(unsigned preferred) const -> unsigned {
  std::unordered_set<uint32_t> occupied;
  for (const auto& r : resources) occupied.insert(r.register_bind_point);
  unsigned bp = preferred;
  while (occupied.contains(bp)) ++bp;
  return bp;
}

namespace {

/// @brief Collects input/output signature register indices from DCL instructions.
void CollectSignatureRegisters(const std::vector<Instruction>& instructions,
                               std::unordered_set<uint32_t>& occupied, bool inputs) {
  for (const auto& instr : instructions) {
    const auto op = instr.opcode;
    const bool is_input = op == Opcode::DclInput || op == Opcode::DclInputPs || op == Opcode::DclInputPsSiv || op == Opcode::DclInputSgv || op == Opcode::DclInputSiv;
    const bool is_output = op == Opcode::DclOutput || op == Opcode::DclOutputSgv || op == Opcode::DclOutputSiv;
    if ((inputs && is_input) || (!inputs && is_output)) {
      if (!instr.operands.empty() && !instr.operands.front().index_entries.empty()) {
        const auto& idx = instr.operands.front().index_entries.front();
        if (idx.immediate_lo.has_value()) occupied.insert(*idx.immediate_lo);
      }
    }
  }
}

}  // namespace

auto ShaderProgram::FindNextAvailableInput() const -> unsigned {
  std::unordered_set<uint32_t> occupied;
  CollectSignatureRegisters(instructions, occupied, /*inputs=*/true);
  unsigned bp = 0;
  while (occupied.contains(bp)) ++bp;
  return bp;
}

auto ShaderProgram::FindNextAvailableOutput() const -> unsigned {
  std::unordered_set<uint32_t> occupied;
  CollectSignatureRegisters(instructions, occupied, /*inputs=*/false);
  unsigned bp = 0;
  while (occupied.contains(bp)) ++bp;
  return bp;
}

auto ShaderProgram::EnsureTempDeclaration() -> void {
  if (temp_count == 0) return;
  bool found_dcl_temps = false;
  for (auto& instruction : instructions) {
    if (instruction.opcode == Opcode::DclTemps) {
      instruction = BuildTempDeclaration(temp_count);
      found_dcl_temps = true;
      break;
    }
  }
  if (!found_dcl_temps) {
    uint32_t insert_index = 0;
    for (uint32_t i = 0; i < instructions.size(); ++i) {
      if (OpcodeIsDeclaration(instructions[i].opcode)) insert_index = i + 1;
    }
    instructions.insert(instructions.begin() + static_cast<ptrdiff_t>(insert_index),
                        BuildTempDeclaration(temp_count));
  }
}

auto ShaderProgram::MakeSelectComponentMode(uint32_t component) -> uint32_t {
  return ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE) | ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECT_1(static_cast<D3D10_SB_4_COMPONENT_NAME>(component));
}

auto ShaderProgram::MakeConstantBufferDeclarationOperand(uint32_t register_index, uint32_t element_count) -> Operand {
  Operand operand;
  operand.type = OperandType::CBuffer;
  operand.components.num_components = NumComponents::Four;
  operand.component_mode =
      ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE) | (ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE(D3D10_SB_4_COMPONENT_X, D3D10_SB_4_COMPONENT_Y, D3D10_SB_4_COMPONENT_Z, D3D10_SB_4_COMPONENT_W) << 4);
  Operand::Index bind_index;
  bind_index.representation = Operand::IndexRepresentation::Immediate32;
  bind_index.immediate_lo = register_index;
  operand.index_entries.push_back(std::move(bind_index));
  Operand::Index count_index;
  count_index.representation = Operand::IndexRepresentation::Immediate32;
  count_index.immediate_lo = element_count;
  operand.index_entries.push_back(std::move(count_index));
  return operand;
}

auto ShaderProgram::MakeSamplerOperand(uint32_t register_index) -> Operand {
  Operand operand;
  operand.type = OperandType::Sampler;
  operand.components.num_components = NumComponents::Zero;
  operand.component_mode = 0;
  Operand::Index idx;
  idx.representation = Operand::IndexRepresentation::Immediate32;
  idx.immediate_lo = register_index;
  operand.index_entries.push_back(std::move(idx));
  return operand;
}

auto ShaderProgram::MakeResourceOperand(uint32_t register_index) -> Operand {
  Operand operand;
  operand.type = OperandType::Resource;
  operand.components.num_components = NumComponents::Zero;
  operand.component_mode = 0;
  Operand::Index idx;
  idx.representation = Operand::IndexRepresentation::Immediate32;
  idx.immediate_lo = register_index;
  operand.index_entries.push_back(std::move(idx));
  return operand;
}

auto ShaderProgram::MakeInputOperand(uint32_t register_index) -> Operand {
  Operand operand;
  operand.type = OperandType::Input;
  operand.components.num_components = NumComponents::Four;
  operand.component_mode = ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) | D3D10_SB_OPERAND_4_COMPONENT_MASK_ALL;
  Operand::Index idx;
  idx.representation = Operand::IndexRepresentation::Immediate32;
  idx.immediate_lo = register_index;
  operand.index_entries.push_back(std::move(idx));
  return operand;
}

auto ShaderProgram::MakeOutputOperand(uint32_t register_index) -> Operand {
  Operand operand;
  operand.type = OperandType::Output;
  operand.components.num_components = NumComponents::Four;
  operand.component_mode = ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) | D3D10_SB_OPERAND_4_COMPONENT_MASK_ALL;
  Operand::Index idx;
  idx.representation = Operand::IndexRepresentation::Immediate32;
  idx.immediate_lo = register_index;
  operand.index_entries.push_back(std::move(idx));
  return operand;
}

auto ShaderProgram::MakeUavOperand(uint32_t register_index) -> Operand {
  Operand operand;
  operand.type = OperandType::UAV;
  operand.components.num_components = NumComponents::Zero;
  operand.component_mode = 0;
  Operand::Index idx;
  idx.representation = Operand::IndexRepresentation::Immediate32;
  idx.immediate_lo = register_index;
  operand.index_entries.push_back(std::move(idx));
  return operand;
}

auto ShaderProgram::FindInsertAfterLastDeclaration(Opcode opcode) -> uint32_t {
  uint32_t insert_index = 0;
  for (uint32_t i = 0; i < instructions.size(); ++i) {
    if (instructions[i].opcode == opcode) {
      insert_index = i + 1;
    }
  }
  return insert_index;
}

auto ShaderProgram::AllocateBindPoint(const std::unordered_set<uint32_t>& occupied, bool auto_bind,
                                      uint32_t requested_register_index, uint32_t& resolved_register_index, std::string& error) -> bool {
  if (!auto_bind) {
    if (occupied.contains(requested_register_index)) {
      error = "SM5 declaration register index already occupied: " + std::to_string(requested_register_index);
      return false;
    }
    resolved_register_index = requested_register_index;
    return true;
  }

  uint32_t candidate = requested_register_index;
  while (occupied.contains(candidate)) {
    if (candidate == std::numeric_limits<uint32_t>::max()) {
      error = "SM5 declaration auto_bind exhausted available bind points";
      return false;
    }
    ++candidate;
  }
  resolved_register_index = candidate;
  return true;
}

auto ShaderProgram::RecordNamedBinding(std::unordered_map<std::string, uint32_t>& bindings, const std::string& handle,
                                       uint32_t register_index, const char* resource_kind, std::string& error) -> bool {
  if (handle.empty()) {
    return true;
  }
  if (bindings.contains(handle)) {
    error = std::string("duplicate SM5 ") + resource_kind + " declaration handle: " + handle;
    return false;
  }
  bindings[handle] = register_index;
  return true;
}

auto ShaderProgram::FinalizeInstruction(Instruction instruction) -> Instruction {
  instruction.length_in_dwords = static_cast<uint32_t>(instruction.Encode().size());
  return instruction;
}

auto ShaderProgram::BuildTempDeclaration(uint32_t temp_count) -> Instruction {
  Instruction instruction;
  instruction.opcode = Opcode::DclTemps;
  Operand operand;
  operand.type = OperandType::Temp;
  operand.components.num_components = NumComponents::Zero;
  operand.component_mode = 0;
  Operand::Index idx;
  idx.representation = Operand::IndexRepresentation::Immediate32;
  idx.immediate_lo = temp_count;
  operand.index_entries.push_back(std::move(idx));
  instruction.operands.push_back(std::move(operand));
  return FinalizeInstruction(std::move(instruction));
}

auto ShaderProgram::BuildConstantBufferDeclaration(const dxp::sm5::step::AddResourceStep::CBufferDecl& decl) -> Instruction {
  Instruction instruction;
  instruction.opcode = Opcode::DclConstantBuffer;
  const uint32_t register_index = decl.register_index.value_or(0U);
  instruction.operands.push_back(MakeConstantBufferDeclarationOperand(register_index, decl.elements));
  instruction.controls.access_pattern = static_cast<uint32_t>(decl.access_pattern);
  return instruction;
}

auto ShaderProgram::BuildTextureDeclaration(const dxp::sm5::step::AddResourceStep::TextureDecl& decl) -> Instruction {
  Instruction instruction;
  instruction.opcode = Opcode::DclResource;
  const uint32_t register_index = decl.register_index.value_or(0U);
  instruction.operands.push_back(MakeResourceOperand(register_index));
  instruction.controls.resource_dimension = decl.dimension;
  instruction.controls.resource_return_type = ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, 0) | ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, 1) | ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, 2) | ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, 3);
  return instruction;
}

auto ShaderProgram::BuildInputDeclaration(const dxp::sm5::step::AddResourceStep::InputDecl& decl) -> Instruction {
  Instruction instruction;
  instruction.opcode = Opcode::DclInputPs;
  instruction.controls.input_interpolation_mode = static_cast<uint32_t>(decl.interpolation_mode);
  const uint32_t register_index = decl.register_index.value_or(0U);
  instruction.operands.push_back(MakeInputOperand(register_index));
  return instruction;
}

auto ShaderProgram::BuildOutputDeclaration(const dxp::sm5::step::AddResourceStep::OutputDecl& decl) -> Instruction {
  Instruction instruction;
  instruction.opcode = Opcode::DclOutput;
  const uint32_t register_index = decl.register_index.value_or(0U);
  instruction.operands.push_back(MakeOutputOperand(register_index));
  return instruction;
}

auto ShaderProgram::BuildSamplerDeclaration(const dxp::sm5::step::AddResourceStep::SamplerDecl& decl) -> Instruction {
  Instruction instruction;
  instruction.opcode = Opcode::DclSampler;
  const uint32_t register_index = decl.register_index.value_or(0U);
  instruction.operands.push_back(MakeSamplerOperand(register_index));
  instruction.sampler_mode = decl.mode;
  return instruction;
}

auto ShaderProgram::BuildRawResourceDeclaration(const dxp::sm5::step::AddResourceStep::RawResourceDecl& decl) -> Instruction {
  Instruction instruction;
  instruction.opcode = Opcode::DclResourceRaw;
  const uint32_t register_index = decl.register_index.value_or(0U);
  instruction.operands.push_back(MakeResourceOperand(register_index));
  return instruction;
}

auto ShaderProgram::BuildStructuredResourceDeclaration(const dxp::sm5::step::AddResourceStep::StructuredResourceDecl& decl) -> Instruction {
  Instruction instruction;
  instruction.opcode = Opcode::DclResourceStructured;
  const uint32_t register_index = decl.register_index.value_or(0U);
  instruction.operands.push_back(MakeResourceOperand(register_index));
  instruction.controls.structure_stride = decl.structure_stride;
  return instruction;
}

auto ShaderProgram::BuildUavDeclaration(const dxp::sm5::step::AddResourceStep::UavDecl& decl) -> Instruction {
  Instruction instruction;
  const uint32_t register_index = decl.register_index.value_or(0U);
  if (decl.kind == step::AddResourceStep::UavKind::Raw) {
    instruction.opcode = Opcode::DclUnorderedAccessViewRaw;
    instruction.operands.push_back(MakeUavOperand(register_index));
    instruction.controls.uav_flags = decl.globally_coherent ? D3D11_SB_GLOBALLY_COHERENT_ACCESS : 0;
    return instruction;
  }

  if (decl.kind == step::AddResourceStep::UavKind::Structured) {
    instruction.opcode = Opcode::DclUnorderedAccessViewStructured;
    instruction.operands.push_back(MakeUavOperand(register_index));
    uint32_t flags = decl.globally_coherent ? D3D11_SB_GLOBALLY_COHERENT_ACCESS : 0;
    if (decl.has_order_preserving_counter) {
      flags |= D3D11_SB_UAV_HAS_ORDER_PRESERVING_COUNTER;
    }
    instruction.controls.uav_flags = flags;
    instruction.controls.structure_stride = decl.structure_stride;
    return instruction;
  }

  instruction.opcode = Opcode::DclUnorderedAccessViewTyped;
  instruction.operands.push_back(MakeUavOperand(register_index));
  instruction.controls.resource_dimension = decl.dimension;
  instruction.controls.resource_return_type = ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, 0) | ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, 1) | ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, 2) | ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, 3);
  instruction.controls.uav_flags = decl.globally_coherent ? D3D11_SB_GLOBALLY_COHERENT_ACCESS : 0;
  return instruction;
}

auto ShaderProgram::AddInputDeclaration(const dxp::sm5::step::AddResourceStep::InputDecl& decl, uint32_t& out_register_index, std::string& error) -> bool {
  std::unordered_set<uint32_t> occupied;
  uint32_t insert_index = 0;
  for (uint32_t i = 0; i < instructions.size(); ++i) {
    const auto opcode = instructions[i].opcode;
    if ((opcode == Opcode::DclInput || opcode == Opcode::DclInputPs || opcode == Opcode::DclInputPsSiv || opcode == Opcode::DclInputPsSgv) && !instructions[i].operands.empty() && !instructions[i].operands.front().index_entries.empty()) {
      occupied.insert(*instructions[i].operands.front().index_entries[0].immediate_lo);
      insert_index = i + 1;
    }
  }

  const bool is_auto = !decl.register_index.has_value();
  const uint32_t requested = is_auto ? 0U : *decl.register_index;
  if (!AllocateBindPoint(occupied, is_auto, requested, out_register_index, error)) {
    return false;
  }

  step::AddResourceStep::InputDecl resolved_decl = decl;
  resolved_decl.register_index = out_register_index;
  instructions.insert(instructions.begin() + static_cast<ptrdiff_t>(insert_index),
                      BuildInputDeclaration(resolved_decl));
  return true;
}

auto ShaderProgram::AddOutputDeclaration(const dxp::sm5::step::AddResourceStep::OutputDecl& decl, uint32_t& out_register_index, std::string& error) -> bool {
  std::unordered_set<uint32_t> occupied;
  uint32_t insert_index = 0;
  for (uint32_t i = 0; i < instructions.size(); ++i) {
    const auto opcode = instructions[i].opcode;
    if ((opcode == Opcode::DclOutput || opcode == Opcode::DclOutputSiv || opcode == Opcode::DclOutputSgv) && !instructions[i].operands.empty() && !instructions[i].operands.front().index_entries.empty()) {
      occupied.insert(*instructions[i].operands.front().index_entries[0].immediate_lo);
      insert_index = i + 1;
    }
  }

  const bool is_auto = !decl.register_index.has_value();
  const uint32_t requested = is_auto ? 0U : *decl.register_index;
  if (!AllocateBindPoint(occupied, is_auto, requested, out_register_index, error)) {
    return false;
  }

  step::AddResourceStep::OutputDecl resolved_decl = decl;
  resolved_decl.register_index = out_register_index;
  instructions.insert(instructions.begin() + static_cast<ptrdiff_t>(insert_index),
                      BuildOutputDeclaration(resolved_decl));
  return true;
}

auto ShaderProgram::AddTextureDeclaration(const dxp::sm5::step::AddResourceStep::TextureDecl& decl, uint32_t& out_register_index, std::string& error) -> bool {
  std::unordered_set<uint32_t> occupied;
  for (const auto& instruction : instructions) {
    const auto opcode = instruction.opcode;
    if ((opcode == Opcode::DclResource || opcode == Opcode::DclResourceRaw || opcode == Opcode::DclResourceStructured) && !instruction.operands.empty() && !instruction.operands.front().index_entries.empty()) {
      occupied.insert(*instruction.operands.front().index_entries[0].immediate_lo);
    }
  }

  const bool is_auto = !decl.register_index.has_value();
  const uint32_t requested = is_auto ? 0U : *decl.register_index;
  if (!AllocateBindPoint(occupied, is_auto, requested, out_register_index, error)) {
    return false;
  }

  step::AddResourceStep::TextureDecl resolved_decl = decl;
  resolved_decl.register_index = out_register_index;
  const uint32_t insert_index = FindInsertAfterLastDeclaration(static_cast<Opcode>(D3D10_SB_OPCODE_DCL_RESOURCE));
  instructions.insert(instructions.begin() + static_cast<ptrdiff_t>(insert_index),
                      BuildTextureDeclaration(resolved_decl));
  return true;
}

auto ShaderProgram::AddRawResourceDeclaration(const dxp::sm5::step::AddResourceStep::RawResourceDecl& decl, uint32_t& out_register_index,
                                              std::string& error) -> bool {
  std::unordered_set<uint32_t> occupied;
  for (const auto& instruction : instructions) {
    const auto opcode = instruction.opcode;
    if ((opcode == Opcode::DclResource || opcode == Opcode::DclResourceRaw || opcode == Opcode::DclResourceStructured) && !instruction.operands.empty() && !instruction.operands.front().index_entries.empty()) {
      occupied.insert(*instruction.operands.front().index_entries.front().immediate_lo);
    }
  }

  const bool is_auto = !decl.register_index.has_value();
  const uint32_t requested = is_auto ? 0U : *decl.register_index;
  if (!AllocateBindPoint(occupied, is_auto, requested, out_register_index, error)) {
    return false;
  }

  step::AddResourceStep::RawResourceDecl resolved_decl = decl;
  resolved_decl.register_index = out_register_index;
  const uint32_t insert_index = FindInsertAfterLastDeclaration(static_cast<Opcode>(D3D11_SB_OPCODE_DCL_RESOURCE_RAW));
  instructions.insert(instructions.begin() + static_cast<ptrdiff_t>(insert_index),
                      BuildRawResourceDeclaration(resolved_decl));
  return true;
}

auto ShaderProgram::AddStructuredResourceDeclaration(const dxp::sm5::step::AddResourceStep::StructuredResourceDecl& decl, uint32_t& out_register_index,
                                                     std::string& error) -> bool {
  std::unordered_set<uint32_t> occupied;
  for (const auto& instruction : instructions) {
    const auto opcode = instruction.opcode;
    if ((opcode == Opcode::DclResource || opcode == Opcode::DclResourceRaw || opcode == Opcode::DclResourceStructured) && !instruction.operands.empty() && !instruction.operands.front().index_entries.empty()) {
      occupied.insert(*instruction.operands.front().index_entries.front().immediate_lo);
    }
  }

  const bool is_auto = !decl.register_index.has_value();
  const uint32_t requested = is_auto ? 0U : *decl.register_index;
  if (!AllocateBindPoint(occupied, is_auto, requested, out_register_index, error)) {
    return false;
  }

  step::AddResourceStep::StructuredResourceDecl resolved_decl = decl;
  resolved_decl.register_index = out_register_index;
  const uint32_t insert_index = FindInsertAfterLastDeclaration(static_cast<Opcode>(D3D11_SB_OPCODE_DCL_RESOURCE_STRUCTURED));
  instructions.insert(instructions.begin() + static_cast<ptrdiff_t>(insert_index),
                      BuildStructuredResourceDeclaration(resolved_decl));
  return true;
}

auto ShaderProgram::AddCBufferDeclaration(const dxp::sm5::step::AddResourceStep::CBufferDecl& decl, uint32_t& out_register_index, std::string& error) -> bool {
  std::unordered_set<uint32_t> occupied;
  for (const auto& cbuffer : cbuffers) {
    occupied.insert(cbuffer.register_bind_point);
  }

  const bool is_auto = !decl.register_index.has_value();
  const uint32_t requested = is_auto ? 0U : *decl.register_index;
  if (!AllocateBindPoint(occupied, is_auto, requested, out_register_index, error)) {
    return false;
  }

  step::AddResourceStep::CBufferDecl resolved_decl = decl;
  resolved_decl.register_index = out_register_index;
  const uint32_t insert_index = FindInsertAfterLastDeclaration(static_cast<Opcode>(D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER));
  instructions.insert(instructions.begin() + static_cast<ptrdiff_t>(insert_index),
                      BuildConstantBufferDeclaration(resolved_decl));
  return true;
}

auto ShaderProgram::AddSamplerDeclaration(const dxp::sm5::step::AddResourceStep::SamplerDecl& decl, uint32_t& out_register_index, std::string& error) -> bool {
  std::unordered_set<uint32_t> occupied;
  for (const auto& instruction : instructions) {
    const auto opcode = instruction.opcode;
    if (opcode == Opcode::DclSampler && !instruction.operands.empty() && !instruction.operands.front().index_entries.empty()) {
      occupied.insert(*instruction.operands.front().index_entries.front().immediate_lo);
    }
  }

  const bool is_auto = !decl.register_index.has_value();
  const uint32_t requested = is_auto ? 0U : *decl.register_index;
  if (!AllocateBindPoint(occupied, is_auto, requested, out_register_index, error)) {
    return false;
  }

  step::AddResourceStep::SamplerDecl resolved_decl = decl;
  resolved_decl.register_index = out_register_index;
  const uint32_t insert_index = FindInsertAfterLastDeclaration(static_cast<Opcode>(D3D10_SB_OPCODE_DCL_SAMPLER));
  instructions.insert(instructions.begin() + static_cast<ptrdiff_t>(insert_index),
                      BuildSamplerDeclaration(resolved_decl));
  return true;
}

auto ShaderProgram::AddUavDeclaration(const dxp::sm5::step::AddResourceStep::UavDecl& decl, uint32_t& out_register_index, std::string& error) -> bool {
  std::unordered_set<uint32_t> occupied;
  for (const auto& instruction : instructions) {
    const auto opcode = instruction.opcode;
    if ((opcode == Opcode::DclUnorderedAccessViewTyped || opcode == Opcode::DclUnorderedAccessViewRaw || opcode == Opcode::DclUnorderedAccessViewStructured) && !instruction.operands.empty() && !instruction.operands.front().index_entries.empty()) {
      occupied.insert(*instruction.operands.front().index_entries.front().immediate_lo);
    }
  }

  const bool is_auto = !decl.register_index.has_value();
  const uint32_t requested = is_auto ? 0U : *decl.register_index;
  if (!AllocateBindPoint(occupied, is_auto, requested, out_register_index, error)) {
    return false;
  }

  step::AddResourceStep::UavDecl resolved_decl = decl;
  resolved_decl.register_index = out_register_index;
  const uint32_t insert_index = FindInsertAfterLastDeclaration(static_cast<Opcode>(D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED));
  instructions.insert(instructions.begin() + static_cast<ptrdiff_t>(insert_index), BuildUavDeclaration(resolved_decl));
  return true;
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

  // Preserve the parsed dimension (the original's token0 dimension bits) and
  // the parsed return-type token; the builder populates both for new decls.
  const uint32_t dimension =
      instruction.controls.resource_dimension != 0 ? instruction.controls.resource_dimension : static_cast<uint32_t>(D3D10_SB_RESOURCE_DIMENSION_TEXTURE2D);
  const uint32_t return_type_token = instruction.controls.resource_return_type != 0
                                         ? instruction.controls.resource_return_type
                                         : EncodeFloatResourceReturnTypeToken();

  const auto resource_operand =
      EncodeDeclarationOperand(OperandType::Resource, {*instruction.operands.front().index_entries[0].immediate_lo});
  const uint32_t kLength = 1U + static_cast<uint32_t>(resource_operand.size()) + 1U;

  std::vector<uint32_t> encoded;
  encoded.reserve(kLength);
  encoded.push_back(ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_DCL_RESOURCE) | ENCODE_D3D10_SB_RESOURCE_DIMENSION(static_cast<D3D10_SB_RESOURCE_DIMENSION>(dimension)) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(kLength));
  encoded.insert(encoded.end(), resource_operand.begin(), resource_operand.end());
  encoded.push_back(return_type_token);
  return encoded;
}

auto EncodeConstantBufferDeclaration(const Instruction& instruction) -> std::vector<uint32_t> {
  if (instruction.operands.empty() || instruction.operands.front().index_entries.size() < 2) {
    return {};
  }

  const uint32_t access_pattern = D3D10_SB_CONSTANT_BUFFER_IMMEDIATE_INDEXED;

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

auto EncodeInstructionToken0(const Instruction& instruction, uint32_t total_dwords) -> uint32_t {
  uint32_t token0 = 0;
  const auto opcode = static_cast<uint32_t>(instruction.opcode);

  token0 |= ENCODE_D3D10_SB_OPCODE_TYPE(opcode);

  token0 |= ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(total_dwords);

  if (!instruction.controls.extended_op_codes.empty()) {
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
    token0 |= ENCODE_D3D10_SB_SAMPLER_MODE(static_cast<D3D10_SB_SAMPLER_MODE>(instruction.sampler_mode));
  }

  if (opcode == D3D10_SB_OPCODE_DCL_GLOBAL_FLAGS && instruction.controls.sync_flags != 0) {
    token0 |= ENCODE_D3D10_SB_GLOBAL_FLAGS(instruction.controls.sync_flags);
  }

  if (opcode == D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER && instruction.controls.access_pattern != 0) {
    token0 |= ENCODE_D3D10_SB_D3D10_SB_CONSTANT_BUFFER_ACCESS_PATTERN(instruction.controls.access_pattern);
  }

  if (opcode == D3D10_SB_OPCODE_DCL_RESOURCE) {
    if (instruction.controls.resource_dimension != 0) {
      token0 |= ENCODE_D3D10_SB_RESOURCE_DIMENSION(static_cast<D3D10_SB_RESOURCE_DIMENSION>(instruction.controls.resource_dimension));
    }
    if (instruction.controls.resource_return_type != 0) {
      token0 |= instruction.controls.resource_return_type;
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
    encoded.push_back(ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_CUSTOMDATA) | ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(static_cast<uint32_t>(1 + custom_data.size())));
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
    const uint32_t token0 = EncodeInstructionToken0(*this, total_length);
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

  uint32_t total_length = 1 + static_cast<uint32_t>(controls.extended_op_codes.size());
  for (const auto& operand : operands) {
    total_length += static_cast<uint32_t>(operand.Encode().size());
  }

  const uint32_t token0 = EncodeInstructionToken0(*this, total_length);

  std::vector<uint32_t> encoded;
  encoded.push_back(token0);

  // Extended opcode tokens sit between the opcode token and the operands
  // (e.g. sample controls); re-emit the raw tokens parsed from the source.
  for (const auto& ext : controls.extended_op_codes) {
    encoded.push_back(ext.value);
  }

  for (const auto& operand : operands) {
    auto operand_tokens = operand.Encode();
    encoded.insert(encoded.end(), operand_tokens.begin(), operand_tokens.end());
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
}  // namespace dxp::sm5
