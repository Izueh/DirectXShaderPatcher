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
  if (argc != 2) {
    std::cerr << "Usage: declarative_prefilter_recipe_0x56C468C3 <input.cso>\n";
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

  const unsigned initialFrcCount =
      CountOpMatches(*entryFunction, hlsl::OP::OpCode::Frc);
  if (initialFrcCount == 0) {
    std::cerr << "Expected test shader to contain at least one Frc call.\n";
    return 1;
  }

    const DxilCallPattern impossibleProbe =
      DxOpCall(hlsl::OP::OpCode::Frc)
        .Capture("probe_root")
        .Args({ConstantIntOperand(1, 3735928559)})
        .Build();

  DxilRecipe recipe;
    recipe.AddStep(MakePrefilterStep("skip_if_probe_missing", {impossibleProbe}));
    recipe.AddStep(MakeCustomRecipeStep(
      "should_not_execute_after_prefilter",
        [](DxilRecipeContext &context) {
          return MakeRecipeStepFailure(
              context,
              "prefilter sentinel step executed unexpectedly");
      }));

  DxilRecipeContext recipeContext;
  std::vector<uint8_t> outputContainer;
  if (!PatchDxilContainerInMemory(recipe, inputShader,
                                  outputContainer, {},
                                  &recipeContext)) {
    std::cerr << "PatchDxilContainerInMemory failed.";
    if (!recipeContext.lastError.empty())
      std::cerr << " " << recipeContext.lastError;
    std::cerr << "\n";
    return 1;
  }

  if (recipeContext.totalRuleMatches != 0) {
    std::cerr << "Expected prefilter miss to skip later rewrite steps without applying rules.\n";
    return 1;
  }

  if (!recipeContext.lastError.empty()) {
    std::cerr << "Expected prefilter miss to stop successfully, but saw error: "
              << recipeContext.lastError << "\n";
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

  const unsigned finalFrcCount =
      CountOpMatches(*patchedEntryFunction, hlsl::OP::OpCode::Frc);
  if (finalFrcCount != initialFrcCount) {
    std::cerr << "Expected prefilter miss to leave Frc count unchanged at "
              << initialFrcCount << ", but saw " << finalFrcCount << ".\n";
    return 1;
  }

  std::cout << "Declarative prefilter skipped later steps and preserved Frc count at "
            << finalFrcCount << ".\n";
  std::cout.flush();
  std::cerr.flush();
  std::_Exit(0);
}