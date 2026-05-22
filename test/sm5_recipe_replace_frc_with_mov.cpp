#include "TestSupport.h"
#include "dxp/sm5/Container.h"
#include "dxp/sm5/Patch.h"
#include "dxp/sm5/Parse.h"
#include "dxp/sm5/RecipeParse.h"

#include <iostream>
#include <vector>

namespace {

static int FindNthOpcodeIndex(const dxp::sm5::Program &program,
                              dxp::sm5::OpcodeType opcode,
                              int ordinal) {
  int seen = 0;
  for (size_t i = 0; i < program.Instructions.size(); ++i) {
    if (static_cast<dxp::sm5::OpcodeType>(program.Instructions[i].Opcode) != opcode) {
      continue;
    }
    if (seen == ordinal) {
      return static_cast<int>(i);
    }
    ++seen;
  }
  return -1;
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "Usage: sm5_recipe_replace_frc_with_mov <input.ps_5_0.cso> <recipe.recipe.yml>\n";
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

  const int firstInputFrc = FindNthOpcodeIndex(inputProgram, D3D10_SB_OPCODE_FRC, 0);
  const int secondInputFrc = FindNthOpcodeIndex(inputProgram, D3D10_SB_OPCODE_FRC, 1);
  if (firstInputFrc < 0 || secondInputFrc < 0) {
    std::cerr << "Expected at least two FRC instructions in the input program.\n";
    return 1;
  }

  dxp::sm5::RecipeParseResult parseResult;
  if (!dxp::sm5::ParseRecipeFile(argv[2], parseResult)) {
    std::cerr << "Failed to parse SM5 recipe file: " << parseResult.Error << "\n";
    return 1;
  }

  const auto patchResult =
      dxp::sm5::PatchContainerInMemory(inputBytes, parseResult.Recipe);
  if (!patchResult.Success) {
    std::cerr << "Failed to patch SM5 shader: " << patchResult.Error << "\n";
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

  if (patchedProgram.Instructions.size() != inputProgram.Instructions.size()) {
    std::cerr << "Patched instruction count changed unexpectedly.\n";
    return 1;
  }

  const auto firstOpcode = static_cast<dxp::sm5::OpcodeType>(
      patchedProgram.Instructions[static_cast<size_t>(firstInputFrc)].Opcode);
  if (firstOpcode != D3D10_SB_OPCODE_MOV) {
    std::cerr << "First FRC was not replaced with MOV.\n";
    return 1;
  }

  const auto secondOpcode = static_cast<dxp::sm5::OpcodeType>(
      patchedProgram.Instructions[static_cast<size_t>(secondInputFrc)].Opcode);
  if (secondOpcode != D3D10_SB_OPCODE_FRC) {
    std::cerr << "Second FRC changed unexpectedly.\n";
    return 1;
  }

  std::cout << "SM5 recipe replacement succeeded at instruction index "
            << firstInputFrc << ".\n";
  return 0;
}