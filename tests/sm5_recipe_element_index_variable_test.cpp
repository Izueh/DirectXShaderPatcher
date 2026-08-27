// SM5 element_index variable test.
//
// Verifies that element_index on a handle operand can accept:
// 1. A literal uint32_t value (existing behavior)
// 2. A variable name string resolved from the recipe's env at runtime
// 3. Missing variables default to 0 for CBuffer types
// 4. Non-CBuffer types do NOT get a default second index

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

// ─── Test 1: Literal element_index ────────────────────────────────────────────

bool TestLiteralElementIndex(const std::filesystem::path& shader) {
  const char* yaml = R"YAML(version: 1
steps:
  - kind: add_resource
    name: add_resources
    cbuffers:
      - handle: my_cbuffer
        register_index: 5
        elements: 8
    temps: [result]
  - kind: apply_rule
    name: emit_with_literal_element
    match_mode: match_all
    rewrite_mode: replace
    rule:
      match:
        - opcode: mov
          operands:
            - {type: temp}
            - {type: temp}
      emit:
        - opcode: mov
          operands:
            - {type: temp, handle: { name: result }, components: {selection_mode: mask, value: xyzw}}
            - {type: constant_buffer, handle: { name: my_cbuffer, element_index: 3 }, components: {selection_mode: select, value: x}}
)YAML";

  auto parse_result = dxp::sm5::Recipe::ParseFromText(yaml, "literal-element-index-test");
  if (!parse_result) {
    std::cerr << "  recipe parse failed: " << parse_result.error() << "\n";
    return false;
  }

  std::vector<uint8_t> shader_bytes;
  if (!ReadFile(shader.string(), shader_bytes)) {
    std::cerr << "  failed to read shader: " << shader << "\n";
    return false;
  }

  const auto patch_result = parse_result.value().Execute(shader_bytes);
  if (!patch_result) {
    std::cerr << "  execute failed: " << patch_result.error() << "\n";
    return false;
  }

  auto program = dxp::sm5::ShaderProgram::FromBytes(patch_result->output_bytes);
  if (!program) {
    std::cerr << "  patched output failed to re-parse: " << program.error() << "\n";
    return false;
  }

  // Find the emitted 'mov' instruction and verify the cbuffer operand has
  // element_index = 3 (second index entry).
  bool found_mov = false;
  for (const auto& instr : program->instructions) {
    if (instr.opcode == Opcode::Mov && instr.operands.size() >= 2
        && instr.operands[1].type == OperandType::CBuffer
        && instr.operands[1].index_entries.size() == 2) {
      auto reg0 = instr.operands[1].index_entries[0].immediate_lo.value_or(0);
      auto elem0 = instr.operands[1].index_entries[1].immediate_lo.value_or(0);
      if (reg0 != 5 || elem0 != 3) {
        std::cerr << "  literal element_index: expected (reg=5, elem=3), got ("
                  << reg0 << ", " << elem0 << ")\n";
        return false;
      }
      found_mov = true;
      break;
    }
  }

  if (!found_mov) {
    std::cerr << "  emitted 'mov' with literal element_index not found\n";
    return false;
  }

  std::cout << "  literal element_index: reg=5, elem=3 — correct\n";
  return true;
}

// ─── Test 2: Variable element_index ───────────────────────────────────────────

