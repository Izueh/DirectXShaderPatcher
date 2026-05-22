#include "TestSupport.h"
#include "dxp/sm5/Container.h"
#include "dxp/sm5/Patch.h"
#include "dxp/sm5/Parse.h"
#include "dxp/sm5/Recipe.h"

#include <iostream>
#include <vector>

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: sm5_recipe_custom_step_reserve_temps <input.ps_5_0.cso>\n";
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

  const uint32_t initialTempCount = inputProgram.TempCount;

  dxp::sm5::Recipe recipe;
  recipe.AddStep(dxp::sm5::MakeCustomRecipeStep(
      "reserve_two_temps", [](dxp::sm5::RecipeContext &context) {
        uint32_t baseIndex = 0;
        if (!dxp::sm5::ReserveTempRegisters(context, 2, baseIndex)) {
          return dxp::sm5::MakeRecipeStepFailure(
              context, "ReserveTempRegisters failed");
        }

        context.SetState("reserved_base", baseIndex);
        return dxp::sm5::MakeRecipeStepSuccess(true, 0, false);
      }));

  const auto patchResult = dxp::sm5::PatchContainerInMemory(inputBytes, recipe);
  if (!patchResult.Success) {
    std::cerr << "Failed to patch SM5 shader with custom reserve step: "
              << patchResult.Error << "\n";
    return 1;
  }

  const uint32_t *reservedBase =
      patchResult.RecipeContext.FindState<uint32_t>("reserved_base");
  if (reservedBase == nullptr) {
    std::cerr << "Expected custom step to publish reserved_base state.\n";
    return 1;
  }

  if (*reservedBase != initialTempCount) {
    std::cerr << "Expected reserved base to equal original temp count.\n";
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

  if (patchedProgram.TempCount != initialTempCount + 2) {
    std::cerr << "Expected custom step reserve to increase dcl_temps by 2.\n";
    return 1;
  }

  std::cout << "SM5 custom step reserved 2 temp registers at base "
            << *reservedBase << ".\n";
  return 0;
}
