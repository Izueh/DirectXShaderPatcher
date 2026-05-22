#include "TestSupport.h"
#include "dxp/sm5/Container.h"
#include "dxp/sm5/Patch.h"
#include "dxp/sm5/Parse.h"
#include "dxp/sm5/Recipe.h"

#include <iostream>
#include <vector>

namespace {

static bool OperandsEqual(const dxp::sm5::Operand &lhs,
                          const dxp::sm5::Operand &rhs) {
  if (lhs.Type != rhs.Type || lhs.NumComponents != rhs.NumComponents ||
      lhs.ComponentMode != rhs.ComponentMode || lhs.Modifier != rhs.Modifier ||
      lhs.Indices != rhs.Indices || lhs.ImmediateValues != rhs.ImmediateValues) {
    return false;
  }

  if (static_cast<bool>(lhs.RelativeOperand) !=
      static_cast<bool>(rhs.RelativeOperand)) {
    return false;
  }

  if (lhs.RelativeOperand && rhs.RelativeOperand) {
    return OperandsEqual(*lhs.RelativeOperand, *rhs.RelativeOperand);
  }

  return true;
}

static int FindFrcMulSequence(const dxp::sm5::Program &program) {
  for (size_t index = 0; index + 1 < program.Instructions.size(); ++index) {
    const auto &first = program.Instructions[index];
    const auto &second = program.Instructions[index + 1];
    if (static_cast<dxp::sm5::OpcodeType>(first.Opcode) == D3D10_SB_OPCODE_FRC &&
        static_cast<dxp::sm5::OpcodeType>(second.Opcode) == D3D10_SB_OPCODE_MUL &&
        second.Operands.size() >= 2) {
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

  const int targetInstructionIndex = FindFrcMulSequence(inputProgram);
  if (targetInstructionIndex < 0) {
    std::cerr << "Failed to locate target FRC/MUL sequence.\n";
    return 1;
  }

  const uint32_t initialTempCount = inputProgram.TempCount;
    const dxp::sm5::Instruction originalMulInstruction =
      inputProgram.Instructions[static_cast<size_t>(targetInstructionIndex + 1)];

  dxp::sm5::RecipeRule stateTempRule;
    dxp::sm5::RecipeInstructionPattern firstInstruction;
    firstInstruction.Opcode = "frc";
    firstInstruction.Capture = "ign_frc";
    stateTempRule.Match.Sequence.push_back(firstInstruction);

    dxp::sm5::RecipeInstructionPattern secondInstruction;
    secondInstruction.Opcode = "mul";
    secondInstruction.Capture = "ign_mul";
    dxp::sm5::RecipeOperandPattern matchDst;
    matchDst.Capture = "dst";
    secondInstruction.Operands.push_back(matchDst);
    dxp::sm5::RecipeOperandPattern matchSrc;
    matchSrc.Capture = "src";
    secondInstruction.Operands.push_back(matchSrc);
    stateTempRule.Match.Sequence.push_back(secondInstruction);

  dxp::sm5::RecipeInstructionTemplate saveValue;
  saveValue.Opcode = "mov";
  dxp::sm5::RecipeOperandPattern saveDst;
  saveDst.Type = "temp";
  saveDst.StateTemp = "shared_temp_r";
  saveValue.Operands.push_back(saveDst);
  dxp::sm5::RecipeOperandPattern saveSrc;
  saveSrc.FromCapture = "src";
  saveValue.Operands.push_back(saveSrc);
  stateTempRule.Emit.push_back(saveValue);

  dxp::sm5::RecipeInstructionTemplate restoreValue;
  restoreValue.Opcode = "mov";
  dxp::sm5::RecipeOperandPattern restoreDst;
  restoreDst.FromCapture = "dst";
  restoreValue.Operands.push_back(restoreDst);
  dxp::sm5::RecipeOperandPattern restoreSrc;
  restoreSrc.Type = "temp";
  restoreSrc.StateTemp = "shared_temp_r";
  restoreValue.Operands.push_back(restoreSrc);
  stateTempRule.Emit.push_back(restoreValue);

  dxp::sm5::Recipe recipe;
  recipe.AddStep(dxp::sm5::MakeCustomRecipeStep(
      "reserve_shared_temp", [](dxp::sm5::RecipeContext &context) {
        uint32_t baseIndex = 0;
        if (!dxp::sm5::ReserveTempRegisters(context, 1, baseIndex)) {
          return dxp::sm5::MakeRecipeStepFailure(
              context, "ReserveTempRegisters failed");
        }
        context.SetState("shared_temp_r", baseIndex);
        return dxp::sm5::MakeRecipeStepSuccess(true, 0, false);
      }));
  recipe.AddStep(dxp::sm5::MakeRewriteRulesStep(
      "rewrite_with_state_temp", {stateTempRule},
      dxp::sm5::RecipeRuleApplicationMode::First, true));

  const auto patchResult = dxp::sm5::PatchContainerInMemory(inputBytes, recipe);
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

  dxp::sm5::Container patchedContainer;
  if (!dxp::sm5::ParseDxbcContainer(patchResult.OutputBytes, patchedContainer)) {
    std::cerr << "Failed to parse patched DXBC container.\n";
    return 1;
  }

  dxp::sm5::Program patchedProgram;
  if (!dxp::sm5::ParseShaderChunk(patchedContainer, patchedProgram)) {
    std::cerr << "Failed to parse patched SM5 program.\n";
    return 1;
  }

  if (patchedProgram.TempCount != initialTempCount + 1) {
    std::cerr << "Expected state_temp flow to reserve one temp register.\n";
    return 1;
  }

  const auto &saveInstruction =
      patchedProgram.Instructions[static_cast<size_t>(targetInstructionIndex)];
  if (static_cast<dxp::sm5::OpcodeType>(saveInstruction.Opcode) !=
      D3D10_SB_OPCODE_MOV) {
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
    std::cerr << "Expected first MOV destination to target resolved state_temp register.\n";
    return 1;
  }

  const auto &restoreInstruction =
      patchedProgram.Instructions[static_cast<size_t>(targetInstructionIndex + 1)];
  if (static_cast<dxp::sm5::OpcodeType>(restoreInstruction.Opcode) !=
      D3D10_SB_OPCODE_MOV) {
    std::cerr << "Expected second emitted instruction to be MOV.\n";
    return 1;
  }

  if (restoreInstruction.Operands.size() != 2) {
    std::cerr << "Expected second emitted MOV to have two operands.\n";
    return 1;
  }

  if (!OperandsEqual(restoreInstruction.Operands[0],
                     originalMulInstruction.Operands[0])) {
    std::cerr << "Expected second MOV destination to preserve captured destination.\n";
    return 1;
  }

  if (restoreInstruction.Operands[1].Type != D3D10_SB_OPERAND_TYPE_TEMP ||
      restoreInstruction.Operands[1].Indices.empty() ||
      restoreInstruction.Operands[1].Indices[0] != *sharedTempBase) {
    std::cerr << "Expected second MOV source to read resolved state_temp register.\n";
    return 1;
  }

  std::cout << "SM5 state_temp emit operand resolved to r" << *sharedTempBase
            << " across custom and declarative recipe steps.\n";
  return 0;
}
