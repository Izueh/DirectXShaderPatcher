#include "TestSupport.h"

#include <iostream>

namespace {

static const hlsl::DxilResource *
FindUavByName(const hlsl::DxilModule &dxilModule, const std::string &name) {
  for (const auto &uav : dxilModule.GetUAVs()) {
    if (uav != nullptr && uav->GetGlobalName() == name)
      return uav.get();
  }

  return nullptr;
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: texture_uav_add_0x965B1360 <input.cso>\n";
    return 1;
  }

  ScopedCoInitialize coinit;
  LoadedDxilShader shader;
  if (!LoadShaderFromPath(argv[1], shader, false))
    return 1;

  const size_t initialUavCount = shader.dxilModule->GetUAVs().size();

  TextureResourceDesc textureDesc =
      TextureResourceBuilder(MakeUniqueGlobalName(*shader.module, "MyRwTex"))
          .RWTexture2D()
          .Float4()
          .Register(
              FindNextAvailableBinding(shader.dxilModule->GetUAVs(), 0, 0), 0)
          .Build();

  if (!AddTextureUAV(*shader.module, *shader.dxilModule, textureDesc)) {
    std::cerr << "AddTextureUAV returned false.\n";
    return 1;
  }

  if (shader.dxilModule->GetUAVs().size() != initialUavCount + 1) {
    std::cerr << "Expected UAV count to increase from " << initialUavCount
              << " to " << (initialUavCount + 1) << ", but saw "
              << shader.dxilModule->GetUAVs().size() << ".\n";
    return 1;
  }

  const hlsl::DxilResource &addedUav = *shader.dxilModule->GetUAVs().back();
  llvm::Type *addedUavType = addedUav.GetHLSLType();
  llvm::Type *addedUavElementType =
      addedUavType != nullptr && addedUavType->isPointerTy()
          ? addedUavType->getPointerElementType()
          : nullptr;
  if (addedUav.GetGlobalName() != textureDesc.name) {
    std::cerr << "Expected added UAV name '" << textureDesc.name
              << "' but saw '" << addedUav.GetGlobalName() << "'.\n";
    return 1;
  }

  if (addedUavElementType == nullptr || !addedUavElementType->isStructTy() ||
      addedUavElementType->getStructName() !=
          "class.RWTexture2D<vector<float, 4> >") {
    std::cerr
        << "Added UAV did not use the canonical RWTexture2D<float4> type.\n";
    return 1;
  }

  if (addedUav.GetSpaceID() != textureDesc.binding.GetSpace() ||
      addedUav.GetLowerBound() != textureDesc.binding.GetBindPoint()) {
    std::cerr << "Added UAV binding did not match requested u"
              << textureDesc.binding.GetBindPoint() << ", space"
              << textureDesc.binding.GetSpace() << ".\n";
    return 1;
  }

  if (addedUav.GetKind() != hlsl::DXIL::ResourceKind::Texture2D ||
      !addedUav.IsRW()) {
    std::cerr << "Added UAV did not retain expected RW Texture2D shape.\n";
    return 1;
  }

  RefreshDxilModule(*shader.dxilModule);
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
  if (!ReloadPatchedContainer(outputContainer, patchedContext, patchedModule,
                              patchedDxilModule)) {
    return 1;
  }

  if (patchedDxilModule->GetUAVs().size() != initialUavCount + 1) {
    std::cerr << "Reloaded container reported "
              << patchedDxilModule->GetUAVs().size() << " UAVs instead of "
              << (initialUavCount + 1) << ".\n";
    return 1;
  }

  const hlsl::DxilResource *reloadedUav =
      FindUavByName(*patchedDxilModule, textureDesc.name);
  if (reloadedUav == nullptr) {
    std::cerr << "Reloaded container did not contain UAV '" << textureDesc.name
              << "'.\n";
    return 1;
  }

  if (reloadedUav->GetLowerBound() != textureDesc.binding.GetBindPoint() ||
      reloadedUav->GetSpaceID() != textureDesc.binding.GetSpace() ||
      reloadedUav->GetKind() != hlsl::DXIL::ResourceKind::Texture2D ||
      !reloadedUav->IsRW()) {
    std::cerr << "Reloaded UAV metadata did not match the injected RWTexture2D "
                 "UAV.\n";
    return 1;
  }

  llvm::Type *reloadedUavType = reloadedUav->GetHLSLType();
  llvm::Type *reloadedUavElementType =
      reloadedUavType != nullptr && reloadedUavType->isPointerTy()
          ? reloadedUavType->getPointerElementType()
          : nullptr;
  if (reloadedUavElementType == nullptr ||
      !reloadedUavElementType->isStructTy() ||
      reloadedUavElementType->getStructName() !=
          "class.RWTexture2D<vector<float, 4> >") {
    std::cerr << "Reloaded UAV did not preserve the canonical "
                 "RWTexture2D<float4> type.\n";
    return 1;
  }

  const std::string addedUavName = reloadedUav->GetGlobalName();
  const unsigned addedUavBindPoint = reloadedUav->GetLowerBound();
  const unsigned addedUavSpace = reloadedUav->GetSpaceID();
  const size_t finalUavCount = patchedDxilModule->GetUAVs().size();

  patchedModule.reset();
  shader.module.reset();

  std::cout << "Added UAV '" << addedUavName << "' at u" << addedUavBindPoint
            << ", space" << addedUavSpace
            << " and reloaded it successfully from the patched container"
            << " (initial UAVs=" << initialUavCount
            << ", final UAVs=" << finalUavCount << ")\n";
  return 0;
}