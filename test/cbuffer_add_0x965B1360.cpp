#include "TestSupport.h"

#include <cstddef>
#include <iostream>

namespace {

struct AddedGlobalsCpu {
  float tint[4];
  float exposure;
  uint32_t mode;
  uint32_t padding[2];
};

static bool ValidateAddedGlobalsAnnotation(const hlsl::DxilModule &dxilModule,
                                           const hlsl::DxilCBuffer &cbuffer,
                                           std::string &errorMessage) {
  llvm::Type *cbufferType = cbuffer.GetHLSLType();
  llvm::Type *elementType =
      cbufferType != nullptr && cbufferType->isPointerTy()
          ? cbufferType->getPointerElementType()
          : nullptr;
  llvm::StructType *structType =
      llvm::dyn_cast_or_null<llvm::StructType>(elementType);
  if (structType == nullptr) {
    errorMessage = "Added cbuffer type is not a struct pointer.";
    return false;
  }

  if (structType->getStructName() != "AddedGlobals") {
    errorMessage = "Added cbuffer did not preserve the expected schema type name.";
    return false;
  }

  const hlsl::DxilStructAnnotation *annotation =
      dxilModule.GetTypeSystem().GetStructAnnotation(structType);
  if (annotation == nullptr) {
    errorMessage = "Added cbuffer struct annotation is missing.";
    return false;
  }

  if (annotation->GetCBufferSize() != sizeof(AddedGlobalsCpu)) {
    errorMessage = "Added cbuffer size annotation is incorrect.";
    return false;
  }

  if (annotation->GetNumFields() != 4) {
    errorMessage = "Added cbuffer field annotation count is incorrect.";
    return false;
  }

  if (structType->getNumElements() != 4) {
    errorMessage = "Added cbuffer struct field count is incorrect.";
    return false;
  }

  struct ExpectedFieldInfo {
    const char *name;
    unsigned offset;
    hlsl::CompType::Kind compKind;
    unsigned vectorSize;
  };

  const ExpectedFieldInfo expectedFields[] = {
      {"Tint", static_cast<unsigned>(offsetof(AddedGlobalsCpu, tint)),
       hlsl::CompType::getF32().GetKind(), 4},
      {"Exposure", static_cast<unsigned>(offsetof(AddedGlobalsCpu, exposure)),
       hlsl::CompType::getF32().GetKind(), 1},
      {"Mode", static_cast<unsigned>(offsetof(AddedGlobalsCpu, mode)),
       hlsl::CompType::getU32().GetKind(), 1},
      {"Padding", static_cast<unsigned>(offsetof(AddedGlobalsCpu, padding)),
       hlsl::CompType::getU32().GetKind(), 2},
  };

  for (unsigned fieldIndex = 0; fieldIndex < 4; ++fieldIndex) {
    const hlsl::DxilFieldAnnotation &fieldAnnotation =
        annotation->GetFieldAnnotation(fieldIndex);
    const ExpectedFieldInfo &expectedField = expectedFields[fieldIndex];
    if (!fieldAnnotation.HasFieldName() ||
        fieldAnnotation.GetFieldName() != expectedField.name) {
      errorMessage = "Added cbuffer field name annotation is incorrect.";
      return false;
    }

    if (!fieldAnnotation.HasCBufferOffset() ||
        fieldAnnotation.GetCBufferOffset() != expectedField.offset) {
      errorMessage = "Added cbuffer field offset annotation is incorrect.";
      return false;
    }

    if (!fieldAnnotation.HasCompType() ||
        fieldAnnotation.GetCompType().GetKind() != expectedField.compKind) {
      errorMessage = "Added cbuffer field component type annotation is incorrect.";
      return false;
    }

    if (!fieldAnnotation.IsCBVarUsed()) {
      errorMessage = "Added cbuffer field usage annotation is incorrect.";
      return false;
    }

    llvm::Type *fieldType = structType->getElementType(fieldIndex);
    const unsigned actualVectorSize = fieldType->isVectorTy()
                                          ? fieldType->getVectorNumElements()
                                          : 1;
    if (actualVectorSize != expectedField.vectorSize) {
      errorMessage = "Added cbuffer LLVM field type does not match the schema vector width.";
      return false;
    }

    if (fieldAnnotation.GetVectorSize() != 0 &&
        fieldAnnotation.GetVectorSize() != expectedField.vectorSize) {
      errorMessage = "Added cbuffer field vector size annotation is incorrect.";
      return false;
    }
  }

  return true;
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2 && argc != 3) {
    std::cerr << "Usage: cbuffer_add_0x965B1360 <input.cso> [output.cso]\n";
    return 1;
  }

  const char *outputPath = argc == 3 ? argv[2] : nullptr;

  ScopedCoInitialize coinit;
  LoadedDxilShader shader;
  if (!LoadShaderForMutation(argv[1], shader, true))
    return 1;

