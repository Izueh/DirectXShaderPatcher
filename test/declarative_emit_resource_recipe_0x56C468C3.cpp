#include "TestSupport.h"

#include <cstdlib>
#include <iostream>

namespace {

static unsigned CountGroupIdXCalls(llvm::Function &function) {
  std::vector<DxilMatchResult> matches;
  CollectDxilCallMatches(
      function,
      DxOpCall(hlsl::OP::OpCode::GroupId)
        .Args({ConstantIntOperand(1, 0)})
          .Build(),
      matches);
  return static_cast<unsigned>(matches.size());
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "Usage: declarative_emit_resource_recipe_0x56C468C3 <input.cso> <recipe.yml>\n";
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

  const unsigned initialGroupIdXCount = CountGroupIdXCalls(*entryFunction);
  const unsigned initialCBufferLoadCount =
      CountDxOpCalls(*entryFunction, "dx.op.cbufferLoadLegacy.i32");
  if (initialGroupIdXCount == 0) {
    std::cerr << "Expected test shader to contain at least one groupId.x call.\n";
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
    std::cerr << "Expected declarative emitted-call recipe to apply at least one rule.\n";
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

  const unsigned finalGroupIdXCount = CountGroupIdXCalls(*patchedEntryFunction);
  const unsigned finalCBufferLoadCount =
      CountDxOpCalls(*patchedEntryFunction, "dx.op.cbufferLoadLegacy.i32");
  if (finalGroupIdXCount >= initialGroupIdXCount) {
    std::cerr << "Expected emitted declarative rewrite to reduce groupId.x count from "
              << initialGroupIdXCount << ", but saw " << finalGroupIdXCount
              << ".\n";
    return 1;
  }

  if (finalCBufferLoadCount <= initialCBufferLoadCount) {
    std::cerr << "Expected emitted declarative rewrite to increase cbufferLoadLegacy.i32 count from "
              << initialCBufferLoadCount << ", but saw " << finalCBufferLoadCount
              << ".\n";
    return 1;
  }

  std::cout << "Declarative emitted rewrite reduced groupId.x count from "
            << initialGroupIdXCount << " to " << finalGroupIdXCount
            << " and increased cbufferLoadLegacy.i32 count from "
            << initialCBufferLoadCount << " to " << finalCBufferLoadCount
            << ".\n";
  std::cout.flush();
  std::cerr.flush();
  std::_Exit(0);
}