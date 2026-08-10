// Extended-opcode emit tests.
//
// Pins the canonical ResourceDim + ResourceReturnType extended-opcode pair that
// recipe-emitted resource-access instructions (ld, sample family, gather4) must
// carry. A bare `ld` (no extended tokens — the pre-fix dxp emit) is rejected by
// strict consumers: Flugan's disassembler fails with 80004005. Also pins the
// cbuffer element-index default (INDEX_2D operand) and the byte-identical
// re-serialization of patched output.
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#include "d3d11TokenizedProgramFormat.hpp"
#include "dxp/sm5/Recipe.hpp"
#include "src/dxp/sm5/ShaderProgram.hpp"
#include "tests/helper/TestHelper.hpp"

namespace {

using dxp::sm5::model::Instruction;
using dxp::sm5::model::NumComponents;
using dxp::sm5::model::Opcode;
using dxp::sm5::model::Operand;
using dxp::sm5::model::OperandType;
using dxp::sm5::model::SelectionMode;

/// @brief Builds a temp operand with a mask-mode component selection.
Operand MakeTemp(uint32_t mask, const char* value) {
  Operand op;
  op.type = OperandType::Temp;
  op.components.num_components = NumComponents::Four;
  op.components.selection_mode = SelectionMode::Mask;
  op.components.value = value;
  op.component_mode = ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE)
                      | ENCODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(mask);
  return op;
}

/// @brief Packed 4x4-bit float4 resource return type (the DCL_RESOURCE /
/// extended-token encoding).
uint32_t PackedFloat4ReturnType() {
  uint32_t packed = 0;
  for (uint32_t component = 0; component < 4; ++component) {
    packed |= ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, component);
  }
  return packed;
}

// Every SM5 resource-access opcode must encode the canonical ResourceDim +
// ResourceReturnType pair when the resource declaration was stamped.
bool TestCanonicalSynthesisForAllResourceOpcodes() {
  const Opcode kOpcodes[] = {
      Opcode::Ld,
      Opcode::Sample,
      Opcode::SampleB,
      Opcode::SampleC,
      Opcode::SampleCLz,
      Opcode::SampleD,
      Opcode::SampleL,
      Opcode::Gather4,
      Opcode::Gather4C,
  };
  for (const Opcode opcode : kOpcodes) {
    Instruction instr;
    instr.opcode = opcode;
    instr.controls.resource_dimension = D3D10_SB_RESOURCE_DIMENSION_TEXTURE2D;
    instr.controls.resource_return_type = PackedFloat4ReturnType();
    instr.operands = {MakeTemp(0xF, "xyzw"), MakeTemp(0xF, "xyzw")};

    const auto encoded = instr.Encode();
    if (!DECODE_IS_D3D10_SB_OPCODE_EXTENDED(encoded[0])) {
      std::cerr << "  extended bit not set on token0 for opcode " << static_cast<uint32_t>(opcode) << "\n";
      return false;
    }
    const uint32_t kDim = encoded[1];
    if ((kDim & 0x3F) != D3D11_SB_EXTENDED_OPCODE_RESOURCE_DIM
        || DECODE_D3D11_SB_EXTENDED_RESOURCE_DIMENSION(kDim) != D3D10_SB_RESOURCE_DIMENSION_TEXTURE2D
        || (kDim & D3D10_SB_OPCODE_EXTENDED_MASK) == 0) {
      std::cerr << "  non-canonical ResourceDim token for opcode " << static_cast<uint32_t>(opcode) << ": 0x"
                << std::hex << kDim << std::dec << "\n";
      return false;
    }
    const uint32_t kReturnType = encoded[2];
    if ((kReturnType & 0x3F) != D3D11_SB_EXTENDED_OPCODE_RESOURCE_RETURN_TYPE
        || (kReturnType & D3D10_SB_OPCODE_EXTENDED_MASK) != 0) {
      std::cerr << "  non-canonical ResourceReturnType token for opcode " << static_cast<uint32_t>(opcode) << ": 0x"
                << std::hex << kReturnType << std::dec << "\n";
      return false;
    }
    for (uint32_t component = 0; component < 4; ++component) {
      if (DECODE_D3D11_SB_EXTENDED_RESOURCE_RETURN_TYPE(kReturnType, component) != D3D10_SB_RETURN_TYPE_FLOAT) {
        std::cerr << "  non-float return type on opcode " << static_cast<uint32_t>(opcode) << "\n";
        return false;
      }
    }
  }
  std::cout << "  canonical synthesis: all resource-access opcodes encode ResourceDim + float4 ResourceReturnType\n";
  return true;
}