bool TestVariableElementIndex(const std::filesystem::path& shader) {
  const char* yaml = R"YAML(version: 1
steps:
  - kind: add_resource
    name: add_resources
    cbuffers:
      - handle: my_cbuffer
        register_index: 7
        elements: 8
    temps: [result]
  - kind: apply_rule
    name: emit_with_var_element
    match_mode: match_all
    rewrite_mode: replace
    rule:
      match:
        - opcode: mov
          operands:
            - {type: temp}
            - {type: temp}
      emit:
        - opcode: mov
          operands:
            - {type: temp, handle: { name: result }, components: {selection_mode: mask, value: xyzw}}
            - {type: constant_buffer, handle: { name: my_cbuffer, element_index: struct_offset }, components: {selection_mode: select, value: x}}
)YAML";

  auto parse_result = dxp::sm5::Recipe::ParseFromText(yaml, "variable-element-index-test");
  if (!parse_result) {
    std::cerr << "  recipe parse failed: " << parse_result.error() << "\n";
    return false;
  }

  std::vector<uint8_t> shader_bytes;
  if (!ReadFile(shader.string(), shader_bytes)) {
    std::cerr << "  failed to read shader: " << shader << "\n";
    return false;
  }

  // Set the environment variable that the recipe references.
  auto recipe = std::move(*parse_result);
  recipe.SetEnv("struct_offset", static_cast<uint32_t>(5));

  const auto patch_result = recipe.Execute(shader_bytes);
  if (!patch_result) {
    std::cerr << "  execute failed: " << patch_result.error() << "\n";
    return false;
  }

  auto program = dxp::sm5::ShaderProgram::FromBytes(patch_result->output_bytes);
  if (!program) {
    std::cerr << "  patched output failed to re-parse: " << program.error() << "\n";
    return false;
  }

  // Find the emitted 'mov' instruction and verify the cbuffer operand has
  // element_index = 5 (resolved from variable).
  bool found_mov = false;
  for (const auto& instr : program->instructions) {
    if (instr.opcode == Opcode::Mov && instr.operands.size() >= 2
        && instr.operands[1].type == OperandType::CBuffer
        && instr.operands[1].index_entries.size() == 2) {
      auto reg1 = instr.operands[1].index_entries[0].immediate_lo.value_or(0);
      auto elem1 = instr.operands[1].index_entries[1].immediate_lo.value_or(0);
      if (reg1 != 7 || elem1 != 5) {
        std::cerr << "  variable element_index: expected (reg=7, elem=5), got ("
                  << reg1 << ", " << elem1 << ")\n";
        return false;
      }
      found_mov = true;
      break;
    }
  }

  if (!found_mov) {
    std::cerr << "  emitted 'mov' with variable element_index not found\n";
    return false;
  }

  std::cout << "  variable element_index: reg=7, elem=5 (from env) — correct\n";
  return true;
}

// ─── Test 3: Missing variable defaults to 0 for CBuffer ──────────────────────

bool TestMissingVariableDefaultsToZero(const std::filesystem::path& shader) {
  const char* yaml = R"YAML(version: 1
steps:
  - kind: add_resource
    name: add_resources
    cbuffers:
      - handle: my_cbuffer
        register_index: 3
        elements: 8
    temps: [result]
  - kind: apply_rule
    name: emit_with_missing_var
    match_mode: match_all
    rewrite_mode: replace
    rule:
      match:
        - opcode: mov
          operands:
            - {type: temp}
            - {type: temp}
      emit:
        - opcode: mov
          operands:
            - {type: temp, handle: { name: result }, components: {selection_mode: mask, value: xyzw}}
            - {type: constant_buffer, handle: { name: my_cbuffer, element_index: nonexistent_var }, components: {selection_mode: select, value: x}}
)YAML";

  auto parse_result = dxp::sm5::Recipe::ParseFromText(yaml, "missing-var-test");
  if (!parse_result) {
    std::cerr << "  recipe parse failed: " << parse_result.error() << "\n";
    return false;
  }

  std::vector<uint8_t> shader_bytes;
  if (!ReadFile(shader.string(), shader_bytes)) {
    std::cerr << "  failed to read shader: " << shader << "\n";
    return false;
  }

  // Do NOT set "nonexistent_var" — it should default to 0.
  auto recipe = std::move(*parse_result);
  // No SetEnv call for nonexistent_var

  const auto patch_result = recipe.Execute(shader_bytes);
  if (!patch_result) {
    std::cerr << "  execute failed: " << patch_result.error() << "\n";
    return false;
  }

  auto program = dxp::sm5::ShaderProgram::FromBytes(patch_result->output_bytes);
  if (!program) {
    std::cerr << "  patched output failed to re-parse: " << program.error() << "\n";
    return false;
  }

  // Find the emitted 'mov' instruction and verify the cbuffer operand has
  // element_index = 0 (default for missing variable).
  bool found_mov = false;
  for (const auto& instr : program->instructions) {
    if (instr.opcode == Opcode::Mov && instr.operands.size() >= 2
        && instr.operands[1].type == OperandType::CBuffer
        && instr.operands[1].index_entries.size() == 2) {
      auto reg2 = instr.operands[1].index_entries[0].immediate_lo.value_or(0);
      auto elem2 = instr.operands[1].index_entries[1].immediate_lo.value_or(0);
      if (reg2 != 3 || elem2 != 0) {
        std::cerr << "  missing variable default: expected (reg=3, elem=0), got ("
                  << reg2 << ", " << elem2 << ")\n";
        return false;
      }
      found_mov = true;
      break;
    }
  }

  if (!found_mov) {
    std::cerr << "  emitted 'mov' with missing variable default not found\n";
    return false;
  }

  std::cout << "  missing variable defaults to 0 for CBuffer — correct\n";
  return true;
}

