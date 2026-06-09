#include "../../../include/dxp/sm6/Resources.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Type.h"

#include "dxc/DXIL/DxilCBuffer.h"
#include "dxc/DXIL/DxilCompType.h"
#include "dxc/DXIL/DxilConstants.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilResource.h"
#include "dxc/DXIL/DxilSampler.h"
#include "dxc/DXIL/DxilTypeSystem.h"

using llvm::GlobalVariable;
using llvm::Module;

namespace {

static const char *GetTextureElementTypeName(hlsl::DXIL::ComponentType kind) {
  switch (kind) {
  case hlsl::DXIL::ComponentType::F32:
    return "float";
  case hlsl::DXIL::ComponentType::U32:
    return "uint";
  case hlsl::DXIL::ComponentType::I32:
    return "int";
  case hlsl::DXIL::ComponentType::Invalid:
    return "invalid";
  default:
    return "invalid";
  }
}

static llvm::Type *GetTextureElementScalarType(llvm::LLVMContext &ctx,
                                               hlsl::DXIL::ComponentType kind) {
  switch (kind) {
  case hlsl::DXIL::ComponentType::F32:
    return llvm::Type::getFloatTy(ctx);
  case hlsl::DXIL::ComponentType::U32:
  case hlsl::DXIL::ComponentType::I32:
    return llvm::Type::getInt32Ty(ctx);
  case hlsl::DXIL::ComponentType::Invalid:
    return nullptr;
  default:
    return nullptr;
  }
}

static hlsl::CompType GetTextureCompType(hlsl::DXIL::ComponentType kind) {
  return hlsl::CompType(kind);
}

static bool ValidateTextureResourceDesc(const TextureResourceDesc &desc,
                                        std::string &errorMessage) {
  if (desc.name.empty()) {
    errorMessage = "Texture resource name must not be empty.";
    return false;
  }

  switch (desc.binding.GetResourceClass()) {
  case hlsl::DXIL::ResourceClass::SRV:
    if (desc.isReadWrite) {
      errorMessage = "Texture SRV descriptors must not be marked read-write.";
      return false;
    }
    break;
  case hlsl::DXIL::ResourceClass::UAV:
    if (!desc.isReadWrite) {
      errorMessage = "Texture UAV descriptors must be marked read-write.";
      return false;
    }
    break;
  default:
    errorMessage =
        "Texture resources currently support SRV and UAV binding classes only.";
    return false;
  }

  if (desc.vectorWidth < 1 || desc.vectorWidth > 4) {
    errorMessage = "Texture resource vector width must be between 1 and 4.";
    return false;
  }

  switch (desc.kind) {
  case hlsl::DXIL::ResourceKind::Texture2D:
  case hlsl::DXIL::ResourceKind::Texture2DArray:
    break;
  default:
    errorMessage = "TextureResourceBuilder currently supports Texture2D and "
                   "Texture2DArray only.";
    return false;
  }

  switch (desc.elementKind) {
  case hlsl::DXIL::ComponentType::F32:
  case hlsl::DXIL::ComponentType::U32:
  case hlsl::DXIL::ComponentType::I32:
    break;
  default:
    errorMessage = "TextureResourceBuilder currently supports F32, U32, and "
                   "I32 element types only.";
    return false;
  }

  return true;
}

static std::string
GetTextureElementTypeDisplayName(const TextureResourceDesc &desc) {
  const std::string scalarName = GetTextureElementTypeName(desc.elementKind);
  if (desc.vectorWidth == 1)
    return scalarName;

  return "vector<" + scalarName + ", " + std::to_string(desc.vectorWidth) + ">";
}

static std::string GetTextureTypeName(const TextureResourceDesc &desc) {
  switch (desc.kind) {
  case hlsl::DXIL::ResourceKind::Texture2D:
    return desc.isReadWrite ? "class.RWTexture2D<" +
                                  GetTextureElementTypeDisplayName(desc) + " >"
                            : "class.Texture2D<" +
                                  GetTextureElementTypeDisplayName(desc) + " >";
  case hlsl::DXIL::ResourceKind::Texture2DArray:
    return desc.isReadWrite ? "class.RWTexture2DArray<" +
                                  GetTextureElementTypeDisplayName(desc) + " >"
                            : "class.Texture2DArray<" +
                                  GetTextureElementTypeDisplayName(desc) + " >";
  case hlsl::DXIL::ResourceKind::Invalid:
    return "invalid";
  default:
    return "invalid";
  }
}

static std::string GetTextureMipsTypeName(const TextureResourceDesc &desc) {
  if (desc.isReadWrite)
    return std::string();
  return GetTextureTypeName(desc) + "::mips_type";
}

static llvm::Type *GetTextureElementType(llvm::LLVMContext &ctx,
                                         const TextureResourceDesc &desc) {
  llvm::Type *scalarType = GetTextureElementScalarType(ctx, desc.elementKind);
  if (scalarType == nullptr)
    return nullptr;
  if (desc.vectorWidth == 1)
    return scalarType;

  return llvm::VectorType::get(scalarType, desc.vectorWidth);
}

static unsigned GetCBufferFieldVectorSize(const CBufferFieldDesc &field) {
  return field.vectorSize;
}

static hlsl::CompType::Kind
GetCBufferFieldCompKind(const CBufferFieldDesc &field) {
  return field.compType;
}

static unsigned GetCBufferFieldScalarSize(hlsl::CompType::Kind compType) {
  switch (compType) {
  case hlsl::DXIL::ComponentType::F32:
  case hlsl::DXIL::ComponentType::U32:
  case hlsl::DXIL::ComponentType::I32:
    return 4;
  default:
    return 0;
  }
}

static unsigned GetCBufferFieldSize(const CBufferFieldDesc &field) {
  return GetCBufferFieldScalarSize(field.compType) *
         GetCBufferFieldVectorSize(field);
}

static llvm::Type *GetCBufferFieldType(llvm::LLVMContext &ctx,
                                       const CBufferFieldDesc &field) {
  llvm::Type *scalarType = nullptr;
  switch (field.compType) {
  case hlsl::DXIL::ComponentType::F32:
    scalarType = llvm::Type::getFloatTy(ctx);
    break;
  case hlsl::DXIL::ComponentType::U32:
  case hlsl::DXIL::ComponentType::I32:
    scalarType = llvm::Type::getInt32Ty(ctx);
    break;
  default:
    return nullptr;
  }

  const unsigned vectorSize = GetCBufferFieldVectorSize(field);
  if (vectorSize == 1)
    return scalarType;

  return llvm::VectorType::get(scalarType, vectorSize);
}

static bool ValidateCBufferSchema(const CBufferSchema &schema,
                                  std::string &errorMessage) {
  if (schema.typeName.empty()) {
    errorMessage = "CBuffer schema type name must not be empty.";
    return false;
  }

  if (schema.sizeInBytes == 0) {
    errorMessage = "CBuffer schema size must be non-zero.";
    return false;
  }

  unsigned previousOffset = 0;
  bool havePreviousField = false;
  for (const CBufferFieldDesc &field : schema.fields) {
    if (field.name.empty()) {
      errorMessage = "CBuffer schema field name must not be empty.";
      return false;
    }

    switch (field.compType) {
    case hlsl::DXIL::ComponentType::F32:
    case hlsl::DXIL::ComponentType::U32:
    case hlsl::DXIL::ComponentType::I32:
      break;
    default:
      errorMessage = "CBuffer schema currently supports F32, U32, and I32 "
                     "field types only.";
      return false;
    }

    if (field.vectorSize < 1 || field.vectorSize > 4) {
      errorMessage =
          "CBuffer schema field vector size must be between 1 and 4.";
      return false;
    }

    const unsigned fieldSize = GetCBufferFieldSize(field);
    if (field.offset + fieldSize > schema.sizeInBytes) {
      errorMessage =
          "CBuffer schema field exceeds declared struct size: " + field.name;
      return false;
    }

    if (havePreviousField && field.offset < previousOffset) {
      errorMessage =
          "CBuffer schema fields must be declared in ascending offset order.";
      return false;
    }

    previousOffset = field.offset;
    havePreviousField = true;
  }

  return true;
}

static void BuildCBufferSchemaLayout(const CBufferSchema &schema,
                                     llvm::LLVMContext &ctx,
                                     std::vector<llvm::Type *> &elementTypes,
                                     std::vector<int> &fieldMapping) {
  elementTypes.clear();
  fieldMapping.clear();

  unsigned currentOffset = 0;
  for (size_t fieldIndex = 0; fieldIndex < schema.fields.size(); ++fieldIndex) {
    const CBufferFieldDesc &field = schema.fields[fieldIndex];
    if (field.offset > currentOffset) {
      elementTypes.push_back(llvm::ArrayType::get(
          llvm::Type::getInt8Ty(ctx), field.offset - currentOffset));
      fieldMapping.push_back(-1);
      currentOffset = field.offset;
    }

    elementTypes.push_back(GetCBufferFieldType(ctx, field));
    fieldMapping.push_back(static_cast<int>(fieldIndex));
    currentOffset += GetCBufferFieldSize(field);
  }

  if (schema.sizeInBytes > currentOffset) {
    elementTypes.push_back(llvm::ArrayType::get(
        llvm::Type::getInt8Ty(ctx), schema.sizeInBytes - currentOffset));
    fieldMapping.push_back(-1);
  }
}

static bool
CanUseUnpackedCBufferLayout(const llvm::DataLayout &dataLayout,
                            const CBufferSchema &schema,
                            const std::vector<llvm::Type *> &elementTypes,
                            const std::vector<int> &fieldMapping) {
  if (elementTypes.empty())
    return false;

  llvm::StructType *layoutProbe = llvm::StructType::get(
      elementTypes.front()->getContext(), elementTypes, false);
  const llvm::StructLayout *structLayout =
      dataLayout.getStructLayout(layoutProbe);
  if (structLayout->getSizeInBytes() != schema.sizeInBytes)
    return false;

  for (size_t elementIndex = 0; elementIndex < fieldMapping.size();
       ++elementIndex) {
    const int schemaFieldIndex = fieldMapping[elementIndex];
    if (schemaFieldIndex < 0)
      continue;

    const uint64_t actualOffset = structLayout->getElementOffset(elementIndex);
    const unsigned expectedOffset =
        schema.fields[static_cast<size_t>(schemaFieldIndex)].offset;
    if (actualOffset != expectedOffset)
      return false;
  }

  return true;
}

template <typename TResource>
static bool
HasBindingConflict(const std::vector<std::unique_ptr<TResource>> &resources,
                   unsigned space, unsigned bindPoint) {
  for (const auto &resource : resources) {
    if (resource->GetSpaceID() == space &&
        resource->GetLowerBound() == bindPoint)
      return true;
  }

  return false;
}

static bool HasGlobalNameConflict(const Module &module,
                                  const std::string &name) {
  return module.getNamedValue(name) != nullptr;
}

static void KeepGlobalAlive(hlsl::DxilModule &dxilModule,
                            llvm::GlobalVariable *globalVariable) {
  auto &llvmUsed = dxilModule.GetLLVMUsed();

  if (std::find(llvmUsed.begin(), llvmUsed.end(), globalVariable) ==
      llvmUsed.end())
    llvmUsed.push_back(globalVariable);
}

static llvm::StructType *GetOrCreateCBufferType(llvm::Module &module,
                                                const CBufferDesc &desc) {
  llvm::LLVMContext &ctx = module.getContext();
  llvm::StructType *cbufferStructType = module.getTypeByName(desc.name + "_t");

  if (!cbufferStructType)
    cbufferStructType = llvm::StructType::create(ctx, desc.name + "_t");

  if (cbufferStructType->isOpaque()) {
    const unsigned dwordCount = std::max(1u, ((desc.sizeInBytes + 3u) / 4u));
    llvm::ArrayType *payloadType =
        llvm::ArrayType::get(llvm::Type::getInt32Ty(ctx), dwordCount);
    llvm::Type *const elements[] = {payloadType};
    cbufferStructType->setBody(elements, false);
  }

  return cbufferStructType;
}

static llvm::StructType *
GetOrCreateCBufferSchemaType(llvm::Module &module,
                             const CBufferSchema &schema) {
  llvm::LLVMContext &ctx = module.getContext();
  llvm::StructType *cbufferStructType = module.getTypeByName(schema.typeName);

  if (!cbufferStructType)
    cbufferStructType = llvm::StructType::create(ctx, schema.typeName);

  if (cbufferStructType->isOpaque()) {
    std::vector<llvm::Type *> elementTypes;
    std::vector<int> fieldMapping;
    BuildCBufferSchemaLayout(schema, ctx, elementTypes, fieldMapping);
    const bool packed = !CanUseUnpackedCBufferLayout(
        module.getDataLayout(), schema, elementTypes, fieldMapping);
    cbufferStructType->setBody(elementTypes, packed);
  }

  return cbufferStructType;
}

static llvm::StructType *GetOrCreateMarkerStructType(llvm::Module &module,
                                                     const std::string &name) {
  llvm::LLVMContext &ctx = module.getContext();
  llvm::StructType *structType = module.getTypeByName(name);
  if (!structType)
    structType = llvm::StructType::create(ctx, name);

  if (structType->isOpaque()) {
    llvm::Type *const elements[] = {llvm::Type::getInt32Ty(ctx)};
    structType->setBody(elements, false);
  }

  return structType;
}

static llvm::StructType *
GetOrCreateTextureType(llvm::Module &module, const TextureResourceDesc &desc) {
  llvm::LLVMContext &ctx = module.getContext();
  const std::string textureTypeName = GetTextureTypeName(desc);
  llvm::Type *elementType = GetTextureElementType(ctx, desc);
  if (elementType == nullptr)
    return nullptr;

  llvm::StructType *textureType = module.getTypeByName(textureTypeName);
  if (!textureType)
    textureType = llvm::StructType::create(ctx, textureTypeName);
  if (textureType->isOpaque()) {
    if (desc.isReadWrite) {
      llvm::Type *const textureElements[] = {elementType};
      textureType->setBody(textureElements, false);
    } else {
      const std::string mipsTypeName = GetTextureMipsTypeName(desc);
      llvm::StructType *mipsType = module.getTypeByName(mipsTypeName);
      if (!mipsType)
        mipsType = llvm::StructType::create(ctx, mipsTypeName);
      if (mipsType->isOpaque()) {
        llvm::Type *const mipsElements[] = {llvm::Type::getInt32Ty(ctx)};
        mipsType->setBody(mipsElements, false);
      }

      llvm::Type *const textureElements[] = {elementType, mipsType};
      textureType->setBody(textureElements, false);
    }
  }

  return textureType;
}

static llvm::StructType *
GetOrCreateResolvedCBufferType(llvm::Module &module, const CBufferDesc &desc) {
  return desc.schema != nullptr
             ? GetOrCreateCBufferSchemaType(module, *desc.schema)
             : GetOrCreateCBufferType(module, desc);
}

static llvm::Constant *CreateCBufferSymbol(llvm::Module &module,
                                           const CBufferDesc &desc) {
  llvm::StructType *cbufferStructType =
      GetOrCreateResolvedCBufferType(module, desc);
  return llvm::UndefValue::get(cbufferStructType->getPointerTo());
}

static llvm::Constant *CreateTextureSymbol(llvm::Module &module,
                                           const TextureResourceDesc &desc) {
  llvm::StructType *textureType = GetOrCreateTextureType(module, desc);
  if (!textureType)
    return nullptr;
  return llvm::UndefValue::get(textureType->getPointerTo());
}

static GlobalVariable *EnsureSamplerGlobal(Module &module,
                                           const SamplerDesc &desc) {
  llvm::StructType *samplerType =
      GetOrCreateMarkerStructType(module, desc.name + "_Sampler_t");

  llvm::GlobalVariable *globalVariable = module.getGlobalVariable(desc.name);
  if (!globalVariable) {
    globalVariable = new llvm::GlobalVariable(
        module, samplerType, false, llvm::GlobalValue::ExternalLinkage, nullptr,
        desc.name);
  }

  return globalVariable;
}

static void MaybeAnnotateCBufferType(hlsl::DxilModule &dxilModule,
                                     llvm::StructType *cbufferStructType,
                                     unsigned sizeInBytes) {
  if (!cbufferStructType || cbufferStructType->getNumElements() != 1)
    return;

  hlsl::DxilTypeSystem &typeSystem = dxilModule.GetTypeSystem();
  hlsl::DxilStructAnnotation *structAnnotation =
      typeSystem.GetStructAnnotation(cbufferStructType);

  if (!structAnnotation)
    structAnnotation = typeSystem.AddStructAnnotation(cbufferStructType);

  structAnnotation->SetCBufferSize(sizeInBytes);

  hlsl::DxilFieldAnnotation &dataField =
      structAnnotation->GetFieldAnnotation(0);
  dataField.SetFieldName("Data");
  dataField.SetCBufferOffset(0);
  dataField.SetCompType(hlsl::CompType::getU32().GetKind());
  dataField.SetCBVarUsed(true);

  typeSystem.FinishStructAnnotation(*structAnnotation);
}

static void MaybeAnnotateCBufferType(hlsl::DxilModule &dxilModule,
                                     llvm::StructType *cbufferStructType,
                                     const CBufferSchema &schema) {
  if (!cbufferStructType)
    return;

  std::vector<llvm::Type *> elementTypes;
  std::vector<int> fieldMapping;
  BuildCBufferSchemaLayout(schema, cbufferStructType->getContext(),
                           elementTypes, fieldMapping);

  hlsl::DxilTypeSystem &typeSystem = dxilModule.GetTypeSystem();
  hlsl::DxilStructAnnotation *structAnnotation =
      typeSystem.GetStructAnnotation(cbufferStructType);
  if (!structAnnotation)
    structAnnotation = typeSystem.AddStructAnnotation(cbufferStructType);

  structAnnotation->SetCBufferSize(schema.sizeInBytes);

  for (size_t fieldIndex = 0; fieldIndex < fieldMapping.size(); ++fieldIndex) {
    hlsl::DxilFieldAnnotation &fieldAnnotation =
        structAnnotation->GetFieldAnnotation(static_cast<unsigned>(fieldIndex));
    const int schemaFieldIndex = fieldMapping[fieldIndex];
    if (schemaFieldIndex < 0)
      continue;

    const CBufferFieldDesc &field =
        schema.fields[static_cast<size_t>(schemaFieldIndex)];
    fieldAnnotation.SetFieldName(field.name);
    fieldAnnotation.SetCBufferOffset(field.offset);
    fieldAnnotation.SetCompType(GetCBufferFieldCompKind(field));
    fieldAnnotation.SetCBVarUsed(true);

    const unsigned vectorSize = GetCBufferFieldVectorSize(field);
    if (vectorSize > 1)
      fieldAnnotation.SetVectorSize(vectorSize);
  }

  typeSystem.FinishStructAnnotation(*structAnnotation);
}

static void MaybeAnnotateSamplerType(hlsl::DxilModule &dxilModule,
                                     GlobalVariable *globalVariable) {
  (void)dxilModule;
  (void)globalVariable;
}

static void TraceResourceMessage(bool traceEnabled, const char *message) {
  if (traceEnabled)
    std::cerr << "[trace] " << message << "\n";
}

}

