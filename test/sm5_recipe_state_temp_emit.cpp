#include "TestSupport.h"
#include "dxp/sm5/Patch.h"
#include "dxp/sm5/Recipe.h"

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

} // namespace

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

  const uint32_t initialTempCount = inputProgram.TempCount;
  const dxp::sm5::ProgramInstruction originalMulInstruction =
      inputProgram
          .Instructions[static_cast<size_t>(targetInstructionIndex + 1)];

    dxp::sm5::RecipeRule stateTempRule =
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
                           .CaptureAs("dst"))
                   .AddOperand(dxp::sm5::RecipeOperandPattern{}
                           .CaptureAs("src"))))
        .AddEmit(dxp::sm5::RecipeInstructionTemplate{}
               .WithOpcode("mov")
               .AddOperand(dxp::sm5::RecipeOperandPattern{}
                       .WithType("temp")
                       .WithStateTemp("shared_temp_r"))
               .AddOperand(
                 dxp::sm5::RecipeOperandPattern{}.CaptureAs("src")))
        .AddEmit(dxp::sm5::RecipeInstructionTemplate{}
               .WithOpcode("mov")
               .AddOperand(
                 dxp::sm5::RecipeOperandPattern{}.CaptureAs("dst"))
               .AddOperand(dxp::sm5::RecipeOperandPattern{}
                       .WithType("temp")
                       .WithStateTemp("shared_temp_r")));

  dxp::sm5::Recipe recipe;
  recipe.AddStep(dxp::sm5::MakeCustomRecipeStep(
      "reserve_shared_temp", [](dxp::sm5::RecipeContext &context) {
        uint32_t baseIndex = 0;
        if (!dxp::sm5::ReserveTempRegisters(context, 1, baseIndex)) {
          return dxp::sm5::MakeRecipeStepFailure(context,
                                                 "ReserveTempRegisters failed");
        }
        context.SetState("shared_temp_r", baseIndex);
        return dxp::sm5::MakeRecipeStepSuccess(true, 0, false);
      }));
  recipe.AddStep(dxp::sm5::MakeRewriteRulesStep(
      "rewrite_with_state_temp", {stateTempRule},
      dxp::sm5::RecipeRuleApplicationMode::First, true));

  const auto patchResult = dxp::sm5::PatchContainer(inputBytes, recipe);
  if (!patchResult.Success) {
    std::cerr << "Failed to patch SM5 shader: " << patchResult.Error << "\n";
    return 1;
  }

  const uint32_t *sharedTempBase =
      patchResult.RecipeContext.FindState<uint32_t>("shared_temp_r");
  if (sharedTempBase == nullptr) {
    std::cerr << "Expected custom step to publish shared_temp_r state.\n";
    return 1;
  }

  if (*sharedTempBase != initialTempCount) {
    std::cerr << "Expected shared temp base to equal original temp count.\n";
    return 1;
  }

  dxp::sm5::ProgramInspection patchedProgram;
  if (!dxp::sm5::InspectProgram(patchResult.OutputBytes, patchedProgram,
                                &inspectError)) {
    std::cerr << "Failed to inspect patched SM5 program: " << inspectError
              << "\n";
    return 1;
  }

  if (patchedProgram.TempCount != initialTempCount + 1) {
    std::cerr << "Expected state_temp flow to reserve one temp register.\n";
    return 1;
  }

  const auto &saveInstruction =
      patchedProgram.Instructions[static_cast<size_t>(targetInstructionIndex)];
  if (saveInstruction.Opcode != D3D10_SB_OPCODE_MOV) {
    std::cerr << "Expected first emitted instruction to be MOV.\n";
    return 1;
  }

  if (saveInstruction.Operands.size() != 2) {
    std::cerr << "Expected first emitted MOV to have two operands.\n";
    return 1;
  }

  if (saveInstruction.Operands[0].Type != D3D10_SB_OPERAND_TYPE_TEMP ||
      saveInstruction.Operands[0].Indices.empty() ||
      saveInstruction.Operands[0].Indices[0] != *sharedTempBase) {
    std::cerr << "Expected first MOV destination to target resolved state_temp "
                 "register.\n";
    return 1;
  }

  const auto &restoreInstruction =
      patchedProgram
          .Instructions[static_cast<size_t>(targetInstructionIndex + 1)];
  if (restoreInstruction.Opcode != D3D10_SB_OPCODE_MOV) {
    std::cerr << "Expected second emitted instruction to be MOV.\n";
    return 1;
  }

  if (restoreInstruction.Operands.size() != 2) {
    std::cerr << "Expected second emitted MOV to have two operands.\n";
    return 1;
  }

  if (!OperandsEqual(restoreInstruction.Operands[0],
                     originalMulInstruction.Operands[0])) {
    std::cerr << "Expected second MOV destination to preserve captured "
                 "destination.\n";
    return 1;
  }

  if (restoreInstruction.Operands[1].Type != D3D10_SB_OPERAND_TYPE_TEMP ||
      restoreInstruction.Operands[1].Indices.empty() ||
      restoreInstruction.Operands[1].Indices[0] != *sharedTempBase) {
    std::cerr
        << "Expected second MOV source to read resolved state_temp register.\n";
    return 1;
  }

  std::cout << "SM5 state_temp emit operand resolved to r" << *sharedTempBase
            << " across custom and declarative recipe steps.\n";
  return 0;
}
