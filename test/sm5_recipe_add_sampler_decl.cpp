#include "TestSupport.h"
#include "dxp/sm5/Container.h"
#include "dxp/sm5/Parse.h"
#include "dxp/sm5/RecipeParse.h"

#include <iostream>
#include <vector>

namespace {

static bool HasSamplerDecl(const dxp::sm5::Program &program,
                           uint32_t bindPoint,
                           uint32_t mode) {
  for (const auto &instruction : program.Instructions) {
    if (static_cast<dxp::sm5::OpcodeType>(instruction.Opcode) != D3D10_SB_OPCODE_DCL_SAMPLER) {
      continue;
    }

    if (instruction.Operands.empty() || instruction.Operands.front().Indices.empty()) {
      continue;
    }

    if (instruction.Operands.front().Indices.front() == bindPoint &&
        !instruction.RawTokens.empty() &&
        DECODE_D3D10_SB_SAMPLER_MODE(instruction.RawTokens.front()) ==
            static_cast<D3D10_SB_SAMPLER_MODE>(mode)) {
      return true;
    }
  }

  return false;
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: sm5_recipe_add_sampler_decl <input.ps_5_0.cso>\n";
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

  const size_t initialSamplerCount = inputProgram.Samplers.size();

  const char *recipeText = R"YAML(version: 1
sampler_decls:
  - bind_point: 11
    mode: comparison
)YAML";

  dxp::sm5::RecipeParseResult parseResult;
  if (!dxp::sm5::ParseRecipeText(recipeText, parseResult, "inline-sm5-sampler-decl-test")) {
    std::cerr << "Failed to parse inline SM5 sampler recipe: " << parseResult.Error << "\n";
    return 1;
  }

  const auto patchResult = dxp::sm5::PatchContainerInMemory(inputBytes, parseResult.Recipe);
  if (!patchResult.Success) {
    std::cerr << "Failed to patch SM5 shader with sampler declaration recipe: "
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

  if (patchedProgram.Samplers.size() != initialSamplerCount + 1) {
    std::cerr << "Expected one additional sampler declaration.\n";
    return 1;
  }

  if (!HasSamplerDecl(patchedProgram, 11u, D3D10_SB_SAMPLER_MODE_COMPARISON)) {
    std::cerr << "Expected patched shader to declare comparison sampler s11.\n";
    return 1;
  }

  std::cout << "SM5 recipe added comparison sampler declaration s11.\n";
  return 0;
}