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
    std::cerr << "Usage: sm5_recipe_context_variables <input.ps_5_0.cso>\n";
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

  const char *recipeText = R"YAML(version: 1
steps:
  - kind: apply_rules
    name: rewrite_with_runtime_vars
    if:
      eq:
        input: enable_noise_patch
        value: true
    rules:
      - name: rewrite_with_runtime_vars_rule
        match:
          opcode: mul
          operands:
            - capture: dst
            - capture: src
        emit:
          - opcode: mov
            operands:
              - capture: dst
              - type: immediate32
                immediates_u32: [frame_seed]
)YAML";

  dxp::sm5::RecipeParseResult parseResult;
  if (!dxp::sm5::ParseRecipeText(recipeText, parseResult,
                                 "inline-sm5-context-variables-test")) {
    std::cerr << "Failed to parse inline SM5 recipe: " << parseResult.Error
              << "\n";
    return 1;
  }

  dxp::sm5::RecipeContext context;
  context.SetVariable<uint32_t>("frame_seed", 0xDEADBEEFu);

  dxp::sm5::RecipeExecutionOptions options;
  options.BeforeStep = [](const std::string &stepName,
                          dxp::sm5::RecipeContext &stepContext) {
    if (stepName == "rewrite_with_runtime_vars") {
      stepContext.SetVariable<bool>("enable_noise_patch", true);
      stepContext.SetVariable<uint32_t>("frame_seed", 0x3F800000u);
    }
  };
  options.AfterStep = [](const std::string &stepName,
                         const dxp::sm5::RecipeStepResult &,
                         dxp::sm5::RecipeContext &stepContext) {
    if (stepName == "rewrite_with_runtime_vars") {
      stepContext.UnsetVariable("frame_seed");
      stepContext.ResetVariables();
    }
  };

  const auto patchResult =
      dxp::sm5::PatchContainer(inputBytes, parseResult.Recipe, context, options);
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
    std::cerr << "Expected matched MUL instruction to become MOV.\n";
    return 1;
  }

  if (patchedInstruction.Operands.size() != 2 ||
      patchedInstruction.Operands[1].Type != D3D10_SB_OPERAND_TYPE_IMMEDIATE32 ||
      patchedInstruction.Operands[1].ImmediateValues.size() != 1 ||
      patchedInstruction.Operands[1].ImmediateValues[0] != 0x3F800000u) {
    std::cerr << "Expected emitted immediate32 operand to resolve from frame_seed variable.\n";
    return 1;
  }

  const uint32_t *restoredSeed = context.FindVariable<uint32_t>("frame_seed");
  if (restoredSeed == nullptr || *restoredSeed != 0xDEADBEEFu) {
    std::cerr << "Expected ResetVariables to restore initial frame_seed value.\n";
    return 1;
  }

  const bool *hasMulMatch =
      context.FindState<bool>("rewrite_with_runtime_vars_rule");
  if (hasMulMatch == nullptr || !*hasMulMatch) {
    std::cerr << "Expected rule name to publish true match outcome.\n";
    return 1;
  }

  std::cout << "SM5 context variables resolved in immediates and step hooks mutated/reset runtime context.\n";
  return 0;
}
