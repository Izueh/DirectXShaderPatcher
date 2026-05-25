#include "TestSupport.h"

#include <cstddef>
#include <cstdlib>
#include <iostream>

namespace {

struct ISFastFrameConstantsCpu {
  uint32_t FrameIndex;
  uint32_t Padding[3];
};

static std::string BuildDefaultPatchedOutputPath(const std::string &inputPath) {
  return DefaultArtifactOutputPath(inputPath, ".recipe.isfast.patched.cso");
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2 && argc != 3) {
    std::cerr
        << "Usage: gatherforeground_isfast_recipe <input.cso> [output.cso]\n"
        << "If [output.cso] is omitted, the test writes into "
        << "artifacts/test-output under the repo root.\n";
    return 1;
  }

  const std::string outputPath =
      argc == 3 ? std::string(argv[2]) : BuildDefaultPatchedOutputPath(argv[1]);

  ScopedCoInitialize coinit;
  LoadedDxilShader shader;
  if (!LoadShaderFromPath(argv[1], shader, true))
    return 1;

  llvm::Function *entryFunction = shader.dxilModule->GetEntryFunction();
  if (entryFunction == nullptr) {
    std::cerr << "Failed to locate the DXIL entry function.\n";
    return 1;
  }

  const size_t initialSrvCount = shader.dxilModule->GetSRVs().size();
  const size_t initialCBufferCount = shader.dxilModule->GetCBuffers().size();
  const unsigned initialTextureLoadCount =
      CountDxOpCalls(*entryFunction, "dx.op.textureLoad.f32");
  const unsigned initialIgnCount = CountIgnNoiseChains(*entryFunction);
  const unsigned initialBlueNoiseLoadCount =
      CountBlueNoiseTextureLoads(*entryFunction, *shader.dxilModule);
  if (initialIgnCount == 0 && initialBlueNoiseLoadCount == 0) {
    std::cerr
        << "The test shader did not contain any IGN or BlueNoise patterns.\n";
    return 1;
  }

  TextureResourceDesc noiseTextureDesc =
      TextureResourceBuilder("FASTNoiseTexture")
          .Texture2DArray()
          .Float2()
          .Register(0, 50)
          .Build();
  noiseTextureDesc.binding.SetBindPoint(kDxilRecipeAutoBinding);

  CBufferSchema frameIndexSchema =
      CBufferSchemaBuilder<ISFastFrameConstantsCpu>("ISFastFrameConstants")
          .UInt("FrameIndex", static_cast<unsigned>(offsetof(
                                  ISFastFrameConstantsCpu, FrameIndex)))
          .UInt3("Padding", static_cast<unsigned>(
                                offsetof(ISFastFrameConstantsCpu, Padding)))
          .Build();

  CBufferDesc frameIndexCBufferDesc;
  frameIndexCBufferDesc.name = "ISFastFrameConstantsCB";
  frameIndexCBufferDesc.binding.Set(kDxilRecipeAutoBinding, 0,
                                    hlsl::DXIL::ResourceClass::CBuffer);
  frameIndexCBufferDesc.sizeInBytes =
      static_cast<unsigned>(sizeof(ISFastFrameConstantsCpu));
  frameIndexCBufferDesc.schema = &frameIndexSchema;

  DxilRecipe recipe;
  recipe.AddStep(MakeAddTextureStep("fast_noise", noiseTextureDesc))
      .AddStep(MakeAddCBufferStep("frame_constants", frameIndexCBufferDesc))
      .AddStep(MakeApplyComputeNoiseRewriteRulesStep(
          "replace_compute_noise", "fast_noise", "frame_constants", true))
      .AddStep(MakeExpectIgnCountStep(0))
      .AddStep(MakeExpectBlueNoiseCountStep(0))
      .AddStep(MakePruneDeadCodeStep())
      .AddStep(MakeRefreshResourcesStep());

  DxilRecipeContext recipeContext;
  if (!ExecuteDxilRecipe(recipe, *shader.module, *shader.dxilModule,
                         &recipeContext, true)) {
    std::cerr << "ExecuteDxilRecipe returned false.";
    if (!recipeContext.lastError.empty())
      std::cerr << " " << recipeContext.lastError;
    std::cerr << "\n";
    return 1;
  }

