#include "TestSupport.h"
#include "dxp/sm5/Patch.h"
#include "dxp/sm5/Recipe.h"

#include <iostream>
#include <vector>

namespace {

static int FindFrcMulSequence(const dxp::sm5::ProgramInspection &program) {
  for (size_t index = 0; index + 1 < program.Instructions.size(); ++index) {
    const auto &first = program.Instructions[index];
    const auto &second = program.Instructions[index + 1];
    if (first.Opcode == D3D10_SB_OPCODE_FRC &&
        second.Opcode == D3D10_SB_OPCODE_MUL && second.Operands.size() >= 2) {
      return static_cast<int>(index);
    }
  }
  return -1;
}

}

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: sm5_recipe_state_temp_emit <input.ps_5_0.cso>\n";
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

  const int targetInstructionIndex = FindFrcMulSequence(inputProgram);
  if (targetInstructionIndex < 0) {
    std::cerr << "Failed to locate target FRC/MUL sequence.\n";
    return 1;
  }

  const auto &matchedMulInstruction =
      inputProgram.Instructions[static_cast<size_t>(targetInstructionIndex) + 1];
  if (matchedMulInstruction.Operands.empty() ||
      matchedMulInstruction.Operands[0].Type != D3D10_SB_OPERAND_TYPE_TEMP ||
      matchedMulInstruction.Operands[0].Indices.empty()) {
    std::cerr << "Expected matched MUL destination to be temp with immediate index.\n";
    return 1;
  }
  const uint32_t expectedCapturedTemp = matchedMulInstruction.Operands[0].Indices[0];

    dxp::sm5::RecipeRule stateIndexedRule =
      dxp::sm5::RecipeRule{}
        .RewriteAs(dxp::sm5::RecipeRuleRewriteMode::ReplaceRange)
        .WithMatch(dxp::sm5::RecipeMatchPattern{}
               .AddInstruction(
                 dxp::sm5::RecipeInstructionPattern{}
                   .WithOpcode("frc")
                   .CaptureAs("ign_frc"))
               .AddInstruction(
                 dxp::sm5::RecipeInstructionPattern{}
                   .WithOpcode("mul")
                   .CaptureAs("ign_mul")
                   .AddOperand(dxp::sm5::RecipeOperandPattern{}
                           .CaptureAs("dst")
                           .AddIndexPattern(
                             dxp::sm5::RecipeOperandIndexPatternBuilder{}
                               .WithRepresentation(
                                 dxp::sm5::RecipeOperandIndexRepresentation::
                                   Immediate32)
                               .CaptureAs("shared_temp_r")
                               .Build()))
                   .AddOperand(dxp::sm5::RecipeOperandPattern{}
                           .CaptureAs("src"))))
        .AddEmit(dxp::sm5::RecipeInstructionTemplate{}
               .WithOpcode("mov")
               .AddOperand(dxp::sm5::RecipeOperandPattern{}
                 .WithType("temp")
                 .AddIndexPattern(
                   dxp::sm5::RecipeOperandIndexPatternBuilder{}
                     .WithRepresentation(
                       dxp::sm5::RecipeOperandIndexRepresentation::
                         Immediate32)
                     .WithMatchCapture("shared_temp_r")
                     .Build()))
               .AddOperand(
                 dxp::sm5::RecipeOperandPattern{}.CaptureAs("src")))
        .AddEmit(dxp::sm5::RecipeInstructionTemplate{}
               .WithOpcode("mov")
               .AddOperand(
                 dxp::sm5::RecipeOperandPattern{}.CaptureAs("dst"))
               .AddOperand(dxp::sm5::RecipeOperandPattern{}
                 .WithType("temp")
                 .AddIndexPattern(
                   dxp::sm5::RecipeOperandIndexPatternBuilder{}
                     .WithRepresentation(
                       dxp::sm5::RecipeOperandIndexRepresentation::
                         Immediate32)
                     .WithMatchCapture("shared_temp_r")
                     .Build())));

  dxp::sm5::Recipe recipe;
  recipe.AddStep(dxp::sm5::MakeRewriteRulesStep(
      "rewrite_with_index_capture", {stateIndexedRule},
      dxp::sm5::RecipeRuleApplicationMode::First, true));

  const auto patchResult = dxp::sm5::PatchContainer(inputBytes, recipe);
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

  if (patchedProgram.TempCount != inputProgram.TempCount) {
    std::cerr << "Expected index-capture flow to avoid reserving temp registers.\n";
    return 1;
  }

  int saveInstructionIndex = -1;
  for (size_t i = static_cast<size_t>(targetInstructionIndex);
       i < patchedProgram.Instructions.size(); ++i) {
    const auto &candidate = patchedProgram.Instructions[i];
    if (candidate.Opcode != D3D10_SB_OPCODE_MOV ||
        candidate.Operands.size() != 2) {
      continue;
    }
    if (candidate.Operands[0].Type != D3D10_SB_OPERAND_TYPE_TEMP ||
        candidate.Operands[0].Indices.empty() ||
        candidate.Operands[0].Indices[0] != expectedCapturedTemp) {
      continue;
    }
    saveInstructionIndex = static_cast<int>(i);
    break;
  }

  if (saveInstructionIndex < 0) {
    std::cerr << "Expected emitted MOV destination to target resolved state "
                 "index register.\n";
    return 1;
  }

  bool foundAnyStateIndexedTempUse = false;
  for (const auto &instruction : patchedProgram.Instructions) {
    for (const auto &operand : instruction.Operands) {
      if (operand.Type == D3D10_SB_OPERAND_TYPE_TEMP &&
          !operand.Indices.empty() &&
          operand.Indices[0] == expectedCapturedTemp) {
        foundAnyStateIndexedTempUse = true;
        break;
      }
    }
    if (foundAnyStateIndexedTempUse) {
      break;
    }
  }

  if (!foundAnyStateIndexedTempUse) {
    std::cerr << "Expected rewritten instructions to reference temp r"
              << expectedCapturedTemp
              << " resolved from captured index match_capture.\n";
    return 1;
  }

  std::cout << "SM5 index capture/match_capture emit resolved to r"
            << expectedCapturedTemp << " in declarative rewrite.\n";
  return 0;
}
