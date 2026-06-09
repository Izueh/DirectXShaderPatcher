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

}

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

  const char *validRecipeText = R"YAML(version: 1
steps:
  - name: replace_window_mul
    rules:
      - name: inline_rule_1
        match:
          opcode: mul
          capture: target_mul
          operands:
            - capture: dst
            - capture: src
        emit:
          - opcode: mov
            operands:
              - capture: dst
              - capture: src
)YAML";

  dxp::sm5::RecipeParseResult validParseResult;
  if (!dxp::sm5::ParseRecipeText(validRecipeText, validParseResult,
                                 "inline-sm5-replace-window-test")) {
    std::cerr << "Failed to parse inline SM5 replace recipe: "
              << validParseResult.Error << "\n";
    return 1;
  }

  const auto validPatchResult =
      dxp::sm5::PatchContainer(inputBytes, validParseResult.Recipe);
  if (!validPatchResult.Success) {
    std::cerr << "Failed to patch SM5 shader with replace window recipe: "
              << validPatchResult.Error << "\n";
    return 1;
  }

  if (validPatchResult.Report.Steps.size() != 1) {
    std::cerr << "Expected exactly one step report for replace rewrite.\n";
    return 1;
  }

  const auto &stepReport = validPatchResult.Report.Steps.front();
  if (stepReport.Rules.size() != 1) {
    std::cerr << "Expected exactly one rule report for replace rewrite.\n";
    return 1;
  }

  const auto &ruleReport = stepReport.Rules.front();
  if (ruleReport.MatchCount == 0 || ruleReport.AppliedCount == 0 ||
      !ruleReport.Changed) {
    std::cerr << "Expected replace rewrite rule report to record an applied "
                 "mutating match.\n";
    return 1;
  }

  dxp::sm5::ProgramInspection patchedProgram;
  if (!dxp::sm5::InspectProgram(validPatchResult.OutputBytes, patchedProgram,
                                &inspectError)) {
    std::cerr << "Failed to inspect patched SM5 program: " << inspectError
              << "\n";
    return 1;
  }

  const auto &patchedInstruction =
      patchedProgram.Instructions[static_cast<size_t>(targetInstructionIndex)];
  if (patchedInstruction.Opcode != D3D10_SB_OPCODE_MOV) {
    std::cerr
        << "Expected the captured target MUL instruction to become MOV.\n";
    return 1;
  }

  if (patchedInstruction.Operands.size() != 2) {
    std::cerr
        << "Expected emitted MOV instruction to have exactly two operands.\n";
    return 1;
  }

  if (!OperandsEqual(patchedInstruction.Operands[0],
                     originalInstruction.Operands[0])) {
    std::cerr << "Expected replace MOV destination operand to "
                 "preserve the captured destination.\n";
    return 1;
  }

  if (!OperandsEqual(patchedInstruction.Operands[1],
                     originalInstruction.Operands[1])) {
    std::cerr << "Expected replace MOV source operand to preserve the "
                 "captured source.\n";
    return 1;
  }

  const char *invalidReplaceFieldRecipeText = R"YAML(version: 1
steps:
  - name: invalid_replace_field
    rules:
      - name: inline_rule_2
        match:
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

  dxp::sm5::RecipeParseResult invalidReplaceFieldParseResult;
  if (dxp::sm5::ParseRecipeText(invalidReplaceFieldRecipeText,
                                invalidReplaceFieldParseResult,
                                "inline-sm5-invalid-replace-field-test")) {
    std::cerr << "Expected parsing to reject the removed SM5 replace field.\n";
    return 1;
  }

  const char *invalidBeforeWithoutIndexRecipeText = R"YAML(version: 1
steps:
  - name: invalid_before_without_index
    rules:
      - name: inline_rule_3
        match:
          opcode: mul
          capture: target_mul
          rewrite_mode: before
          operands:
            - capture: dst
            - capture: src
        emit:
          - opcode: mov
            operands:
              - capture: dst
              - capture: src
)YAML";

  dxp::sm5::RecipeParseResult invalidBeforeWithoutIndexParseResult;
  if (dxp::sm5::ParseRecipeText(invalidBeforeWithoutIndexRecipeText,
                                invalidBeforeWithoutIndexParseResult,
                                "inline-sm5-invalid-before-without-index-test")) {
    std::cerr << "Expected parsing to reject before rewrite without "
                 "insert_relative_index.\n";
    return 1;
  }

  std::cout << "SM5 Replace rewrote the full matched window and parser "
               "rejected removed replace field plus missing before index.\n";
  return 0;
}