std::string MakeUniqueGlobalName(const Module &module,
                                 const std::string &baseName) {
  if (!HasGlobalNameConflict(module, baseName))
    return baseName;

  for (unsigned suffix = 1; suffix != 0; ++suffix) {
    const std::string candidate = baseName + std::to_string(suffix);
    if (!HasGlobalNameConflict(module, candidate))
      return candidate;
  }

  return baseName;
}

bool AddCBuffer(Module &module, hlsl::DxilModule &dxilModule,
                const CBufferDesc &desc) {
  const unsigned sizeInBytes =
      desc.schema != nullptr ? desc.schema->sizeInBytes : desc.sizeInBytes;

  if (desc.name.empty() || sizeInBytes == 0) {
    std::cerr << "Refusing to add cbuffer with empty name or zero size.\n";
    return false;
  }

  if (desc.schema != nullptr) {
    std::string schemaError;
    if (!ValidateCBufferSchema(*desc.schema, schemaError)) {
      std::cerr << schemaError << "\n";
      return false;
    }

    if (desc.sizeInBytes != 0 && desc.sizeInBytes != desc.schema->sizeInBytes) {
      std::cerr
          << "CBuffer schema size does not match the requested buffer size.\n";
      return false;
    }
  }

  if (HasGlobalNameConflict(module, desc.name) ||
      HasBindingConflict(dxilModule.GetCBuffers(), desc.binding.GetSpace(),
                         desc.binding.GetBindPoint())) {
    std::cerr << "CBuffer name or binding already exists: " << desc.name
              << "\n";
    return false;
  }

  llvm::StructType *cbufferStructType =
      GetOrCreateResolvedCBufferType(module, desc);
  llvm::Constant *symbol = CreateCBufferSymbol(module, desc);
  if (desc.schema != nullptr)
    MaybeAnnotateCBufferType(dxilModule, cbufferStructType, *desc.schema);
  else
    MaybeAnnotateCBufferType(dxilModule, cbufferStructType, sizeInBytes);

  std::unique_ptr<hlsl::DxilCBuffer> cbuffer =
      std::make_unique<hlsl::DxilCBuffer>();
  cbuffer->SetGlobalSymbol(symbol);
  cbuffer->SetGlobalName(desc.name);
  cbuffer->SetHLSLType(symbol->getType());
  cbuffer->SetSpaceID(desc.binding.GetSpace());
  cbuffer->SetLowerBound(desc.binding.GetBindPoint());
  cbuffer->SetRangeSize(1);
  cbuffer->SetID(static_cast<unsigned>(dxilModule.GetCBuffers().size()));
  cbuffer->SetSize(sizeInBytes);

  dxilModule.AddCBuffer(std::move(cbuffer));
  return true;
}

