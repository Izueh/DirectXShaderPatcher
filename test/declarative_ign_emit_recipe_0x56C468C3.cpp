#include "TestSupport.h"

#include <cstdlib>
#include <iostream>

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "Usage: declarative_ign_emit_recipe_0x56C468C3 <input.cso> <recipe.yml>\n";
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

  const unsigned initialIgnCount = CountIgnNoiseChains(*entryFunction);
  const unsigned initialTextureLoadCount =
      CountDxOpCalls(*entryFunction, "dx.op.textureLoad.f32");
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
    std::cerr << "Expected declarative IGN emit recipe to apply at least one rule.\n";
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

  if (HasTypedHandleDxilOpOverloads(*patchedModule)) {
    std::cerr << "Patched module introduced typed DXIL handle op overloads instead of reusing the shader's existing prototypes.\n";
    return 1;
  }

  const unsigned finalIgnCount = CountIgnNoiseChains(*patchedEntryFunction);
  const unsigned finalTextureLoadCount =
      CountDxOpCalls(*patchedEntryFunction, "dx.op.textureLoad.f32");
  if (finalIgnCount >= initialIgnCount) {
    std::cerr << "Expected declarative IGN emit rewrite to reduce IGN chain count from "
              << initialIgnCount << ", but saw " << finalIgnCount << ".\n";
    return 1;
  }

  if (finalTextureLoadCount <= initialTextureLoadCount) {
    std::cerr << "Expected declarative IGN emit rewrite to increase textureLoad.f32 count from "
              << initialTextureLoadCount << ", but saw " << finalTextureLoadCount << ".\n";
    return 1;
  }

  std::cout << "Declarative IGN emit rewrite reduced IGN chain count from "
            << initialIgnCount << " to " << finalIgnCount
            << " and increased textureLoad.f32 count from "
            << initialTextureLoadCount << " to " << finalTextureLoadCount << ".\n";
  std::cout.flush();
  std::cerr.flush();
  std::_Exit(0);
}