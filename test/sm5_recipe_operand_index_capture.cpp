#include "TestSupport.h"
#include "dxp/sm5/Patch.h"
#include "dxp/sm5/RecipeParse.h"

#include <filesystem>
#include <iostream>
#include <vector>

namespace {

static bool OperandsEqual(const dxp::sm5::ProgramOperand &lhs,
                          const dxp::sm5::ProgramOperand &rhs) {
  if (lhs.Type != rhs.Type || lhs.NumComponents != rhs.NumComponents ||
      lhs.ComponentMode != rhs.ComponentMode || lhs.Modifier != rhs.Modifier ||
      lhs.Indices != rhs.Indices || lhs.ImmediateValues != rhs.ImmediateValues) {
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

static int FindTargetMul(const dxp::sm5::ProgramInspection &program) {
  for (size_t index = 0; index < program.Instructions.size(); ++index) {
    const auto &instruction = program.Instructions[index];
    if (instruction.Opcode != D3D10_SB_OPCODE_MUL ||
        instruction.Operands.size() < 2) {
      continue;
    }

    const auto &dst = instruction.Operands[0];
    const auto &src = instruction.Operands[1];
    if (!dst.Indices.empty() && !src.Indices.empty()) {
      return static_cast<int>(index);
    }
  }

  return -1;
}

}

int main(int argc, char **argv) {
  if (argc != 2 && argc != 3) {
    std::cerr << "Usage: sm5_recipe_operand_index_capture <input.ps_5_0.cso> "
                 "[output.cso]\n";
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
    std::cerr << "Failed to locate a MUL instruction with indexed operands.\n";
    return 1;
  }

  const auto &originalInstruction =
      inputProgram.Instructions[static_cast<size_t>(targetIndex)];
  const auto originalDst = originalInstruction.Operands[0];
  const auto originalSrc = originalInstruction.Operands[1];

  const char *recipeText = R"YAML(version: 1
steps:
  - name: operand_index_capture_regression
    rules:
      - name: inline_rule_1
        match:
          opcode: mul
          operands:
            - type: temp
              capture: dst
              indices:
                - representation: immediate32
                  capture: dst_reg
            - capture: src
        emit:
          - opcode: mov
            operands:
              - capture: dst
              - capture: src
)YAML";
  dxp::sm5::RecipeParseResult parseResult;
  if (!dxp::sm5::ParseRecipeText(recipeText, parseResult,
                                 "inline-sm5-operand-index-capture-test")) {
    std::cerr << "Failed to parse inline operand/index capture recipe: "
              << parseResult.Error << "\n";
    return 1;
  }

  const auto patchResult = dxp::sm5::PatchContainer(inputBytes, parseResult.Recipe);
  if (!patchResult.Success) {
    std::cerr << "PatchContainer failed: " << patchResult.Error << "\n";
    return 1;
  }

  if (patchResult.Report.Steps.size() != 1 ||
      patchResult.Report.Steps.front().Rules.size() != 1 ||
      patchResult.Report.Steps.front().Rules.front().AppliedCount == 0) {
    std::cerr << "Expected one applied rule for operand/index capture regression.\n";
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
    std::cerr << "Expected target instruction to be rewritten to MOV.\n";
    return 1;
  }

  if (patchedInstruction.Operands.size() != 2) {
    std::cerr << "Expected rewritten MOV to have exactly two operands.\n";
    return 1;
  }

  const auto &patchedDst = patchedInstruction.Operands[0];
  const auto &patchedSrc = patchedInstruction.Operands[1];

  if (!OperandsEqual(patchedDst, originalDst)) {
    std::cerr << "Expected emitted destination operand to preserve captured "
                 "operand exactly.\n";
    return 1;
  }

  if (!OperandsEqual(patchedSrc, originalSrc)) {
    std::cerr << "Expected emitted source operand to preserve captured "
                 "operand exactly.\n";
    return 1;
  }

  if (argc == 3) {
    const std::filesystem::path outputPath(argv[2]);
    const std::filesystem::path parentPath = outputPath.parent_path();
    if (!parentPath.empty()) {
      std::error_code error;
      if (!std::filesystem::create_directories(parentPath, error) && error) {
        std::cerr << "Failed to create output directory: "
                  << parentPath.string() << "\n";
        return 1;
      }
    }

    if (!WriteFile(argv[2], patchResult.OutputBytes.data(),
                   patchResult.OutputBytes.size())) {
      std::cerr << "Failed to write patched output file: " << argv[2] << "\n";
      return 1;
    }
  }

  std::cout << "SM5 operand/index/capture regression test passed.\n";
  return 0;
}