bool AddTextureSRV(Module &module, hlsl::DxilModule &dxilModule,
                   const TextureResourceDesc &desc) {
  if (desc.name.empty()) {
    std::cerr << "Refusing to add texture SRV with an empty name.\n";
    return false;
  }

  std::string validationError;
  if (!ValidateTextureResourceDesc(desc, validationError)) {
    std::cerr << validationError << "\n";
    return false;
  }

  if (HasGlobalNameConflict(module, desc.name) ||
      HasBindingConflict(dxilModule.GetSRVs(), desc.binding.GetSpace(),
                         desc.binding.GetBindPoint())) {
    std::cerr << "Texture SRV name or binding already exists: " << desc.name
              << "\n";
    return false;
  }

  llvm::Constant *symbol = CreateTextureSymbol(module, desc);
  if (!symbol) {
    std::cerr << "Failed to create a canonical texture symbol for " << desc.name
              << ".\n";
    return false;
  }

  std::unique_ptr<hlsl::DxilResource> texture =
      std::make_unique<hlsl::DxilResource>();
  texture->SetGlobalSymbol(symbol);
  texture->SetGlobalName(desc.name);
  texture->SetHLSLType(symbol->getType());
  texture->SetSpaceID(desc.binding.GetSpace());
  texture->SetLowerBound(desc.binding.GetBindPoint());
  texture->SetRangeSize(1);
  texture->SetID(static_cast<unsigned>(dxilModule.GetSRVs().size()));
  texture->SetKind(desc.kind);
  texture->SetCompType(GetTextureCompType(desc.elementKind));
  texture->SetSampleCount(0);
  texture->SetRW(desc.isReadWrite);

  dxilModule.AddSRV(std::move(texture));
  return true;
}

