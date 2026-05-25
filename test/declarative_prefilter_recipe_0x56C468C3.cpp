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
  if (!LoadShaderFromPath(argv[1], shader, false))
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
      "should_not_execute_after_prefilter", [](DxilRecipeContext &context) {
        return MakeRecipeStepFailure(
            context, "prefilter sentinel step executed unexpectedly");
      }));

  DxilRecipeContext recipeContext;
  std::vector<uint8_t> outputContainer;
  if (!PatchDxilContainer(recipe, inputShader, outputContainer, {},
                          &recipeContext)) {
    std::cerr << "PatchDxilContainer failed.";
    if (!recipeContext.lastError.empty())
      std::cerr << " " << recipeContext.lastError;
    std::cerr << "\n";
    return 1;
  }

  if (recipeContext.totalRuleMatches != 0) {
    std::cerr << "Expected prefilter miss to skip later rewrite steps without "
                 "applying rules.\n";
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

  const char *matchOnlyRecipeText = R"YAML(version: 1
rewrite_rules:
  - id: match_only_frc_probe
    match:
      opcode: Frc
      capture: probe_root
      mode: None
steps:
  - kind: apply_rule
    rule: match_only_frc_probe
    mode: First
    required: true
)YAML";

  DxilRecipeParseResult matchOnlyParseResult;
  if (!ParseDxilRecipeText(matchOnlyRecipeText, matchOnlyParseResult,
                           "inline-sm6-match-only-test")) {
    std::cerr << "Failed to parse inline SM6 match-only recipe: "
              << matchOnlyParseResult.error << "\n";
    return 1;
  }

  DxilRecipeContext matchOnlyContext;
  if (!ExecuteDxilRecipe(
          matchOnlyParseResult.recipe, *shader.module, *shader.dxilModule,
          matchOnlyParseResult.patchOptions.recipeExecutionOptions,
          &matchOnlyContext)) {
    std::cerr << "ExecuteDxilRecipe failed for inline SM6 match-only recipe.";
    if (!matchOnlyContext.lastError.empty())
      std::cerr << " " << matchOnlyContext.lastError;
    std::cerr << "\n";
    return 1;
  }

  if (matchOnlyContext.totalRuleMatches == 0) {
    std::cerr
        << "Expected SM6 match-only recipe to report at least one match.\n";
    return 1;
  }

  if (matchOnlyContext.moduleModified) {
    std::cerr << "Expected SM6 match-only recipe to avoid module mutation.\n";
    return 1;
  }

  llvm::Function *finalEntryFunction = shader.dxilModule->GetEntryFunction();
  if (finalEntryFunction == nullptr) {
    std::cerr << "Failed to locate final DXIL entry function after match-only "
                 "step.\n";
    return 1;
  }

  const unsigned postMatchOnlyFrcCount =
      CountOpMatches(*finalEntryFunction, hlsl::OP::OpCode::Frc);
  if (postMatchOnlyFrcCount != initialFrcCount) {
    std::cerr << "Expected SM6 match-only recipe to preserve Frc count at "
              << initialFrcCount << ", but saw " << postMatchOnlyFrcCount
              << ".\n";
    return 1;
  }

  std::cout << "Declarative prefilter skipped later steps and SM6 match-only "
               "rules preserved Frc count at "
            << postMatchOnlyFrcCount << ".\n";
  std::cout.flush();
  std::cerr.flush();
}