// A non-resource opcode must never gain extended tokens, even with a stamped
// resource declaration (the stamp belongs to resource-access instructions only).
bool TestNonResourceOpcodeGetsNoExtendedTokens() {
  Instruction instr;
  instr.opcode = Opcode::Mov;
  instr.controls.resource_dimension = D3D10_SB_RESOURCE_DIMENSION_TEXTURE2D;
  instr.controls.resource_return_type = PackedFloat4ReturnType();
  instr.operands = {MakeTemp(0xF, "xyzw"), MakeTemp(0xF, "xyzw")};

  const auto encoded = instr.Encode();
  if (DECODE_IS_D3D10_SB_OPCODE_EXTENDED(encoded[0])) {
    std::cerr << "  mov unexpectedly encoded an extended opcode token\n";
    return false;
  }
  std::cout << "  non-resource opcode: no extended tokens synthesized\n";
  return true;
}

// A cbuffer operand with the canonical register+element index pair (INDEX_2D)
// must serialize with index dimension 2 — the form Flugan/compilers expect.
bool TestCbufferElementIndexEncode() {
  Instruction instr;
  instr.opcode = Opcode::Mov;

  Operand cbuffer;
  cbuffer.type = OperandType::CBuffer;
  cbuffer.components.num_components = NumComponents::Four;
  cbuffer.components.selection_mode = SelectionMode::Select;
  cbuffer.components.value = "x";
  cbuffer.component_mode = ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE)
                           | ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECT_1(D3D10_SB_4_COMPONENT_X);

  Operand::Index register_index;
  register_index.representation = Operand::IndexRepresentation::Immediate32;
  register_index.immediate_lo = 5;
  Operand::Index element_index;
  element_index.representation = Operand::IndexRepresentation::Immediate32;
  element_index.immediate_lo = 0;
  cbuffer.index_entries = {register_index, element_index};

  instr.operands = {MakeTemp(0xF, "xyzw"), cbuffer};

  const auto encoded = instr.Encode();
  const uint32_t kOperandToken = encoded[2];  // token0, then temp operand, then cbuffer operand
  if (DECODE_D3D10_SB_OPERAND_INDEX_DIMENSION(kOperandToken) != 2) {
    std::cerr << "  cbuffer operand encoded with index dimension "
              << static_cast<uint32_t>(DECODE_D3D10_SB_OPERAND_INDEX_DIMENSION(kOperandToken)) << " (expected 2)\n";
    return false;
  }
  if (encoded[3] != 5 || encoded[4] != 0) {
    std::cerr << "  cbuffer indices are not (register, element) = (5, 0): got " << encoded[3] << ", " << encoded[4]
              << "\n";
    return false;
  }
  std::cout << "  cbuffer operand: register+element (INDEX_2D) serialized correctly\n";
  return true;
}