  if (recipeContext.totalRuleMatches == 0) {
    std::cerr << "Expected recipe to apply at least one rewrite rule.\n";
    return 1;
  }

  if (shader.dxilModule->GetSRVs().size() != initialSrvCount + 1) {
    std::cerr << "Expected SRV count to increase from " << initialSrvCount
              << " to " << (initialSrvCount + 1) << ", but saw "
              << shader.dxilModule->GetSRVs().size() << ".\n";
    return 1;
  }

  if (shader.dxilModule->GetCBuffers().size() != initialCBufferCount + 1) {
    std::cerr << "Expected cbuffer count to increase from "
              << initialCBufferCount << " to " << (initialCBufferCount + 1)
              << ", but saw " << shader.dxilModule->GetCBuffers().size()
              << ".\n";
    return 1;
  }

  if (CountDxOpCalls(*entryFunction, "dx.op.textureLoad.f32") !=
      initialTextureLoadCount + initialIgnCount) {
    std::cerr << "Expected textureLoad.f32 count to increase only for IGN "
                 "replacements; BlueNoise rewrites should replace in place.\n";
    return 1;
  }

  if (HasTypedHandleDxilOpOverloads(*shader.module)) {
    std::cerr << "Patched module introduced typed DXIL handle op overloads "
                 "instead of reusing the shader's existing prototypes.\n";
    return 1;
  }

  const hlsl::DxilResource *addedSrv = nullptr;
  if (!FindSrvByName(*shader.dxilModule, noiseTextureDesc.name, &addedSrv) ||
      addedSrv == nullptr) {
    std::cerr << "Injected FASTNoiseTexture SRV was not present after recipe "
                 "execution.\n";
    return 1;
  }

  const hlsl::DxilCBuffer *addedCBuffer = nullptr;
  if (!FindCBufferByName(*shader.dxilModule, frameIndexCBufferDesc.name,
                         &addedCBuffer) ||
      addedCBuffer == nullptr) {
    std::cerr << "Injected ISFastFrameConstantsCB cbuffer was not present "
                 "after recipe execution.\n";
    return 1;
  }

  std::vector<uint8_t> outputContainer;
  if (!SerializePatchedContainer(*shader.dxilModule,
                                 SerializeModuleToBitcode(*shader.module),
                                 outputContainer)) {
    std::cerr << "SerializePatchedContainer failed.\n";
    return 1;
  }

  if (!WriteFile(outputPath, outputContainer.data(), outputContainer.size())) {
    std::cerr << "Failed to write patched shader container: " << outputPath
              << "\n";
    return 1;
  }

  llvm::LLVMContext patchedContext;
  std::unique_ptr<llvm::Module> patchedModule;
  hlsl::DxilModule *patchedDxilModule = nullptr;
  if (!ReloadPatchedContainer(outputContainer, patchedContext, patchedModule,
                              patchedDxilModule)) {
    return 1;
  }

  if (patchedDxilModule->GetSRVs().size() != initialSrvCount + 1 ||
      patchedDxilModule->GetCBuffers().size() != initialCBufferCount + 1) {
    std::cerr << "Reloaded container did not preserve the injected SRV/cbuffer "
                 "counts.\n";
    return 1;
  }

  if (HasTypedHandleDxilOpOverloads(*patchedModule)) {
    std::cerr << "Patched module introduced typed DXIL handle op overloads "
                 "instead of reusing the shader's existing prototypes.\n";
    return 1;
  }

  llvm::Function *reloadedEntryFunction = patchedDxilModule->GetEntryFunction();
  if (reloadedEntryFunction == nullptr) {
    std::cerr << "Failed to locate the reloaded DXIL entry function.\n";
    return 1;
  }

  if (CountIgnNoiseChains(*reloadedEntryFunction) != 0) {
    std::cerr << "Reloaded container still contains live IGN noise chains.\n";
    return 1;
  }

  if (CountBlueNoiseTextureLoads(*reloadedEntryFunction, *patchedDxilModule) !=
      0) {
    std::cerr
        << "Reloaded container still contains live BlueNoise texture loads.\n";
    return 1;
  }

  std::cout << "Patched shader written to: " << outputPath << "\n";
  std::cout.flush();
  std::cerr.flush();
}
