#include "TestSupport.h"
#include "dxp/sm5/Patch.h"
#include "dxp/sm5/RecipeParse.h"

#include <iostream>
#include <vector>

namespace {

static int FindNthOpcodeIndex(const std::vector<uint32_t> &opcodes,
                              uint32_t opcode, int ordinal) {
  int seen = 0;
  for (size_t i = 0; i < opcodes.size(); ++i) {
    if (opcodes[i] != opcode) {
      continue;
    }
    if (seen == ordinal) {
      return static_cast<int>(i);
    }
    ++seen;
  }
  return -1;
}

}

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "Usage: sm5_recipe_replace_frc_with_mov <input.ps_5_0.cso> "
                 "<recipe.recipe.yml>\n";
    return 1;
  }

  std::vector<uint8_t> inputBytes;
  if (!ReadFile(argv[1], inputBytes)) {
    std::cerr << "Failed to read input file: " << argv[1] << "\n";
    return 1;
  }

  std::vector<uint32_t> inputOpcodes;
  std::string inputParseError;
  if (!dxp::sm5::ExtractProgramOpcodes(inputBytes, inputOpcodes,
                                       &inputParseError)) {
    std::cerr << "Failed to extract input opcodes: " << inputParseError << "\n";
    return 1;
  }

  const int firstInputFrc =
      FindNthOpcodeIndex(inputOpcodes, D3D10_SB_OPCODE_FRC, 0);
  const int secondInputFrc =
      FindNthOpcodeIndex(inputOpcodes, D3D10_SB_OPCODE_FRC, 1);
  if (firstInputFrc < 0 || secondInputFrc < 0) {
    std::cerr
        << "Expected at least two FRC instructions in the input program.\n";
    return 1;
  }

  dxp::sm5::RecipeParseResult parseResult;
  if (!dxp::sm5::ParseRecipeFile(argv[2], parseResult)) {
    std::cerr << "Failed to parse SM5 recipe file: " << parseResult.Error
              << "\n";
    return 1;
  }

  const auto patchResult =
      dxp::sm5::PatchContainer(inputBytes, parseResult.Recipe);
  if (!patchResult.Success) {
    std::cerr << "Failed to patch SM5 shader: " << patchResult.Error << "\n";
    return 1;
  }

  if (patchResult.OutputBytes.empty()) {
    std::cerr << "Patched output is unexpectedly empty.\n";
    return 1;
  }

  if (patchResult.OutputBytes == inputBytes) {
    std::cerr << "Patched output is identical to input; expected a mutation.\n";
    return 1;
  }

  if (patchResult.RecipeContext.TotalRuleMatches == 0) {
    std::cerr
        << "Recipe reported zero matches; expected at least one FRC match.\n";
    return 1;
  }

  std::vector<uint32_t> patchedOpcodes;
  std::string patchedParseError;
  if (!dxp::sm5::ExtractProgramOpcodes(patchResult.OutputBytes, patchedOpcodes,
                                       &patchedParseError)) {
    std::cerr << "Failed to extract patched opcodes: " << patchedParseError
              << "\n";
    return 1;
  }

  if (patchedOpcodes.size() != inputOpcodes.size()) {
    std::cerr << "Patched instruction count changed unexpectedly.\n";
    return 1;
  }

  if (patchedOpcodes[static_cast<size_t>(firstInputFrc)] !=
      D3D10_SB_OPCODE_MOV) {
    std::cerr << "First FRC was not replaced with MOV.\n";
    return 1;
  }

  if (patchedOpcodes[static_cast<size_t>(secondInputFrc)] !=
      D3D10_SB_OPCODE_FRC) {
    std::cerr << "Second FRC changed unexpectedly.\n";
    return 1;
  }

  std::cout << "SM5 recipe replacement succeeded at instruction index "
            << firstInputFrc << ". Total rule matches: "
            << patchResult.RecipeContext.TotalRuleMatches << ".\n";
  return 0;
}