bool AddTexture2DSRV(Module &module, hlsl::DxilModule &dxilModule,
                     const TextureResourceDesc &desc) {
  return AddTextureSRV(module, dxilModule, desc);
}

bool AddTextureUAV(Module &module, hlsl::DxilModule &dxilModule,
                   const TextureResourceDesc &desc) {
  if (desc.name.empty()) {
    std::cerr << "Refusing to add texture UAV with an empty name.\n";
    return false;
  }

  std::string validationError;
  if (!ValidateTextureResourceDesc(desc, validationError)) {
    std::cerr << validationError << "\n";
    return false;
  }

  if (HasGlobalNameConflict(module, desc.name) ||
      HasBindingConflict(dxilModule.GetUAVs(), desc.binding.GetSpace(),
                         desc.binding.GetBindPoint())) {
    std::cerr << "Texture UAV name or binding already exists: " << desc.name
              << "\n";
    return false;
  }

  llvm::Constant *symbol = CreateTextureSymbol(module, desc);
  if (!symbol) {
    std::cerr << "Failed to create a canonical texture symbol for " << desc.name
              << ".\n";
    return false;
  }

  std::unique_ptr<hlsl::DxilResource> texture =
      std::make_unique<hlsl::DxilResource>();
  texture->SetGlobalSymbol(symbol);
  texture->SetGlobalName(desc.name);
  texture->SetHLSLType(symbol->getType());
  texture->SetSpaceID(desc.binding.GetSpace());
  texture->SetLowerBound(desc.binding.GetBindPoint());
  texture->SetRangeSize(1);
  texture->SetID(static_cast<unsigned>(dxilModule.GetUAVs().size()));
  texture->SetKind(desc.kind);
  texture->SetCompType(GetTextureCompType(desc.elementKind));
  texture->SetSampleCount(0);
  texture->SetRW(true);

  dxilModule.AddUAV(std::move(texture));
  return true;
}

