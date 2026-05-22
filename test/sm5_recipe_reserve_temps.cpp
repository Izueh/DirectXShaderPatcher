#include "TestSupport.h"
#include "dxp/sm5/Container.h"
#include "dxp/sm5/Patch.h"
#include "dxp/sm5/Parse.h"
#include "dxp/sm5/RecipeParse.h"

#include <iostream>
#include <vector>

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: sm5_recipe_reserve_temps <input.ps_5_0.cso>\n";
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

  const char *recipeText = R"YAML(version: 1
reserved_temps: 3
steps:
  - name: noop
    required: false
    rules: []
)YAML";

  dxp::sm5::RecipeParseResult parseResult;
  if (!dxp::sm5::ParseRecipeText(recipeText, parseResult,
                                 "inline-sm5-reserve-temps-test")) {
    std::cerr << "Failed to parse inline SM5 recipe: " << parseResult.Error
              << "\n";
    return 1;
  }

  const auto patchResult = dxp::sm5::PatchContainerInMemory(inputBytes, parseResult.Recipe);
  if (!patchResult.Success) {
    std::cerr << "Failed to patch SM5 shader with reserved temp recipe: "
              << patchResult.Error << "\n";
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

  if (patchedProgram.TempCount != initialTempCount + 3) {
    std::cerr << "Expected reserved_temps to increase dcl_temps by 3.\n";
    return 1;
  }

  std::cout << "SM5 recipe reserved 3 additional temp registers.\n";
  return 0;
}
