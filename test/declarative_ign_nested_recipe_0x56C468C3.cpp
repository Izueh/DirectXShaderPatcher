#include "TestSupport.h"

#include <cstdlib>
#include <iostream>

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "Usage: declarative_ign_nested_recipe_0x56C468C3 <input.cso> "
                 "<recipe.yml>\n";
    return 1;
  }

  ScopedCoInitialize coinit;

  std::vector<uint8_t> inputShader;
  if (!ReadFile(argv[1], inputShader)) {
    std::cerr << "Failed to read input shader: " << argv[1] << "\n";
    return 1;
  }

  LoadedDxilShader shader;
  if (!LoadShaderFromPath(argv[1], shader, false))
    return 1;

  llvm::Function *entryFunction = shader.dxilModule->GetEntryFunction();
  if (entryFunction == nullptr) {
    std::cerr << "Failed to locate DXIL entry function.\n";
    return 1;
  }

  const unsigned initialIgnCount = CountIgnNoiseChains(*entryFunction);
  if (initialIgnCount == 0) {
    std::cerr << "Expected test shader to contain at least one IGN chain.\n";
    return 1;
  }

  DxilRecipeParseResult parseResult;
  if (!ParseDxilRecipeFile(argv[2], parseResult)) {
    std::cerr << "Failed to parse recipe file: " << parseResult.error << "\n";
    return 1;
  }

  DxilRecipeContext recipeContext;
  std::vector<uint8_t> outputContainer;
  if (!PatchDxilContainer(parseResult.recipe, inputShader, outputContainer,
                          parseResult.patchOptions, &recipeContext)) {
    std::cerr << "PatchDxilContainer failed.";
    if (!recipeContext.lastError.empty())
      std::cerr << " " << recipeContext.lastError;
    std::cerr << "\n";
    return 1;
  }

  if (recipeContext.totalRuleMatches == 0) {
    std::cerr << "Expected nested IGN declarative recipe to apply at least one "
                 "rule.\n";
    return 1;
  }

  llvm::LLVMContext patchedContext;
  std::unique_ptr<llvm::Module> patchedModule;
  hlsl::DxilModule *patchedDxilModule = nullptr;
  if (!ReloadPatchedContainer(outputContainer, patchedContext, patchedModule,
                              patchedDxilModule)) {
    return 1;
  }

  llvm::Function *patchedEntryFunction = patchedDxilModule->GetEntryFunction();
  if (patchedEntryFunction == nullptr) {
    std::cerr << "Failed to locate patched DXIL entry function.\n";
    return 1;
  }

  const unsigned finalIgnCount = CountIgnNoiseChains(*patchedEntryFunction);
  if (finalIgnCount >= initialIgnCount) {
    std::cerr << "Expected nested IGN declarative rewrite to reduce IGN chain "
                 "count from "
              << initialIgnCount << ", but saw " << finalIgnCount << ".\n";
    return 1;
  }

  std::cout << "Nested IGN declarative rewrite reduced IGN chain count from "
            << initialIgnCount << " to " << finalIgnCount << ".\n";
  std::cout.flush();
  std::cerr.flush();
}