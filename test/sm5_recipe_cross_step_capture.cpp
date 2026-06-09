#include "TestSupport.h"
#include "dxp/sm5/Patch.h"
#include "dxp/sm5/RecipeParse.h"

#include <iostream>
#include <vector>

namespace {

static int FindFirstMul(const dxp::sm5::ProgramInspection &program) {
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
    std::cerr << "Usage: sm5_recipe_cross_step_capture <input.ps_5_0.cso>\n";
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

  const int targetInstructionIndex = FindFirstMul(inputProgram);
  if (targetInstructionIndex < 0) {
    std::cerr << "Failed to locate target MUL instruction in input program.\n";
    return 1;
  }

  const auto &originalMul =
      inputProgram.Instructions[static_cast<size_t>(targetInstructionIndex)];
  const auto originalDst = originalMul.Operands[0];
  const auto originalSrc = originalMul.Operands[1];





  const char *recipeText = R"YAML(version: 1
steps:
  - name: capture_step
    rules:
      - name: capture_rule
        match:
          opcode: mul
          rewrite_mode: none
          operands:
            - capture: captured_dst
            - capture: captured_src
  - name: emit_step
    rules:
      - name: emit_rule
        match:
          opcode: mul
          operands:
            - match_capture: captured_dst
            - match_capture: captured_src
        emit:
          - opcode: mov
            operands:
              - capture: captured_dst
              - capture: captured_src
)YAML";

  dxp::sm5::RecipeParseResult parseResult;
  if (!dxp::sm5::ParseRecipeText(recipeText, parseResult,
                                 "inline-sm5-cross-step-capture-test")) {
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


  if (patchResult.Report.Steps.size() != 2) {
    std::cerr << "Expected two steps in recipe report.\n";
    return 1;
  }

  const auto &captureStepReport = patchResult.Report.Steps[0];
  if (!captureStepReport.Success) {
    std::cerr << "Expected capture_step to succeed.\n";
    return 1;
  }
  if (captureStepReport.Rules.size() != 1 ||
      captureStepReport.Rules.front().MatchCount == 0) {
    std::cerr << "Expected capture_step rule to match at least one instruction.\n";
    return 1;
  }

  const auto &emitStepReport = patchResult.Report.Steps[1];
  if (!emitStepReport.Success) {
    std::cerr << "Expected emit_step to succeed.\n";
    return 1;
  }
  if (emitStepReport.Rules.size() != 1 ||
      emitStepReport.Rules.front().AppliedCount == 0) {
    std::cerr << "Expected emit_step rule to apply at least one rewrite.\n";
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
    std::cerr << "Expected target instruction to be rewritten from MUL to MOV.\n";
    return 1;
  }

  if (patchedInstruction.Operands.size() != 2) {
    std::cerr
        << "Expected emitted MOV instruction to have exactly two operands.\n";
    return 1;
  }



  if (patchedInstruction.Operands[0] != originalDst) {
    auto &p = patchedInstruction.Operands[0];
    auto &o = originalDst;
    std::cerr << "Cross-step capture mismatch on dst operand:\n";
    std::cerr << "  Expected: Type=" << o.Type << " Indices=";
    for (auto v : o.Indices) std::cerr << v << " ";
    std::cerr << " ComponentMode=" << o.ComponentMode
              << " NumComponents=" << o.NumComponents;
    std::cerr << "\n";
    std::cerr << "  Actual:   Type=" << p.Type << " Indices=";
    for (auto v : p.Indices) std::cerr << v << " ";
    std::cerr << " ComponentMode=" << p.ComponentMode
              << " NumComponents=" << p.NumComponents;
    std::cerr << "\n";
    std::cerr << "Expected emitted MOV destination operand to preserve the "
                 "captured destination from step 1.\n";
    return 1;
  }

  if (patchedInstruction.Operands[1] != originalSrc) {
    std::cerr << "Expected emitted MOV source operand to preserve the "
                 "captured source from step 1.\n";
    return 1;
  }

  std::cout << "SM5 cross-step capture test passed: captured operands from "
               "step 1 successfully reused in step 2.\n";
  return 0;
}
