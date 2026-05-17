#include "TestSupport.h"

#include <cstdlib>
#include <cstddef>
#include <iostream>

namespace {

struct ISFastFrameConstantsCpu {
  uint32_t FrameIndex;
  uint32_t Padding[3];
};

static std::string BuildDefaultPatchedOutputPath(const std::string &inputPath) {
  const size_t extensionPos = inputPath.rfind(".cso");
  if (extensionPos != std::string::npos)
    return inputPath.substr(0, extensionPos) + ".isfast.patched.cso";

  return inputPath + ".isfast.patched.cso";
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2 && argc != 3) {
    std::cerr << "Usage: gatherforeground_isfast_0xAD818E14 <input.cso> [output.cso]\n"
              << "If [output.cso] is omitted, the test writes "
              << "<input>.isfast.patched.cso next to the input shader.\n";
    return 1;
  }

  const std::string outputPath =
      argc == 3 ? std::string(argv[2])
                : BuildDefaultPatchedOutputPath(argv[1]);

  ScopedCoInitialize coinit;
  LoadedDxilShader shader;
  if (!LoadShaderForMutation(argv[1], shader, true))
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
    std::cerr << "The test shader did not contain any IGN or BlueNoise patterns.\n";
    return 1;
  }

  TextureResourceDesc noiseTextureDesc =
      TextureResourceBuilder("FASTNoiseTexture")
          .Texture2DArray()
          .Float2()
          .Register(0, 50)
          .Build();

  CBufferSchema frameIndexSchema =
      CBufferSchemaBuilder<ISFastFrameConstantsCpu>("ISFastFrameConstants")
          .UInt("FrameIndex",
                static_cast<unsigned>(offsetof(ISFastFrameConstantsCpu, FrameIndex)))
          .UInt3("Padding",
                 static_cast<unsigned>(offsetof(ISFastFrameConstantsCpu, Padding)))
          .Build();

  CBufferDesc frameIndexCBufferDesc;
  frameIndexCBufferDesc.name = "ISFastFrameConstantsCB";
    frameIndexCBufferDesc.binding.Set(
      FindNextAvailableBinding(shader.dxilModule->GetCBuffers(), 0, 1),
      0,
      hlsl::DXIL::ResourceClass::CBuffer);
  frameIndexCBufferDesc.sizeInBytes =
      static_cast<unsigned>(sizeof(ISFastFrameConstantsCpu));
  frameIndexCBufferDesc.schema = &frameIndexSchema;

  if (!AddTextureSRV(*shader.module, *shader.dxilModule, noiseTextureDesc)) {
    std::cerr << "AddTextureSRV returned false for FASTNoiseTexture.\n";
    return 1;
  }

  if (!AddCBuffer(*shader.module, *shader.dxilModule, frameIndexCBufferDesc)) {
    std::cerr << "AddCBuffer returned false for ISFastFrameConstantsCB.\n";
    return 1;
  }

  if (!ReplaceIgnNoiseInComputeShaderWithTextureLoad(*shader.module,
                                                     *shader.dxilModule,
                                                     noiseTextureDesc,
                                                     frameIndexCBufferDesc)) {
    std::cerr << "ReplaceIgnNoiseInComputeShaderWithTextureLoad returned false.\n";
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
              << initialCBufferCount << " to "
              << (initialCBufferCount + 1) << ", but saw "
              << shader.dxilModule->GetCBuffers().size() << ".\n";
    return 1;
  }

  if (CountIgnNoiseChains(*entryFunction) != 0) {
    std::cerr << "Expected all IGN noise chains to be replaced.\n";
    return 1;
  }

  if (CountBlueNoiseTextureLoads(*entryFunction, *shader.dxilModule) != 0) {
    std::cerr << "Expected all BlueNoise texture loads to be replaced.\n";
    return 1;
  }

  if (CountDxOpCalls(*entryFunction, "dx.op.textureLoad.f32") !=
      initialTextureLoadCount + initialIgnCount) {
    std::cerr << "Expected textureLoad.f32 count to increase only for IGN replacements; BlueNoise rewrites should replace in place.\n";
    return 1;
  }

  const hlsl::DxilResource *addedSrv = nullptr;
  if (!FindSrvByName(*shader.dxilModule, noiseTextureDesc.name, &addedSrv) ||
      addedSrv == nullptr) {
    std::cerr << "Injected FASTNoiseTexture SRV was not present after mutation.\n";
    return 1;
  }

  llvm::Type *addedSrvType = addedSrv->GetHLSLType();
  llvm::Type *addedSrvElementType =
      addedSrvType != nullptr && addedSrvType->isPointerTy()
          ? addedSrvType->getPointerElementType()
          : nullptr;
    if (addedSrv->GetLowerBound() != noiseTextureDesc.binding.GetBindPoint() ||
      addedSrv->GetSpaceID() != noiseTextureDesc.binding.GetSpace() ||
      addedSrv->GetKind() != hlsl::DXIL::ResourceKind::Texture2DArray ||
      addedSrv->IsRW() || addedSrvElementType == nullptr ||
      !addedSrvElementType->isStructTy() ||
      addedSrvElementType->getStructName() !=
          "class.Texture2DArray<vector<float, 2> >") {
    std::cerr << "Injected FASTNoiseTexture metadata did not match the requested Texture2DArray<float2> SRV.\n";
    return 1;
  }

  const hlsl::DxilCBuffer *addedCBuffer = nullptr;
  if (!FindCBufferByName(*shader.dxilModule,
                         frameIndexCBufferDesc.name,
                         &addedCBuffer) ||
      addedCBuffer == nullptr) {
    std::cerr << "Injected ISFastFrameConstantsCB cbuffer was not present after mutation.\n";
    return 1;
  }

    if (addedCBuffer->GetLowerBound() != frameIndexCBufferDesc.binding.GetBindPoint() ||
      addedCBuffer->GetSpaceID() != frameIndexCBufferDesc.binding.GetSpace() ||
      addedCBuffer->GetSize() != sizeof(ISFastFrameConstantsCpu)) {
    std::cerr << "Injected frame index cbuffer metadata did not match the requested schema.\n";
    return 1;
  }

  RefreshDxilAfterResourceMutation(*shader.dxilModule);
  if (!VerifyModuleOrReport(*shader.module))
    return 1;

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
  if (!ReloadPatchedContainer(outputContainer,
                              patchedContext,
                              patchedModule,
                              patchedDxilModule)) {
    return 1;
  }

  if (patchedDxilModule->GetSRVs().size() != initialSrvCount + 1 ||
      patchedDxilModule->GetCBuffers().size() != initialCBufferCount + 1) {
    std::cerr << "Reloaded container did not preserve the injected SRV/cbuffer counts.\n";
    return 1;
  }

  const hlsl::DxilResource *reloadedSrv = nullptr;
  if (!FindSrvByName(*patchedDxilModule, noiseTextureDesc.name, &reloadedSrv) ||
      reloadedSrv == nullptr) {
    std::cerr << "Reloaded container did not contain FASTNoiseTexture.\n";
    return 1;
  }

  const hlsl::DxilCBuffer *reloadedCBuffer = nullptr;
  if (!FindCBufferByName(*patchedDxilModule,
                         frameIndexCBufferDesc.name,
                         &reloadedCBuffer) ||
      reloadedCBuffer == nullptr) {
    std::cerr << "Reloaded container did not contain ISFastFrameConstantsCB.\n";
    return 1;
  }

  llvm::Function *reloadedEntryFunction = patchedDxilModule->GetEntryFunction();
  if (reloadedEntryFunction == nullptr) {
    std::cerr << "Failed to locate the reloaded DXIL entry function.\n";
    return 1;
  }

  if (CountDxOpCalls(*reloadedEntryFunction, "dx.op.textureLoad.f32") !=
      initialTextureLoadCount + initialIgnCount) {
    std::cerr << "Reloaded container did not preserve the expected textureLoad.f32 count after IGN and BlueNoise replacement.\n";
    return 1;
  }

  if (CountBlueNoiseTextureLoads(*reloadedEntryFunction, *patchedDxilModule) != 0) {
    std::cerr << "Reloaded container still contains live BlueNoise texture loads.\n";
    return 1;
  }

  std::cout << "Patched shader written to: " << outputPath << "\n";
  std::cout.flush();
  std::cerr.flush();
  std::_Exit(0);
}