bool AddSampler(Module &module, hlsl::DxilModule &dxilModule,
                const SamplerDesc &desc) {
  if (desc.name.empty()) {
    std::cerr << "Refusing to add sampler with an empty name.\n";
    return false;
  }

  if (HasGlobalNameConflict(module, desc.name) ||
      HasBindingConflict(dxilModule.GetSamplers(), desc.binding.GetSpace(),
                         desc.binding.GetBindPoint())) {
    std::cerr << "Sampler name or binding already exists: " << desc.name
              << "\n";
    return false;
  }

  GlobalVariable *globalVariable = EnsureSamplerGlobal(module, desc);
  KeepGlobalAlive(dxilModule, globalVariable);
  MaybeAnnotateSamplerType(dxilModule, globalVariable);

  std::unique_ptr<hlsl::DxilSampler> sampler =
      std::make_unique<hlsl::DxilSampler>();
  sampler->SetGlobalSymbol(globalVariable);
  sampler->SetGlobalName(desc.name);
  sampler->SetHLSLType(globalVariable->getType());
  sampler->SetSpaceID(desc.binding.GetSpace());
  sampler->SetLowerBound(desc.binding.GetBindPoint());
  sampler->SetRangeSize(1);
  sampler->SetID(static_cast<unsigned>(dxilModule.GetSamplers().size()));
  sampler->SetKind(hlsl::DXIL::ResourceKind::Sampler);
  sampler->SetSamplerKind(hlsl::DXIL::SamplerKind::Default);

  dxilModule.AddSampler(std::move(sampler));
  return true;
}

void RefreshDxilModule(hlsl::DxilModule &dxilModule, bool traceEnabled) {
  TraceResourceMessage(traceEnabled, "refresh: refresh op cache");
  hlsl::OP *op = dxilModule.GetOP();
  if (op)
    op->RefreshCache();

  TraceResourceMessage(traceEnabled, "refresh: emit llvm.used");
  dxilModule.EmitLLVMUsed();

  TraceResourceMessage(traceEnabled, "refresh: re-emit resources");
  dxilModule.ReEmitDxilResources();

  TraceResourceMessage(traceEnabled, "refresh: collect shader flags");
  dxilModule.CollectShaderFlagsForModule();
  TraceResourceMessage(traceEnabled, "refresh: upgrade validator version");
  dxilModule.UpgradeToMinValidatorVersion();
  TraceResourceMessage(traceEnabled, "refresh: update validator metadata");
  dxilModule.UpdateValidatorVersionMetadata();

  TraceResourceMessage(traceEnabled, "refresh: clear llvm.used");
  dxilModule.ClearLLVMUsed();

  TraceResourceMessage(traceEnabled, "refresh: refresh op cache (post-clear)");
  op = dxilModule.GetOP();
  if (op)
    op->RefreshCache();
}