#include "TestSupport.h"
#include "dxp/sm5/Container.h"
#include "dxp/sm5/Patch.h"
#include "dxp/sm5/Parse.h"
#include "dxp/sm5/RecipeParse.h"

#include <iostream>
#include <vector>

namespace {

static bool OperandsEqual(const dxp::sm5::Operand &lhs, const dxp::sm5::Operand &rhs) {
  if (lhs.Type != rhs.Type || lhs.NumComponents != rhs.NumComponents ||
      lhs.ComponentMode != rhs.ComponentMode || lhs.Modifier != rhs.Modifier ||
      lhs.Indices != rhs.Indices || lhs.ImmediateValues != rhs.ImmediateValues) {
    return false;
  }

  if (static_cast<bool>(lhs.RelativeOperand) != static_cast<bool>(rhs.RelativeOperand)) {
    return false;
  }

  if (lhs.RelativeOperand && rhs.RelativeOperand) {
    return OperandsEqual(*lhs.RelativeOperand, *rhs.RelativeOperand);
  }

  return true;
}

static int FindTargetInstruction(const dxp::sm5::Program &program) {
  for (size_t index = 0; index < program.Instructions.size(); ++index) {
    const auto &instruction = program.Instructions[index];
    if (static_cast<dxp::sm5::OpcodeType>(instruction.Opcode) == D3D10_SB_OPCODE_MUL &&
        instruction.Operands.size() >= 2) {
      return static_cast<int>(index);
    }
  }
  return -1;
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: sm5_recipe_replace_capture <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> inputBytes;
  if (!ReadFile(argv[1], inputBytes)) {
    std::cerr << "Failed to read input file: " << argv[1] << "\n";
    return 1;
  }

  dxp::sm5::Container inputContainer;
  if (!dxp::sm5::ParseDxbcContainer(inputBytes, inputContainer)) {
    std::cerr << "Failed to parse input DXBC container.\n";
    return 1;
  }

  dxp::sm5::Program inputProgram;
  if (!dxp::sm5::ParseShaderChunk(inputContainer, inputProgram)) {
    std::cerr << "Failed to parse input SM5 program.\n";
    return 1;
  }

  const int targetInstructionIndex = FindTargetInstruction(inputProgram);
  if (targetInstructionIndex < 0) {
    std::cerr << "Failed to locate the target MUL instruction in the input program.\n";
    return 1;
  }

  const dxp::sm5::Instruction originalInstruction =
      inputProgram.Instructions[static_cast<size_t>(targetInstructionIndex)];

  const char *validRecipeText = R"YAML(version: 1
predicates: []
steps:
  - name: replace_captured_mul
    rules:
      - match:
          opcode: mul
          capture: target_mul
          operands:
            - capture: dst
            - capture: src
        replace: target_mul
        emit:
          - opcode: mov
            operands:
              - capture: dst
              - capture: src
)YAML";

  dxp::sm5::RecipeParseResult validParseResult;
  if (!dxp::sm5::ParseRecipeText(validRecipeText,
                                 validParseResult,
                                 "inline-sm5-replace-capture-test")) {
    std::cerr << "Failed to parse inline SM5 replace recipe: "
              << validParseResult.Error << "\n";
    return 1;
  }

  const auto validPatchResult =
      dxp::sm5::PatchContainerInMemory(inputBytes, validParseResult.Recipe);
  if (!validPatchResult.Success) {
    std::cerr << "Failed to patch SM5 shader with replace capture recipe: "
              << validPatchResult.Error << "\n";
    return 1;
  }

  dxp::sm5::Container patchedContainer;
  if (!dxp::sm5::ParseDxbcContainer(validPatchResult.OutputBytes, patchedContainer)) {
    std::cerr << "Failed to parse patched DXBC container.\n";
    return 1;
  }

  dxp::sm5::Program patchedProgram;
  if (!dxp::sm5::ParseShaderChunk(patchedContainer, patchedProgram)) {
    std::cerr << "Failed to parse patched SM5 program.\n";
    return 1;
  }

  const auto &patchedInstruction =
      patchedProgram.Instructions[static_cast<size_t>(targetInstructionIndex)];
  if (static_cast<dxp::sm5::OpcodeType>(patchedInstruction.Opcode) != D3D10_SB_OPCODE_MOV) {
    std::cerr << "Expected the captured target MUL instruction to become MOV.\n";
    return 1;
  }

  if (patchedInstruction.Operands.size() != 2) {
    std::cerr << "Expected emitted MOV instruction to have exactly two operands.\n";
    return 1;
  }

  if (!OperandsEqual(patchedInstruction.Operands[0], originalInstruction.Operands[0])) {
    std::cerr << "Expected replace-targeted MOV destination operand to preserve the captured destination.\n";
    return 1;
  }

  if (!OperandsEqual(patchedInstruction.Operands[1], originalInstruction.Operands[1])) {
    std::cerr << "Expected replace-targeted MOV source operand to preserve the captured source.\n";
    return 1;
  }

  const char *invalidRecipeText = R"YAML(version: 1
predicates: []
steps:
  - name: invalid_replace_capture
    rules:
      - match:
          opcode: mul
          capture: target_mul
          operands:
            - capture: dst
            - capture: src
        replace: missing_capture
        emit:
          - opcode: mov
            operands:
              - capture: dst
              - capture: src
)YAML";

  dxp::sm5::RecipeParseResult invalidParseResult;
  if (!dxp::sm5::ParseRecipeText(invalidRecipeText,
                                 invalidParseResult,
                                 "inline-sm5-invalid-replace-capture-test")) {
    std::cerr << "Failed to parse inline SM5 invalid replace recipe: "
              << invalidParseResult.Error << "\n";
    return 1;
  }

  const auto invalidPatchResult =
      dxp::sm5::PatchContainerInMemory(inputBytes, invalidParseResult.Recipe);
  if (invalidPatchResult.Success) {
    std::cerr << "Expected patching to fail when replace references an unknown instruction capture.\n";
    return 1;
  }

  std::cout << "SM5 replace capture rewrote the named instruction target and rejected unknown replace captures.\n";
  return 0;
}