  const size_t initialCBufferCount = shader.dxilModule->GetCBuffers().size();

  CBufferSchema schema = CBufferSchemaBuilder<AddedGlobalsCpu>("AddedGlobals")
                             .Float4("Tint",
                                     static_cast<unsigned>(offsetof(AddedGlobalsCpu, tint)))
                             .Float("Exposure",
                                    static_cast<unsigned>(offsetof(AddedGlobalsCpu, exposure)))
                             .UInt("Mode",
                                   static_cast<unsigned>(offsetof(AddedGlobalsCpu, mode)))
                             .UInt2("Padding",
                                    static_cast<unsigned>(offsetof(AddedGlobalsCpu, padding)))
                             .Build();

  CBufferDesc cbufferDesc;
  cbufferDesc.name = MakeUniqueGlobalName(*shader.module, "AddedGlobalsCB");
    cbufferDesc.binding.Set(
      FindNextAvailableBinding(shader.dxilModule->GetCBuffers(), 0, 0),
      0,
      hlsl::DXIL::ResourceClass::CBuffer);
  cbufferDesc.sizeInBytes = static_cast<unsigned>(sizeof(AddedGlobalsCpu));
  cbufferDesc.schema = &schema;

  if (!AddCBuffer(*shader.module, *shader.dxilModule, cbufferDesc)) {
    std::cerr << "AddCBuffer returned false.\n";
    return 1;
  }

  if (shader.dxilModule->GetCBuffers().size() != initialCBufferCount + 1) {
    std::cerr << "Expected cbuffer count to increase from "
              << initialCBufferCount << " to " << (initialCBufferCount + 1)
              << ", but saw " << shader.dxilModule->GetCBuffers().size()
              << ".\n";
    return 1;
  }

  const hlsl::DxilCBuffer &addedCBuffer = *shader.dxilModule->GetCBuffers().back();
  if (addedCBuffer.GetGlobalName() != cbufferDesc.name ||
      addedCBuffer.GetLowerBound() != cbufferDesc.binding.GetBindPoint() ||
      addedCBuffer.GetSpaceID() != cbufferDesc.binding.GetSpace() ||
      addedCBuffer.GetSize() != sizeof(AddedGlobalsCpu)) {
    std::cerr << "Added cbuffer metadata did not match the requested schema.\n";
    return 1;
  }

  std::string annotationError;
  if (!ValidateAddedGlobalsAnnotation(*shader.dxilModule,
                                      addedCBuffer,
                                      annotationError)) {
    std::cerr << annotationError << "\n";
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

  if (outputPath != nullptr &&
      !WriteFile(outputPath, outputContainer.data(), outputContainer.size())) {
    std::cerr << "Failed to write patched container: " << outputPath << "\n";
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

  if (patchedDxilModule->GetCBuffers().size() != initialCBufferCount + 1) {
    std::cerr << "Reloaded container reported "
              << patchedDxilModule->GetCBuffers().size()
              << " cbuffers instead of " << (initialCBufferCount + 1)
              << ".\n";
    return 1;
  }

  const hlsl::DxilCBuffer *reloadedCBuffer = nullptr;
  if (!FindCBufferByName(*patchedDxilModule,
                         cbufferDesc.name,
                         &reloadedCBuffer) ||
      reloadedCBuffer == nullptr) {
    std::cerr << "Reloaded container did not contain cbuffer '"
              << cbufferDesc.name << "'.\n";
    return 1;
  }

    if (reloadedCBuffer->GetLowerBound() != cbufferDesc.binding.GetBindPoint() ||
      reloadedCBuffer->GetSpaceID() != cbufferDesc.binding.GetSpace() ||
      reloadedCBuffer->GetSize() != sizeof(AddedGlobalsCpu)) {
    std::cerr << "Reloaded cbuffer metadata did not match the injected schema.\n";
    return 1;
  }

  if (!ValidateAddedGlobalsAnnotation(*patchedDxilModule,
                                      *reloadedCBuffer,
                                      annotationError)) {
    std::cerr << annotationError << "\n";
    return 1;
  }

  const std::string reloadedCBufferName = reloadedCBuffer->GetGlobalName();
  const unsigned reloadedCBufferBindPoint = reloadedCBuffer->GetLowerBound();
  const unsigned reloadedCBufferSpace = reloadedCBuffer->GetSpaceID();
  const size_t finalCBufferCount = patchedDxilModule->GetCBuffers().size();

  std::cout << "Added cbuffer '" << reloadedCBufferName << "' at b"
            << reloadedCBufferBindPoint << ", space"
            << reloadedCBufferSpace
            << " and reloaded it successfully from the patched container"
            << " (initial cbuffers=" << initialCBufferCount
            << ", final cbuffers=" << finalCBufferCount
            << ")";

  if (outputPath != nullptr)
    std::cout << "; wrote patched container to " << outputPath;

  std::cout << "\n";
  return 0;
}
