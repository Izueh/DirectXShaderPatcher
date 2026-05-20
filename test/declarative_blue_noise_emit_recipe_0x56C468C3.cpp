#include "TestSupport.h"

#include <cstdlib>
#include <iostream>

int main(int argc, char **argv) {
  if (argc != 3 && argc != 4) {
    std::cerr << "Usage: declarative_blue_noise_emit_recipe_0x56C468C3 <input.cso> <recipe.yml> [output.cso]\n";
    return 1;
  }

  ScopedCoInitialize coinit;

  std::vector<uint8_t> inputShader;
  if (!ReadFile(argv[1], inputShader)) {
    std::cerr << "Failed to read input shader: " << argv[1] << "\n";
    return 1;
  }

  LoadedDxilShader shader;
  if (!LoadShaderForMutation(argv[1], shader, true))
    return 1;

  llvm::Function *entryFunction = shader.dxilModule->GetEntryFunction();
  if (entryFunction == nullptr) {
    std::cerr << "Failed to locate DXIL entry function.\n";
    return 1;
  }

  const size_t initialSrvCount = shader.dxilModule->GetSRVs().size();
  const size_t initialCBufferCount = shader.dxilModule->GetCBuffers().size();
  const unsigned initialBlueNoiseCount =
      CountBlueNoiseTextureLoads(*entryFunction, *shader.dxilModule);
  const unsigned initialTextureLoadCount =
      CountDxOpCalls(*entryFunction, "dx.op.textureLoad.f32");
  if (initialBlueNoiseCount == 0) {
    std::cerr << "Expected test shader to contain at least one BlueNoise texture load.\n";
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
    std::cerr << "Expected declarative BlueNoise recipe to apply at least one rule.\n";
    return 1;
  }

  if (argc == 4 &&
      !WriteFile(argv[3], outputContainer.data(), outputContainer.size())) {
    std::cerr << "Failed to write patched container: " << argv[3] << "\n";
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

  const unsigned finalBlueNoiseCount =
      CountBlueNoiseTextureLoads(*patchedEntryFunction, *patchedDxilModule);
  const unsigned finalTextureLoadCount =
      CountDxOpCalls(*patchedEntryFunction, "dx.op.textureLoad.f32");
  if (finalBlueNoiseCount != 0) {
    std::cerr << "Expected declarative BlueNoise recipe to remove all BlueNoise loads, but saw "
              << finalBlueNoiseCount << ".\n";
    return 1;
  }

  if (finalTextureLoadCount != initialTextureLoadCount) {
    std::cerr << "Expected declarative BlueNoise rewrite to replace texture loads in place; count changed from "
              << initialTextureLoadCount << " to " << finalTextureLoadCount << ".\n";
    return 1;
  }

  if (patchedDxilModule->GetSRVs().size() != initialSrvCount + 1) {
    std::cerr << "Expected SRV count to increase from " << initialSrvCount
              << " to " << (initialSrvCount + 1) << ", but saw "
              << patchedDxilModule->GetSRVs().size() << ".\n";
    return 1;
  }

  if (patchedDxilModule->GetCBuffers().size() != initialCBufferCount + 1) {
    std::cerr << "Expected cbuffer count to increase from " << initialCBufferCount
              << " to " << (initialCBufferCount + 1) << ", but saw "
              << patchedDxilModule->GetCBuffers().size() << ".\n";
    return 1;
  }

  std::cout << "Declarative BlueNoise rewrite removed " << initialBlueNoiseCount
            << " BlueNoise texture loads without changing total textureLoad.f32 count.\n";
  std::cout.flush();
  std::cerr.flush();
  std::_Exit(0);
}
