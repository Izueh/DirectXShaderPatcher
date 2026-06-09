#include "TestSupport.h"
#include "dxp/sm5/Patch.h"
#include "dxp/sm5/RecipeParse.h"

#include <iostream>
#include <vector>

namespace {

static int FindTargetMul(const dxp::sm5::ProgramInspection &program) {
  for (size_t index = 0; index < program.Instructions.size(); ++index) {
    const auto &instruction = program.Instructions[index];
    if (instruction.Opcode != D3D10_SB_OPCODE_MUL ||
        instruction.Operands.size() < 2) {
      continue;
    }

    const auto &src = instruction.Operands[1];
    if (src.Type == D3D10_SB_OPERAND_TYPE_TEMP && !src.Indices.empty()) {
      return static_cast<int>(index);
    }
  }

  return -1;
}

}

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr
        << "Usage: sm5_recipe_capture_reference_syntax <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> inputBytes;
  if (!ReadFile(argv[1], inputBytes)) {
    std::cerr << "Failed to read input file: " << argv[1] << "\n";
    return 1;
  }

  dxp::sm5::ProgramInspection inputProgram;
  std::string inspectError;
  if (!dxp::sm5::InspectProgram(inputBytes, inputProgram, &inspectError)) {
    std::cerr << "Failed to inspect input SM5 program: " << inspectError
              << "\n";
    return 1;
  }

  const int targetIndex = FindTargetMul(inputProgram);
  if (targetIndex < 0) {
    std::cerr << "Failed to find target MUL instruction.\n";
    return 1;
  }

  const auto &originalInstruction =
      inputProgram.Instructions[static_cast<size_t>(targetIndex)];
  const auto &originalSrc = originalInstruction.Operands[1];

  const char *recipeText = R"YAML(version: 1
steps:
  - name: capture_reference_syntax
    rules:
      - name: inline_rule_1
        match:
          opcode: mul
          capture: inst
          rewrite_mode: before
          insert_relative_index: 0
          operands:
            - type: temp
              capture: dst
              indices:
                - representation: immediate32
                  capture: dst_reg
            - type: temp
              capture: src
              indices:
                - representation: immediate32
                  capture: src_reg
        emit:
          - opcode: mov
            operands:
              - capture: dst
              - type: temp
                indices:
                  - representation: immediate32
                    match_capture: src_reg
)YAML";

  dxp::sm5::RecipeParseResult parseResult;
  if (!dxp::sm5::ParseRecipeText(recipeText, parseResult,
                                 "inline-sm5-capture-reference-syntax")) {
    std::cerr << "Failed to parse inline SM5 recipe: " << parseResult.Error
              << "\n";
    return 1;
  }

  const auto patchResult =
      dxp::sm5::PatchContainer(inputBytes, parseResult.Recipe);
  if (!patchResult.Success) {
    std::cerr << "Failed to patch SM5 shader: " << patchResult.Error << "\n";
    return 1;
  }

  dxp::sm5::ProgramInspection patchedProgram;
  if (!dxp::sm5::InspectProgram(patchResult.OutputBytes, patchedProgram,
                                &inspectError)) {
    std::cerr << "Failed to inspect patched SM5 program: " << inspectError
              << "\n";
    return 1;
  }

  const auto &patchedInstruction =
      patchedProgram.Instructions[static_cast<size_t>(targetIndex)];
  if (patchedInstruction.Opcode != D3D10_SB_OPCODE_MOV) {
    std::cerr << "Expected target MUL to be rewritten to MOV.\n";
    return 1;
  }

  if (patchedInstruction.Operands.size() < 2) {
    std::cerr << "Expected rewritten MOV to contain two operands.\n";
    return 1;
  }

  const auto &patchedSrc = patchedInstruction.Operands[1];
  if (patchedSrc.Indices != originalSrc.Indices) {
    std::cerr << "Expected capture-derived source register indices to "
                 "be preserved.\n";
    return 1;
  }

  std::cout << "SM5 capture-reference syntax validated at instruction index "
            << targetIndex << ".\n";
  return 0;
}