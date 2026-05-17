#include "TestSupport.h"

#include <iostream>

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: texture2darray_add_0x965B1360 <input.cso>\n";
    return 1;
  }

  ScopedCoInitialize coinit;
  LoadedDxilShader shader;
  if (!LoadShaderForMutation(argv[1], shader, false))
    return 1;

  const size_t initialSrvCount = shader.dxilModule->GetSRVs().size();

  TextureResourceDesc textureDesc =
      TextureResourceBuilder(MakeUniqueGlobalName(*shader.module, "MyTexArray"))
          .Texture2DArray()
          .Float4()
          .Register(FindNextAvailableBinding(shader.dxilModule->GetSRVs(), 0, 0), 0)
          .Build();

  if (!AddTextureSRV(*shader.module, *shader.dxilModule, textureDesc)) {
    std::cerr << "AddTextureSRV returned false.\n";
    return 1;
  }

  if (shader.dxilModule->GetSRVs().size() != initialSrvCount + 1) {
    std::cerr << "Expected SRV count to increase from " << initialSrvCount
              << " to " << (initialSrvCount + 1) << ", but saw "
              << shader.dxilModule->GetSRVs().size() << ".\n";
    return 1;
  }

  const hlsl::DxilResource &addedSrv = *shader.dxilModule->GetSRVs().back();
  llvm::Type *addedSrvType = addedSrv.GetHLSLType();
  llvm::Type *addedSrvElementType =
      addedSrvType != nullptr && addedSrvType->isPointerTy()
          ? addedSrvType->getPointerElementType()
          : nullptr;
  if (addedSrv.GetGlobalName() != textureDesc.name) {
    std::cerr << "Expected added SRV name '" << textureDesc.name
              << "' but saw '" << addedSrv.GetGlobalName() << "'.\n";
    return 1;
  }

  if (addedSrvElementType == nullptr || !addedSrvElementType->isStructTy() ||
      addedSrvElementType->getStructName() !=
          "class.Texture2DArray<vector<float, 4> >") {
    std::cerr << "Added SRV did not use the canonical Texture2DArray<float4> type.\n";
    return 1;
  }

  if (addedSrv.GetSpaceID() != textureDesc.binding.GetSpace() ||
      addedSrv.GetLowerBound() != textureDesc.binding.GetBindPoint()) {
    std::cerr << "Added SRV binding did not match requested t"
              << textureDesc.binding.GetBindPoint() << ", space"
              << textureDesc.binding.GetSpace()
              << ".\n";
    return 1;
  }

  if (addedSrv.GetKind() != hlsl::DXIL::ResourceKind::Texture2DArray ||
      addedSrv.IsRW()) {
    std::cerr << "Added SRV did not retain expected Texture2DArray SRV shape.\n";
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

  llvm::LLVMContext patchedContext;
  std::unique_ptr<llvm::Module> patchedModule;
  hlsl::DxilModule *patchedDxilModule = nullptr;
  if (!ReloadPatchedContainer(outputContainer,
                              patchedContext,
                              patchedModule,
                              patchedDxilModule)) {
    return 1;
  }

  if (patchedDxilModule->GetSRVs().size() != initialSrvCount + 1) {
    std::cerr << "Reloaded container reported "
              << patchedDxilModule->GetSRVs().size() << " SRVs instead of "
              << (initialSrvCount + 1) << ".\n";
    return 1;
  }

  const hlsl::DxilResource *reloadedSrv = nullptr;
  if (!FindSrvByName(*patchedDxilModule, textureDesc.name, &reloadedSrv) ||
      reloadedSrv == nullptr) {
    std::cerr << "Reloaded container did not contain SRV '" << textureDesc.name
              << "'.\n";
    return 1;
  }

    if (reloadedSrv->GetLowerBound() != textureDesc.binding.GetBindPoint() ||
      reloadedSrv->GetSpaceID() != textureDesc.binding.GetSpace() ||
      reloadedSrv->GetKind() != hlsl::DXIL::ResourceKind::Texture2DArray ||
      reloadedSrv->IsRW()) {
    std::cerr << "Reloaded SRV metadata did not match the injected Texture2DArray SRV.\n";
    return 1;
  }

  llvm::Type *reloadedSrvType = reloadedSrv->GetHLSLType();
  llvm::Type *reloadedSrvElementType =
      reloadedSrvType != nullptr && reloadedSrvType->isPointerTy()
          ? reloadedSrvType->getPointerElementType()
          : nullptr;
  if (reloadedSrvElementType == nullptr ||
      !reloadedSrvElementType->isStructTy() ||
      reloadedSrvElementType->getStructName() !=
          "class.Texture2DArray<vector<float, 4> >") {
    std::cerr << "Reloaded SRV did not preserve the canonical Texture2DArray<float4> type.\n";
    return 1;
  }

  const std::string addedSrvName = reloadedSrv->GetGlobalName();
  const unsigned addedSrvBindPoint = reloadedSrv->GetLowerBound();
  const unsigned addedSrvSpace = reloadedSrv->GetSpaceID();
  const size_t finalSrvCount = patchedDxilModule->GetSRVs().size();

  patchedModule.reset();
  shader.module.reset();

  std::cout << "Added SRV '" << addedSrvName << "' at t"
            << addedSrvBindPoint << ", space" << addedSrvSpace
            << " and reloaded it successfully from the patched container"
            << " (initial SRVs=" << initialSrvCount << ", final SRVs="
            << finalSrvCount << ")\n";
  return 0;
}