// ─── Test 4: Nullopt element_index on non-CBuffer does NOT add second index ───

bool TestNonCBufferNoDefaultElementIndex(const std::filesystem::path& shader) {
  const char* yaml = R"YAML(version: 1
steps:
  - kind: add_resource
    name: add_resources
    uavs:
      - handle: my_uav
        register_index: 4
        kind: typed
        dimension: texture2d
    temps: [result]
  - kind: apply_rule
    name: emit_uav_no_element
    match_mode: match_all
    rewrite_mode: replace
    rule:
      match:
        - opcode: mov
          operands:
            - {type: temp}
            - {type: temp}
      emit:
        - opcode: ld_uav_typed
          operands:
            - {type: temp, handle: { name: result }, components: {selection_mode: mask, value: xyzw}}
            - {type: temp, handle: { name: result }, components: {selection_mode: mask, value: xyzw}}
            - {type: unordered_access_view, handle: { name: my_uav }, components: {selection_mode: mask, value: xyzw}}
)YAML";

  auto parse_result = dxp::sm5::Recipe::ParseFromText(yaml, "non-cbuffer-no-element-test");
  if (!parse_result) {
    std::cerr << "  recipe parse failed: " << parse_result.error() << "\n";
    return false;
  }

  std::vector<uint8_t> shader_bytes;
  if (!ReadFile(shader.string(), shader_bytes)) {
    std::cerr << "  failed to read shader: " << shader << "\n";
    return false;
  }

  auto recipe = std::move(*parse_result);

  const auto patch_result = recipe.Execute(shader_bytes);
  if (!patch_result) {
    std::cerr << "  execute failed: " << patch_result.error() << "\n";
    return false;
  }

  auto program = dxp::sm5::ShaderProgram::FromBytes(patch_result->output_bytes);
  if (!program) {
    std::cerr << "  patched output failed to re-parse: " << program.error() << "\n";
    return false;
  }

  // Find the emitted 'ld_uav_typed' instruction and verify the UAV operand
  // has only ONE index entry (register only, no element).
  bool found_ld = false;
  for (const auto& instr : program->instructions) {
    if (instr.opcode == Opcode::LdUavTyped && instr.operands.size() >= 3
        && instr.operands[2].type == OperandType::UAV) {
      if (instr.operands[2].index_entries.size() != 1) {
        std::cerr << "  non-CBuffer UAV: expected 1 index entry (register only), got "
                  << instr.operands[2].index_entries.size() << "\n";
        return false;
      }
      auto reg3 = instr.operands[2].index_entries[0].immediate_lo.value_or(0);
      if (reg3 != 4) {
        std::cerr << "  non-CBuffer UAV: expected register=4, got " << reg3 << "\n";
        return false;
      }
      found_ld = true;
      break;
    }
  }

  if (!found_ld) {
    std::cerr << "  emitted 'ld_uav_typed' with UAV handle not found\n";
    return false;
  }

  std::cout << "  non-CBuffer UAV without element_index: single index entry — correct\n";
  return true;
}

}  // namespace

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 2) {
    std::cerr << "Usage: sm5_recipe_element_index_variable_test <input.ps_5_0.cso>\n";
    return 1;
  }

  bool ok = true;
  ok &= TestLiteralElementIndex(args[1]);
  ok &= TestVariableElementIndex(args[1]);
  ok &= TestMissingVariableDefaultsToZero(args[1]);
  ok &= TestNonCBufferNoDefaultElementIndex(args[1]);

  if (ok) {
    std::cout << "SM5 element_index variable tests passed.\n";
  } else {
    std::cerr << "SM5 element_index variable tests FAILED.\n";
  }
  std::cout.flush();
  return ok ? 0 : 1;
}
