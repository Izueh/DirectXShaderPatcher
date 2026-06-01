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
    std::cerr << "Usage: sm5_recipe_before_after <input.ps_5_0.cso>\n";
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

  const auto &originalInstruction =
      inputProgram.Instructions[static_cast<size_t>(targetInstructionIndex)];
  const size_t initialInstructionCount = inputProgram.Instructions.size();

  const char *beforeRecipeText = R"YAML(version: 1
steps:
  - name: insert_before_mul
    rules:
      - name: inline_rule_1
        match:
          opcode: mul
          capture: target_mul
          operands:
            - capture: dst
            - capture: src
          rewrite_mode: before
          insert_relative_index: 0
        emit:
          - opcode: mov
            operands:
              - capture: dst
              - capture: src
)YAML";

  dxp::sm5::RecipeParseResult beforeParseResult;
  if (!dxp::sm5::ParseRecipeText(beforeRecipeText, beforeParseResult,
                                 "inline-sm5-before-test")) {
    std::cerr << "Failed to parse inline SM5 before recipe: "
              << beforeParseResult.Error << "\n";
    return 1;
  }

  const auto beforePatchResult =
      dxp::sm5::PatchContainer(inputBytes, beforeParseResult.Recipe);
  if (!beforePatchResult.Success) {
    std::cerr << "Failed to patch SM5 shader with before recipe: "
              << beforePatchResult.Error << "\n";
    return 1;
  }

  dxp::sm5::ProgramInspection beforeProgram;
  if (!dxp::sm5::InspectProgram(beforePatchResult.OutputBytes, beforeProgram,
                                &inspectError)) {
    std::cerr << "Failed to inspect SM5 program patched with before recipe: "
              << inspectError << "\n";
    return 1;
  }

  if (beforeProgram.Instructions.size() != initialInstructionCount + 1) {
    std::cerr
        << "Expected Before rewrite to increase instruction count by one.\n";
    return 1;
  }

  const auto &beforeInsertedInstruction =
      beforeProgram.Instructions[static_cast<size_t>(targetInstructionIndex)];
  const auto &beforeOriginalInstruction =
      beforeProgram
          .Instructions[static_cast<size_t>(targetInstructionIndex + 1)];
  if (beforeInsertedInstruction.Opcode != D3D10_SB_OPCODE_MOV) {
    std::cerr << "Expected Before rewrite to insert a MOV immediately before "
                 "the matched MUL.\n";
    return 1;
  }
  if (beforeOriginalInstruction.Opcode != D3D10_SB_OPCODE_MUL) {
    std::cerr << "Expected matched MUL to remain immediately after the "
                 "inserted Before instruction.\n";
    return 1;
  }
  if (beforeInsertedInstruction.Operands.size() != 2 ||
      !OperandsEqual(beforeInsertedInstruction.Operands[0],
                     originalInstruction.Operands[0]) ||
      !OperandsEqual(beforeInsertedInstruction.Operands[1],
                     originalInstruction.Operands[1])) {
    std::cerr << "Expected Before rewrite to preserve captured operands on the "
                 "inserted MOV.\n";
    return 1;
  }

  const char *afterRecipeText = R"YAML(version: 1
