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

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: sm5_recipe_replay_object_from_syntax <input.ps_5_0.cso>\n";
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
  - name: replay_object_from_syntax
    rules:
      - match:
          opcode: mul
          capture: { from: inst }
          rewrite_mode: before
          operands:
            - type: temp
              capture: { from: dst }
              indices:
                - representation: immediate32
                  capture: { from: dst_reg }
            - type: temp
              capture: { from: src }
              indices:
                - representation: immediate32
                  capture: { from: src_reg }
        replace: { from: inst }
        emit:
          - opcode: mov
            operands:
              - capture: { from: dst }
              - type: temp
                indices:
                  - representation: immediate32
                    immediate_lo: { from: src_reg }
)YAML";

  dxp::sm5::RecipeParseResult parseResult;
  if (!dxp::sm5::ParseRecipeText(recipeText, parseResult,
                                 "inline-sm5-replay-object-from-syntax")) {
    std::cerr << "Failed to parse inline SM5 recipe: " << parseResult.Error
              << "\n";
    return 1;
  }

  const auto patchResult = dxp::sm5::PatchContainer(inputBytes, parseResult.Recipe);
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
    std::cerr << "Expected replay-object immediate_lo to preserve source "
                 "register indices.\n";
    return 1;
  }

  std::cout << "SM5 replay-object syntax validated at instruction index "
            << targetIndex << ".\n";
  return 0;
}