// FFXV-style integration: a recipe adds a texture + cbuffer and emits an `ld`
// (resource handle) and an `and` (cbuffer read). The patched output must
// (a) be deterministic, (b) re-serialize byte-identically, (c) carry the
// canonical extended pair on the emitted ld, (d) carry the 2-index cbuffer
// operand on the emitted and.
bool TestRecipeEmitRoundTrip(const std::filesystem::path& shader) {
  const char* yaml = R"YAML(version: 1
steps:
  - kind: add_resource
    name: add_test_resources
    textures:
      - handle: test_tex
        register_index: 15
        dimension: 3
    cbuffers:
      - handle: test_cb
        register_index: 14
        elements: 8
    temps: [noise_uv, sampled_noise]
  - kind: apply_rule
    name: emit_noise_ld
    match_mode: match_all
    rewrite_mode: replace
    rule:
      match:
        - opcode: mov
          operands:
            - {type: temp}
            - {type: temp}
      emit:
        - opcode: and
          operands:
            - {type: temp, handle: noise_uv, components: {selection_mode: mask, value: z}}
            - {type: immediate32, immediates_u32: [31]}
            - {type: constant_buffer, handle: test_cb, components: {selection_mode: select, value: x}}
        - opcode: ld
          operands:
            - {type: temp, handle: sampled_noise, components: {selection_mode: mask, value: x}}
            - {type: temp, handle: noise_uv, components: {selection_mode: swizzle, value: xyzw}}
            - {type: resource, handle: test_tex, components: {selection_mode: swizzle, value: xyzw}}
)YAML";

  auto parse_result = dxp::sm5::Recipe::ParseFromText(yaml, "inline-extended-opcode-emit-test");
  if (!parse_result) {
    std::cerr << "  recipe parse failed: " << parse_result.error() << "\n";
    return false;
  }

  std::vector<uint8_t> shader_bytes;
  if (!ReadFile(shader.string(), shader_bytes)) {
    std::cerr << "  failed to read shader: " << shader << "\n";
    return false;
  }

  const auto patch1 = parse_result.value().Execute(shader_bytes);
  if (!patch1) {
    std::cerr << "  execute (pass 1) failed: " << patch1.error() << "\n";
    return false;
  }
  const auto patch2 = parse_result.value().Execute(shader_bytes);
  if (!patch2) {
    std::cerr << "  execute (pass 2) failed: " << patch2.error() << "\n";
    return false;
  }
  if (patch1->output_bytes != patch2->output_bytes) {
    std::cerr << "  output is not deterministic across runs\n";
    return false;
  }

  auto program = dxp::sm5::ShaderProgram::FromBytes(patch1->output_bytes);
  if (!program) {
    std::cerr << "  patched output failed to re-parse: " << program.error() << "\n";
    return false;
  }
  auto serialized = program->Serialize();
  if (!serialized) {
    std::cerr << "  patched output failed to re-serialize: " << serialized.error() << "\n";
    return false;
  }
  if (*serialized != patch1->output_bytes) {
    std::cerr << "  patched output is not byte-identical after parse/serialize (" << patch1->output_bytes.size()
              << " vs " << serialized->size() << " bytes)\n";
    return false;
  }

  bool saw_emitted_ld = false;
  bool saw_emitted_and = false;
  for (const auto& instr : program->instructions) {
    if (instr.opcode == Opcode::Ld && instr.controls.extended_op_codes.size() == 2
        && !instr.operands.empty() && instr.operands.back().type == OperandType::Resource
        && !instr.operands.back().index_entries.empty()
        && instr.operands.back().index_entries.front().immediate_lo == 15) {
      const uint32_t kDim = instr.controls.extended_op_codes[0].value;
      const uint32_t kReturnType = instr.controls.extended_op_codes[1].value;
      if ((kDim & 0x3F) != D3D11_SB_EXTENDED_OPCODE_RESOURCE_DIM
          || DECODE_D3D11_SB_EXTENDED_RESOURCE_DIMENSION(kDim) != D3D10_SB_RESOURCE_DIMENSION_TEXTURE2D
          || (kDim & D3D10_SB_OPCODE_EXTENDED_MASK) == 0
          || (kReturnType & 0x3F) != D3D11_SB_EXTENDED_OPCODE_RESOURCE_RETURN_TYPE
          || (kReturnType & D3D10_SB_OPCODE_EXTENDED_MASK) != 0
          || DECODE_D3D11_SB_EXTENDED_RESOURCE_RETURN_TYPE(kReturnType, 0) != D3D10_SB_RETURN_TYPE_FLOAT) {
        std::cerr << "  emitted ld carries a non-canonical extended pair: 0x" << std::hex << kDim << " 0x"
                  << kReturnType << std::dec << "\n";
        return false;
      }
      saw_emitted_ld = true;
    }
    if (instr.opcode == Opcode::And && instr.operands.size() >= 3
        && instr.operands[2].type == OperandType::CBuffer
        && !instr.operands[2].index_entries.empty()
        && instr.operands[2].index_entries[0].immediate_lo == 14) {
      if (instr.operands[2].index_entries.size() != 2
          || instr.operands[2].index_entries[0].immediate_lo != 14
          || instr.operands[2].index_entries[1].immediate_lo != 0) {
        std::cerr << "  emitted cbuffer operand lacks the register+element (INDEX_2D) form\n";
        return false;
      }
      saw_emitted_and = true;
    }
  }
  if (!saw_emitted_ld) {
    std::cerr << "  emitted ld (t15) not found in patched output\n";
    return false;
  }
  if (!saw_emitted_and) {
    std::cerr << "  emitted and (cb14) not found in patched output\n";
    return false;
  }

  std::cout << "  recipe emit round-trip: deterministic, byte-identical, canonical ld pair, INDEX_2D cbuffer\n";
  return true;
}

}  // namespace

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 2) {
    std::cerr << "Usage: sm5_extended_opcode_emit_test <input.ps_5_0.cso>\n";
    return 1;
  }

  bool ok = true;
  ok &= TestCanonicalSynthesisForAllResourceOpcodes();
  ok &= TestNonResourceOpcodeGetsNoExtendedTokens();
  ok &= TestCbufferElementIndexEncode();
  ok &= TestRecipeEmitRoundTrip(args[1]);

  if (ok) {
    std::cout << "SM5 extended-opcode emit tests passed.\n";
  } else {
    std::cerr << "SM5 extended-opcode emit tests FAILED.\n";
  }
  std::cout.flush();
  return ok ? 0 : 1;
}
