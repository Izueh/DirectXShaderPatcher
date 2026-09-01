// Phase 1 of SM5 declaration cross-referencing: signature semantic parsing.
//
// Pins:
//   - SIV/SGV declarations decode their trailing NameToken into
//     OpcodeControls::semantic_name (e.g. dcl_input_ps_siv ..., position);
//   - non-SIV declarations leave semantic_name unset;
//   - parsed instructions round-trip byte-identically (NameToken is preserved
//     as the declaration's trailing operand, never re-encoded);
//   - recipe-synthesized SIV declarations (register operand only +
//     controls.semantic_name) encode a canonical NameToken.
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#include "dxp/sm5/Recipe.hpp"
#include "src/dxp/sm5/ShaderProgram.hpp"
#include "tests/helper/TestHelper.hpp"

namespace {

using dxp::sm5::model::Opcode;
using dxp::sm5::model::SignatureSemantic;

int g_failures = 0;

void Check(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << "\n";
    ++g_failures;
  }
}

/// @brief Parses a shader and returns the semantic_name of the first
/// declaration with the requested opcode, or unset if none carries one.
std::optional<SignatureSemantic> FindSemantic(const dxp::sm5::ShaderProgram& program, Opcode opcode) {
  for (const auto& instr : program.instructions) {
    if (instr.opcode == opcode && instr.controls.semantic_name.has_value()) {
      return instr.controls.semantic_name;
    }
  }
  return std::nullopt;
}

}  // namespace

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 2) {
    std::cerr << "Usage: sm5_signature_semantic_test <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> input_bytes;
  if (!ReadFile((RepoRootPath() / args[1]).string(), input_bytes)) {
    std::cerr << "Failed to read file: " << args[1] << "\n";
    return 1;
  }

  // --- 1. Parse: dcl_input_ps_siv ..., position decodes to Position ---
  auto program = dxp::sm5::ShaderProgram::FromBytes(input_bytes);
  if (!program) {
    std::cerr << "Failed to parse shader: " << program.error() << "\n";
    return 1;
  }

  const auto position_semantic = FindSemantic(*program, Opcode::DclInputPsSiv);
  Check(position_semantic.has_value(), "dcl_input_ps_siv should decode a semantic name");
  Check(position_semantic == SignatureSemantic::Position, "dcl_input_ps_siv semantic should be position");

  // Non-SIV declarations carry no semantic.
  bool any_plain_input = false;
  for (const auto& instr : program->instructions) {
    if (instr.opcode == Opcode::DclInputPs || instr.opcode == Opcode::DclInput || instr.opcode == Opcode::DclOutput) {
      Check(!instr.controls.semantic_name.has_value(), "non-SIV declaration must not carry semantic_name");
      any_plain_input = true;
    }
  }
  Check(any_plain_input, "corpus shader should contain plain dcl_input_ps/dcl_output declarations");

  // --- 2. Round-trip: re-serialization must be byte-identical ---
  const auto serialized = program->Serialize();
  if (!serialized) {
    std::cerr << "Failed to serialize: " << serialized.error() << "\n";
    return 1;
  }
  Check(serialized->size() == input_bytes.size(), "round-trip size must match input");
  Check(std::equal(serialized->begin(), serialized->end(), input_bytes.begin()),
        "round-trip bytes must be identical (NameToken preserved verbatim)");

  // --- 3. Synthesized SIV declaration encodes a canonical NameToken ---
  {
    dxp::sm5::model::Instruction instr;
    instr.opcode = Opcode::DclInputPsSiv;
    instr.controls.input_interpolation_mode = static_cast<uint32_t>(dxp::sm5::model::InterpolationMode::Linear);
    instr.controls.semantic_name = SignatureSemantic::Position;
    dxp::sm5::model::Operand operand;
    operand.type = dxp::sm5::model::OperandType::Input;
    operand.components.num_components = dxp::sm5::model::NumComponents::Four;
    operand.component_mode = ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) | D3D10_SB_OPERAND_4_COMPONENT_MASK_ALL;
    dxp::sm5::model::Operand::Index idx;
    idx.representation = dxp::sm5::model::Operand::IndexRepresentation::Immediate32;
    idx.immediate_lo = 7;
    operand.index_entries.push_back(std::move(idx));
    instr.operands.push_back(std::move(operand));

    const auto encoded = instr.Encode();
    // [0] opcode token, [1] operand token0, [2] operand index, [3] NameToken
    Check(encoded.size() == 4, "synthesized SIV dcl must encode 4 dwords");
    if (encoded.size() == 4) {
      const uint32_t length = (encoded[0] >> 24) & 0x7F;
      Check(length == 4, "instruction length must include the NameToken dword");
      Check((encoded[3] & 0xFFFF) == 1, "NameToken dword must encode D3D10_SB_NAME_POSITION (1)");
    }
  }

  if (g_failures == 0) {
    std::cout << "sm5_signature_semantic_test passed.\n";
    return 0;
  }
  std::cerr << g_failures << " check(s) failed.\n";
  return 1;
}
