#include "TestSupport.h"

#include <cstdlib>
#include <iostream>

namespace {

static unsigned CountOpMatches(llvm::Function &function,
                               hlsl::OP::OpCode opCode) {
  std::vector<DxilMatchResult> matches;
  return CollectDxilCallMatches(function, DxOpCall(opCode), matches);
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "Usage: declarative_rewrite_recipe_0x56C468C3 <input.cso> <recipe.yml>\n";
    return 1;
  }

  ScopedCoInitialize coinit;

  std::vector<uint8_t> inputShader;
  if (!ReadFile(argv[1], inputShader)) {
    std::cerr << "Failed to read input shader: " << argv[1] << "\n";
    return 1;
  }

  LoadedDxilShader shader;
  if (!LoadShaderForMutation(argv[1], shader, false))
    return 1;

  llvm::Function *entryFunction = shader.dxilModule->GetEntryFunction();
  if (entryFunction == nullptr) {
    std::cerr << "Failed to locate DXIL entry function.\n";
    return 1;
  }

  const unsigned initialFrcCount = CountOpMatches(*entryFunction, hlsl::OP::OpCode::Frc);
  if (initialFrcCount == 0) {
    std::cerr << "Expected test shader to contain at least one Frc call.\n";
    return 1;
  }

  DxilRecipeParseResult parseResult;
  if (!ParseDxilRecipeFile(argv[2], parseResult)) {
    std::cerr << "Failed to parse recipe file: " << parseResult.error << "\n";
    return 1;
  }

  DxilRecipeContext recipeContext;
  std::vector<uint8_t> outputContainer;
  if (!PatchDxilContainerInMemory(parseResult.recipe,
                                  inputShader,
                                  outputContainer,
                                  parseResult.patchOptions,
                                  &recipeContext)) {
    std::cerr << "PatchDxilContainerInMemory failed.";
    if (!recipeContext.lastError.empty())
      std::cerr << " " << recipeContext.lastError;
    std::cerr << "\n";
    return 1;
  }

  if (recipeContext.totalRuleMatches == 0) {
    std::cerr << "Expected declarative rewrite recipe to apply at least one rule.\n";
    return 1;
  }

  llvm::LLVMContext patchedContext;
  std::unique_ptr<llvm::Module> patchedModule;
  hlsl::DxilModule *patchedDxilModule = nullptr;
  if (!ReloadPatchedContainer(outputContainer,
                              patchedContext,
                              patchedModule,
                              patchedDxilModule)) {
    return 1;
  }

  llvm::Function *patchedEntryFunction = patchedDxilModule->GetEntryFunction();
  if (patchedEntryFunction == nullptr) {
    std::cerr << "Failed to locate patched DXIL entry function.\n";
    return 1;
  }

  const unsigned finalFrcCount = CountOpMatches(*patchedEntryFunction, hlsl::OP::OpCode::Frc);
  if (finalFrcCount >= initialFrcCount) {
    std::cerr << "Expected declarative rewrite to reduce Frc count from "
              << initialFrcCount << ", but saw " << finalFrcCount << ".\n";
    return 1;
  }

  std::cout << "Declarative rewrite reduced Frc count from "
            << initialFrcCount << " to " << finalFrcCount << ".\n";
  std::cout.flush();
  std::cerr.flush();
  std::_Exit(0);
}