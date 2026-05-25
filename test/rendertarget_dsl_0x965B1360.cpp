#include "TestSupport.h"

#include <iostream>

static bool ExpectSingleRenderTargetStore(llvm::Function &entryFunction,
                                          RenderTargetStoreDesc desc,
                                          const char *label,
                                          llvm::CallInst *&storeOut) {
  std::vector<DxilMatchResult> matches;
  const unsigned matchCount = CollectDxilCallMatches(
      entryFunction, RenderTargetStoreCall(desc).Capture(label), matches);
  if (matchCount == 0 || matches.empty() || matches[0].rootCall == nullptr) {
    std::cerr << "Expected at least one render-target store match for " << label
              << ", but saw none.\n";
    return false;
  }

  storeOut = matches[0].rootCall;
  return true;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: rendertarget_dsl_0x965B1360 <input.cso>\n";
    return 1;
  }

  ScopedCoInitialize coinit;
  LoadedDxilShader shader;
  if (!LoadShaderFromPath(argv[1], shader, false))
    return 1;

  llvm::Function *entryFunction = shader.dxilModule->GetEntryFunction();
  if (entryFunction == nullptr) {
    std::cerr << "Failed to find the DXIL entry function.\n";
    return 1;
  }

  llvm::CallInst *redStore = nullptr;
  llvm::CallInst *greenStore = nullptr;
  llvm::CallInst *blueStore = nullptr;
  if (!ExpectSingleRenderTargetStore(*entryFunction, RenderTarget(0).R(),
                                     "redStore", redStore) ||
      !ExpectSingleRenderTargetStore(*entryFunction, RenderTarget(0).G(),
                                     "greenStore", greenStore) ||
      !ExpectSingleRenderTargetStore(*entryFunction, RenderTarget(0).B(),
                                     "blueStore", blueStore)) {
    return 1;
  }

  if (redStore == greenStore || redStore == blueStore ||
      greenStore == blueStore) {
    std::cerr
        << "Render-target DSL matched the same store for multiple channels.\n";
    return 1;
  }

  std::cout
      << "Matched render target stores for SV_Target0 RGB via DSL helpers.\n";
  return 0;
}