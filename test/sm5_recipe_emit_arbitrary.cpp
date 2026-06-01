#include "TestSupport.h"
#include "dxp/sm5/Patch.h"
#include "dxp/sm5/RecipeParse.h"

#include <iostream>
#include <vector>

namespace {

static bool OperandsEqual(const dxp::sm5::ProgramOperand &lhs,
                          const dxp::sm5::ProgramOperand &rhs) {
  if (lhs.Type != rhs.Type || lhs.NumComponents != rhs.NumComponents ||
      lhs.ComponentMode != rhs.ComponentMode || lhs.Modifier != rhs.Modifier ||
      lhs.Indices != rhs.Indices ||
      lhs.ImmediateValues != rhs.ImmediateValues) {
    return false;
  }

  if (lhs.RelativeOperands.size() != rhs.RelativeOperands.size()) {
    return false;
  }

  if (!lhs.RelativeOperands.empty() &&
      !OperandsEqual(lhs.RelativeOperands.front(),
                     rhs.RelativeOperands.front())) {
    return false;
  }

  return true;
}

static int FindTargetInstruction(const dxp::sm5::ProgramInspection &program) {
  for (size_t index = 0; index < program.Instructions.size(); ++index) {
    const auto &instruction = program.Instructions[index];
    if (instruction.Opcode == D3D10_SB_OPCODE_MUL &&
        instruction.Operands.size() >= 2) {
      return static_cast<int>(index);
    }
  }
  return -1;
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: sm5_recipe_emit_arbitrary <input.ps_5_0.cso>\n";
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

  const int targetInstructionIndex = FindTargetInstruction(inputProgram);
  if (targetInstructionIndex < 0) {
    std::cerr << "Failed to locate the target MUL instruction in the input "
                 "program.\n";
    return 1;
  }

  const dxp::sm5::ProgramInstruction originalInstruction =
      inputProgram.Instructions[static_cast<size_t>(targetInstructionIndex)];

  const char *recipeText = R"YAML(version: 1
steps:
  - name: replace_mul_with_mov
    rules:
      - name: inline_rule_1
        match:
          opcode: mul
          operands:
            - capture: dst
            - capture: src
        emit:
          - opcode: mov
            operands:
              - capture: dst
              - capture: src
)YAML";

  dxp::sm5::RecipeParseResult parseResult;
  if (!dxp::sm5::ParseRecipeText(recipeText, parseResult,
                                 "inline-sm5-arbitrary-test")) {
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
      patchedProgram.Instructions[static_cast<size_t>(targetInstructionIndex)];
  if (patchedInstruction.Opcode != D3D10_SB_OPCODE_MOV) {
    std::cerr << "Expected the matched MUL instruction to become MOV.\n";
    return 1;
  }

  if (patchedInstruction.Operands.size() != 2) {
    std::cerr
        << "Expected emitted MOV instruction to have exactly two operands.\n";
    return 1;
  }

  if (!OperandsEqual(patchedInstruction.Operands[0],
                     originalInstruction.Operands[0])) {
    std::cerr << "Expected emitted MOV destination operand to preserve the "
                 "captured destination.\n";
    return 1;
  }

  if (!OperandsEqual(patchedInstruction.Operands[1],
                     originalInstruction.Operands[1])) {
    std::cerr << "Expected emitted MOV source operand to preserve the captured "
                 "source operand.\n";
    return 1;
  }

  std::cout << "SM5 operand-aware emit recipe replaced a captured MUL with MOV "
               "at instruction index "
            << targetInstructionIndex << ".\n";
  return 0;
}
