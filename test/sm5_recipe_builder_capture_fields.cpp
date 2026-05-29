#include "TestSupport.h"
#include "dxp/sm5/Patch.h"
#include "dxp/sm5/Recipe.h"

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
    std::cerr
        << "Usage: sm5_recipe_builder_capture_fields <input.ps_5_0.cso>\n";
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
    std::cerr << "Failed to find a MUL instruction with temp source operand.\n";
    return 1;
  }

  const auto &originalInstruction =
      inputProgram.Instructions[static_cast<size_t>(targetIndex)];
  const auto &originalSrc = originalInstruction.Operands[1];

  dxp::sm5::Recipe recipe;
  dxp::sm5::RecipeRule rule;
  rule.WithMatch(
          dxp::sm5::RecipeMatchPattern{}
              .WithOpcode("mul")
              .AddOperand(dxp::sm5::RecipeOperandPatternBuilder{}
                              .CaptureAs("dst")
                              .Build())
              .AddOperand(dxp::sm5::RecipeOperandPatternBuilder{}
                              .CaptureAs("src")
                              .Build()))
      .AddEmit(dxp::sm5::RecipeInstructionTemplate{}
                   .WithOpcode("mov")
                   .AddOperand(dxp::sm5::RecipeOperandPatternBuilder{}
                                   .CaptureAs("dst")
                                   .Build())
                   .AddOperand(dxp::sm5::RecipeOperandPatternBuilder{}
                                   .WithType("temp")
                                   .WithModifier("neg")
                                   .ReplayIndicesFrom("src")
                                   .Build()));

  recipe.AddStep(dxp::sm5::MakeRewriteRulesStep(
      "builder_capture_fields", {std::move(rule)},
      dxp::sm5::RecipeRuleApplicationMode::First, true));

  const auto patchResult = dxp::sm5::PatchContainer(inputBytes, recipe);
  if (!patchResult.Success) {
    std::cerr << "Failed to patch SM5 shader with builder recipe: "
              << patchResult.Error << "\n";
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
    std::cerr << "Expected ReplayIndicesFrom builder path to preserve source "
                 "operand register indices.\n";
    return 1;
  }

  if (patchedSrc.Modifier != D3D10_SB_OPERAND_MODIFIER_NEG) {
    std::cerr << "Expected literal modifier override to coexist with replayed "
                 "indices in emitted operand.\n";
    return 1;
  }

  std::cout << "SM5 builder replay API preserved source indices at instruction "
               "index "
            << targetIndex << ".\n";
  return 0;
}