steps:
  - name: insert_after_mul
    rules:
      - name: inline_rule_2
        match:
          opcode: mul
          capture: target_mul
          operands:
            - capture: dst
            - capture: src
          rewrite_mode: after
          insert_relative_index: 0
        emit:
          - opcode: mov
            operands:
              - capture: dst
              - capture: src
)YAML";

  dxp::sm5::RecipeParseResult afterParseResult;
  if (!dxp::sm5::ParseRecipeText(afterRecipeText, afterParseResult,
                                 "inline-sm5-after-test")) {
    std::cerr << "Failed to parse inline SM5 after recipe: "
              << afterParseResult.Error << "\n";
    return 1;
  }

  const auto afterPatchResult =
      dxp::sm5::PatchContainer(inputBytes, afterParseResult.Recipe);
  if (!afterPatchResult.Success) {
    std::cerr << "Failed to patch SM5 shader with after recipe: "
              << afterPatchResult.Error << "\n";
    return 1;
  }

  dxp::sm5::ProgramInspection afterProgram;
  if (!dxp::sm5::InspectProgram(afterPatchResult.OutputBytes, afterProgram,
                                &inspectError)) {
    std::cerr << "Failed to inspect SM5 program patched with after recipe: "
              << inspectError << "\n";
    return 1;
  }

  if (afterProgram.Instructions.size() != initialInstructionCount + 1) {
    std::cerr
        << "Expected After rewrite to increase instruction count by one.\n";
    return 1;
  }

  const auto &afterOriginalInstruction =
      afterProgram.Instructions[static_cast<size_t>(targetInstructionIndex)];
  const auto &afterInsertedInstruction =
      afterProgram
          .Instructions[static_cast<size_t>(targetInstructionIndex + 1)];
  if (afterOriginalInstruction.Opcode != D3D10_SB_OPCODE_MUL) {
    std::cerr << "Expected matched MUL to remain at the anchor position for "
                 "After rewrite.\n";
    return 1;
  }
  if (afterInsertedInstruction.Opcode != D3D10_SB_OPCODE_MOV) {
    std::cerr << "Expected After rewrite to insert a MOV immediately after the "
                 "matched MUL.\n";
    return 1;
  }
  if (afterInsertedInstruction.Operands.size() != 2 ||
      !OperandsEqual(afterInsertedInstruction.Operands[0],
                     originalInstruction.Operands[0]) ||
      !OperandsEqual(afterInsertedInstruction.Operands[1],
                     originalInstruction.Operands[1])) {
    std::cerr << "Expected After rewrite to preserve captured operands on the "
                 "inserted MOV.\n";
    return 1;
  }

  const char *indexedAfterRecipeText = R"YAML(version: 1
steps:
  - name: after_indexed_mul
    rules:
      - name: inline_rule_3
        match:
          rewrite_mode: after
          insert_relative_index: 0
          sequence:
            - opcode: mul
              operands:
                - capture: dst
                - capture: src
        emit:
          - opcode: mov
            operands:
              - capture: dst
              - capture: src
)YAML";

  dxp::sm5::RecipeParseResult indexedAfterParseResult;
  if (!dxp::sm5::ParseRecipeText(indexedAfterRecipeText,
                                 indexedAfterParseResult,
                                 "inline-sm5-indexed-after-test")) {
    std::cerr << "Failed to parse inline SM5 indexed-after recipe: "
              << indexedAfterParseResult.Error << "\n";
    return 1;
  }

  const auto indexedAfterPatchResult =
      dxp::sm5::PatchContainer(inputBytes, indexedAfterParseResult.Recipe);
  if (!indexedAfterPatchResult.Success) {
    std::cerr << "Failed to patch SM5 shader with indexed-after recipe: "
              << indexedAfterPatchResult.Error << "\n";
    return 1;
  }

  dxp::sm5::ProgramInspection indexedAfterProgram;
  if (!dxp::sm5::InspectProgram(indexedAfterPatchResult.OutputBytes,
                                indexedAfterProgram, &inspectError)) {
    std::cerr
        << "Failed to inspect SM5 program patched with indexed-after recipe: "
        << inspectError << "\n";
    return 1;
  }

  if (indexedAfterProgram.Instructions.size() != initialInstructionCount + 1) {
    std::cerr << "Expected indexed after rewrite to increase instruction count "
                 "by one.\n";
    return 1;
  }

  const auto &indexedAfterOriginalInstruction =
      indexedAfterProgram.Instructions[static_cast<size_t>(targetInstructionIndex)];
  const auto &indexedAfterInsertedInstruction = indexedAfterProgram.Instructions[
      static_cast<size_t>(targetInstructionIndex + 1)];
  if (indexedAfterOriginalInstruction.Opcode != D3D10_SB_OPCODE_MUL) {
    std::cerr << "Expected indexed after rewrite to keep the matched MUL at "
                 "the anchor position.\n";
    return 1;
  }
  if (indexedAfterInsertedInstruction.Opcode != D3D10_SB_OPCODE_MOV) {
    std::cerr << "Expected indexed after rewrite to insert MOV after the "
                 "indexed matched instruction.\n";
    return 1;
  }
  if (indexedAfterInsertedInstruction.Operands.size() != 2 ||
      !OperandsEqual(indexedAfterInsertedInstruction.Operands[0],
                     originalInstruction.Operands[0]) ||
      !OperandsEqual(indexedAfterInsertedInstruction.Operands[1],
                     originalInstruction.Operands[1])) {
    std::cerr << "Expected indexed after rewrite to preserve captured operands "
                 "on the inserted MOV.\n";
    return 1;
  }

  std::cout << "SM5 before/after rewrite modes with explicit index anchors "
               "inserted MOV instructions around the matched MUL.\n";
  return 0;
}