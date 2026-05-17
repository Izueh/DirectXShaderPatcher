#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include <algorithm>

#include <atlbase.h>
#include <malloc.h>

#include "DxilAssemblerLib.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/Errno.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Regex.h"
#include "llvm/Support/YAMLTraits.h"
#include "llvm/Support/YAMLParser.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Local.h"

#include "dxc/DXIL/DxilCBuffer.h"
#include "dxc/DXIL/DxilConstants.h"
#include "dxc/DXIL/DxilMetadataHelper.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilResource.h"
#include "dxc/DXIL/DxilSampler.h"
#include "dxc/DXIL/DxilShaderModel.h"
#include "dxc/DXIL/DxilTypeSystem.h"
#include "dxc/DXIL/DxilUtil.h"
#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/DxilContainer/DxilContainerAssembler.h"
#include "dxc/DxilContainer/DxilContainerReader.h"
#include "dxc/Support/exception.h"
#include "dxc/Support/FileIOHelper.h"

using llvm::GlobalVariable;
using llvm::LLVMContext;
using llvm::Module;

// ============================================================
// File helpers
// ============================================================
namespace {

template <typename TValue> struct RecipeParseEntry {
  const char *name = nullptr;
  TValue value{};
};

template <typename TValue>
static bool ParseRecipeValueByTable(const std::string &text,
                                    TValue &value,
                                    llvm::ArrayRef<RecipeParseEntry<TValue>> entries) {
  for (const RecipeParseEntry<TValue> &entry : entries) {
    if (text == entry.name) {
      value = entry.value;
      return true;
    }
  }

  return false;
}

template <typename TValue, size_t N>
static bool ParseRecipeValueByTable(
    const std::string &text,
    TValue &value,
    const RecipeParseEntry<TValue> (&entries)[N]) {
  return ParseRecipeValueByTable(text,
                                 value,
                                 llvm::ArrayRef<RecipeParseEntry<TValue>>(entries));
}

struct DxilProgramBitcode {
  const uint8_t *ptr = nullptr;
  uint32_t size = 0;
};

struct PendingCBufferBlock {
  std::string id;
  CBufferDesc desc;
  CBufferSchema schema;
};

struct PendingRewriteRuleBlock {
  std::string id;
  hlsl::OP::OpCode dxilOpCode = static_cast<hlsl::OP::OpCode>(0);
  std::string rootCaptureName;
  struct PendingOperandPattern {
    std::string parentCaptureName;
    DxilOperandPattern pattern;
  };
  std::vector<PendingOperandPattern> operandPatterns;
  int activeEmitValueIndex = -1;
  DxilRewriteRule rule;
};

struct ParsedRecipeResourceRef {
  bool found = false;
  std::string resourceName;
  ResourceBindingDesc binding{};
};

static bool ExtractProgramBitcodeFromContainerPart(
    const std::vector<uint8_t> &container,
    hlsl::DxilFourCC partKind,
    DxilProgramBitcode &out) {
  hlsl::DxilContainerReader reader;
  if (FAILED(reader.Load(container.data(), container.size()))) {
    std::cerr << "Failed to parse DXIL container header.\n";
    return false;
  }

  uint32_t partIndex = hlsl::DXIL_CONTAINER_BLOB_NOT_FOUND;
  if (FAILED(reader.FindFirstPartKind(partKind, &partIndex)) ||
      partIndex == hlsl::DXIL_CONTAINER_BLOB_NOT_FOUND) {
    return false;
  }

  const void *partData = nullptr;
  uint32_t partSize = 0;
  if (FAILED(reader.GetPartContent(partIndex, &partData, &partSize)) ||
      !partData || partSize < sizeof(hlsl::DxilProgramHeader)) {
    std::cerr << "DXIL container part is missing or malformed.\n";
    return false;
  }

  const hlsl::DxilProgramHeader *programHeader =
      reinterpret_cast<const hlsl::DxilProgramHeader *>(partData);
  if (!hlsl::IsValidDxilProgramHeader(programHeader, partSize)) {
    std::cerr << "DXIL program header validation failed for container part.\n";
    return false;
  }

  out.ptr = reinterpret_cast<const uint8_t *>(
      hlsl::GetDxilBitcodeData(programHeader));
  out.size = hlsl::GetDxilBitcodeSize(programHeader);
  return true;
}

static std::unique_ptr<Module>
ParseDxilBitcode(const uint8_t *ptr, uint32_t size, LLVMContext &context) {
  auto modOrErr = llvm::parseBitcodeFile(
      llvm::MemoryBufferRef(
          llvm::StringRef(reinterpret_cast<const char *>(ptr), size),
          "dxil-program"),
      context);

  if (!modOrErr) {
    std::cerr << "Failed to parse DXIL bitcode: "
              << modOrErr.getError().message() << "\n";
    return nullptr;
  }

  return std::move(modOrErr.get());
}

static bool LoadDxilState(Module &M, hlsl::DxilModule *&outDM) {
  hlsl::DxilModule &DM = M.GetOrCreateDxilModule();
  outDM = &DM;

  if (DM.HasMetadataErrors())
    std::cerr << "DXIL metadata load reported non-fatal errors.\n";

  return true;
}

} // namespace

static bool VerifyModuleOrReportInternal(Module &M) {
  std::string verificationErrors;
  llvm::raw_string_ostream verificationStream(verificationErrors);
  if (!llvm::verifyModule(M, &verificationStream))
    return true;

  verificationStream.flush();
  std::cerr << "Patched module failed LLVM verification.\n";
  if (!verificationErrors.empty())
    std::cerr << verificationErrors;
  return false;
}

namespace {

class ScopedPatchCoInitialize {
public:
  ScopedPatchCoInitialize() {
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    shouldUninitialize_ = SUCCEEDED(hr);
  }

  ~ScopedPatchCoInitialize() {
    if (shouldUninitialize_)
      CoUninitialize();
  }

private:
  bool shouldUninitialize_ = false;
};

} // namespace

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
  }

  return "invalid";
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
  }

  return nullptr;
}

static llvm::Type *GetDxilScalarType(llvm::LLVMContext &ctx,
                                     hlsl::DXIL::ComponentType kind) {
  return GetTextureElementScalarType(ctx, kind);
}

static llvm::Type *GetEmitValueScalarType(const DxilRewriteEmitValue &value,
                                          llvm::LLVMContext &ctx,
                                          llvm::Type *fallbackType) {
  if (!value.hasExplicitResultComponentType)
    return fallbackType;

  return GetDxilScalarType(ctx, value.resultComponentType);
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
    errorMessage = "Texture resources currently support SRV and UAV binding classes only.";
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
    errorMessage = "TextureResourceBuilder currently supports Texture2D and Texture2DArray only.";
    return false;
  }

  switch (desc.elementKind) {
  case hlsl::DXIL::ComponentType::F32:
  case hlsl::DXIL::ComponentType::U32:
  case hlsl::DXIL::ComponentType::I32:
    break;
  default:
    errorMessage = "TextureResourceBuilder currently supports F32, U32, and I32 element types only.";
    return false;
  }

  return true;
}

static std::string GetTextureElementTypeDisplayName(
    const TextureResourceDesc &desc) {
  const std::string scalarName = GetTextureElementTypeName(desc.elementKind);
  if (desc.vectorWidth == 1)
    return scalarName;

  return "vector<" + scalarName + ", " +
         std::to_string(desc.vectorWidth) + ">";
}

static std::string GetTextureTypeName(const TextureResourceDesc &desc) {
  switch (desc.kind) {
  case hlsl::DXIL::ResourceKind::Texture2D:
    return desc.isReadWrite ? "class.RWTexture2D<" +
                                  GetTextureElementTypeDisplayName(desc) + " >"
                            : "class.Texture2D<" +
                                  GetTextureElementTypeDisplayName(desc) + " >";
  case hlsl::DXIL::ResourceKind::Texture2DArray:
    return desc.isReadWrite
               ? "class.RWTexture2DArray<" +
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

static hlsl::CompType::Kind GetCBufferFieldCompKind(
    const CBufferFieldDesc &field) {
  return field.compType;
}

static unsigned GetCBufferFieldScalarSize(
    hlsl::CompType::Kind compType) {
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
      errorMessage = "CBuffer schema currently supports F32, U32, and I32 field types only.";
      return false;
    }

    if (field.vectorSize < 1 || field.vectorSize > 4) {
      errorMessage = "CBuffer schema field vector size must be between 1 and 4.";
      return false;
    }

    const unsigned fieldSize = GetCBufferFieldSize(field);
    if (field.offset + fieldSize > schema.sizeInBytes) {
      errorMessage = "CBuffer schema field exceeds declared struct size: " +
                     field.name;
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

static bool CanUseUnpackedCBufferLayout(const llvm::DataLayout &dataLayout,
                                        const CBufferSchema &schema,
                                        const std::vector<llvm::Type *> &elementTypes,
                                        const std::vector<int> &fieldMapping) {
  if (elementTypes.empty())
    return false;

  llvm::StructType *layoutProbe =
      llvm::StructType::get(elementTypes.front()->getContext(), elementTypes,
                            false);
  const llvm::StructLayout *structLayout = dataLayout.getStructLayout(layoutProbe);
  if (structLayout->getSizeInBytes() != schema.sizeInBytes)
    return false;

  for (size_t elementIndex = 0; elementIndex < fieldMapping.size(); ++elementIndex) {
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
static bool HasBindingConflict(
    const std::vector<std::unique_ptr<TResource>> &resources,
    unsigned space,
    unsigned bindPoint) {
  for (const auto &resource : resources) {
    if (resource->GetSpaceID() == space && resource->GetLowerBound() == bindPoint)
      return true;
  }

  return false;
}

template <typename TResource>
static unsigned FindNextAvailableBinding(
    const std::vector<std::unique_ptr<TResource>> &resources,
    unsigned space) {
  unsigned bindPoint = 0;
  while (HasBindingConflict(resources, space, bindPoint))
    ++bindPoint;
  return bindPoint;
}

static bool HasGlobalNameConflict(const Module &M, const std::string &name) {
  return M.getNamedValue(name) != nullptr;
}

std::string MakeUniqueGlobalName(const Module &M,
                                 const std::string &baseName) {
  if (!HasGlobalNameConflict(M, baseName))
    return baseName;

  for (unsigned suffix = 1; suffix != 0; ++suffix) {
    const std::string candidate = baseName + std::to_string(suffix);
    if (!HasGlobalNameConflict(M, candidate))
      return candidate;
  }

  return baseName;
}

static void KeepGlobalAlive(hlsl::DxilModule &DM, llvm::GlobalVariable *gv) {
  auto &llvmUsed = DM.GetLLVMUsed();
  if (std::find(llvmUsed.begin(), llvmUsed.end(), gv) == llvmUsed.end())
    llvmUsed.push_back(gv);
}

// ============================================================
// LLVM-side declaration skeletons
// ============================================================
//
// These globals are lightweight placeholders to give resource-like and
// cbuffer-like declarations names in the module. Real production usage may
// require richer object typing or annotations.
//

static llvm::StructType *GetOrCreateCBufferType(llvm::Module &M,
                                                const CBufferDesc &desc) {
  llvm::LLVMContext &ctx = M.getContext();
  llvm::StructType *cbStructTy = M.getTypeByName(desc.name + "_t");

  if (!cbStructTy)
    cbStructTy = llvm::StructType::create(ctx, desc.name + "_t");

  if (cbStructTy->isOpaque()) {
    const unsigned dwordCount =
        std::max(1u, ((desc.sizeInBytes + 3u) / 4u));
    llvm::ArrayType *payloadTy =
        llvm::ArrayType::get(llvm::Type::getInt32Ty(ctx), dwordCount);
    llvm::Type *elements[] = {payloadTy};
    cbStructTy->setBody(elements, false);
  }

  return cbStructTy;
}

static llvm::StructType *GetOrCreateCBufferSchemaType(
    llvm::Module &M,
    const CBufferSchema &schema) {
  llvm::LLVMContext &ctx = M.getContext();
  llvm::StructType *cbStructTy = M.getTypeByName(schema.typeName);

  if (!cbStructTy)
    cbStructTy = llvm::StructType::create(ctx, schema.typeName);

  if (cbStructTy->isOpaque()) {
    std::vector<llvm::Type *> elementTypes;
    std::vector<int> fieldMapping;
    BuildCBufferSchemaLayout(schema, ctx, elementTypes, fieldMapping);
    const bool packed =
        !CanUseUnpackedCBufferLayout(M.getDataLayout(), schema, elementTypes,
                                     fieldMapping);
    cbStructTy->setBody(elementTypes, packed);
  }

  return cbStructTy;
}

static llvm::StructType *GetOrCreateMarkerStructType(llvm::Module &M,
                                                     const std::string &name) {
  llvm::LLVMContext &ctx = M.getContext();
  llvm::StructType *structTy = M.getTypeByName(name);
  if (!structTy)
    structTy = llvm::StructType::create(ctx, name);

  if (structTy->isOpaque()) {
    llvm::Type *elements[] = {llvm::Type::getInt32Ty(ctx)};
    structTy->setBody(elements, false);
  }

  return structTy;
}

static llvm::StructType *GetOrCreateTextureType(llvm::Module &M,
                                                const TextureResourceDesc &desc) {
  llvm::LLVMContext &ctx = M.getContext();
  const std::string textureTypeName = GetTextureTypeName(desc);
  llvm::Type *elementType = GetTextureElementType(ctx, desc);
  if (elementType == nullptr)
    return nullptr;

  llvm::StructType *textureType = M.getTypeByName(textureTypeName);
  if (!textureType)
    textureType = llvm::StructType::create(ctx, textureTypeName);
  if (textureType->isOpaque()) {
    if (desc.isReadWrite) {
      llvm::Type *textureElements[] = {elementType};
      textureType->setBody(textureElements, false);
    } else {
      const std::string mipsTypeName = GetTextureMipsTypeName(desc);
      llvm::StructType *mipsType = M.getTypeByName(mipsTypeName);
      if (!mipsType)
        mipsType = llvm::StructType::create(ctx, mipsTypeName);
      if (mipsType->isOpaque()) {
        llvm::Type *mipsElements[] = {llvm::Type::getInt32Ty(ctx)};
        mipsType->setBody(mipsElements, false);
      }

      llvm::Type *textureElements[] = {elementType, mipsType};
      textureType->setBody(textureElements, false);
    }
  }

  return textureType;
}

static llvm::StructType *GetOrCreateResolvedCBufferType(llvm::Module &M,
                                                        const CBufferDesc &desc) {
  return desc.schema != nullptr ? GetOrCreateCBufferSchemaType(M, *desc.schema)
                                : GetOrCreateCBufferType(M, desc);
}

static llvm::Constant *CreateCBufferSymbol(llvm::Module &M,
                                           const CBufferDesc &desc) {
  llvm::StructType *cbStructTy = GetOrCreateResolvedCBufferType(M, desc);
  return llvm::UndefValue::get(cbStructTy->getPointerTo());
}

static GlobalVariable *EnsureTextureGlobal(Module &M,
                                           const TextureResourceDesc &desc) {
  llvm::StructType *texTy = GetOrCreateTextureType(M, desc);
  if (!texTy)
    return nullptr;

  llvm::GlobalVariable *gv = M.getGlobalVariable(desc.name);
  if (!gv) {
    gv = new llvm::GlobalVariable(
        M,
        texTy,
      /*isConstant*/ !desc.isReadWrite,
        llvm::GlobalValue::ExternalLinkage,
        /*Initializer*/ nullptr,
        desc.name);

    if (!desc.isReadWrite &&
      desc.elementKind == hlsl::DXIL::ComponentType::F32 &&
        desc.vectorWidth == 4)
      gv->setAlignment(16);
  }

  return gv;
}

static llvm::Constant *CreateTextureSymbol(Module &M,
                                           const TextureResourceDesc &desc) {
  llvm::StructType *texTy = GetOrCreateTextureType(M, desc);
  if (!texTy)
    return nullptr;
  return llvm::UndefValue::get(texTy->getPointerTo());
}

static GlobalVariable *EnsureSamplerGlobal(Module &M,
                                           const SamplerDesc &desc) {
  llvm::StructType *samplerTy =
      GetOrCreateMarkerStructType(M, desc.name + "_Sampler_t");

  llvm::GlobalVariable *gv = M.getGlobalVariable(desc.name);
  if (!gv) {
    gv = new llvm::GlobalVariable(
        M,
        samplerTy,
        /*isConstant*/ false,
        llvm::GlobalValue::ExternalLinkage,
        /*Initializer*/ nullptr,
        desc.name);
  }

  return gv;
}

// ============================================================
// Optional type-system annotation hooks
// ============================================================
//
// The repo supports richer type metadata via DxilTypeSystem and
// DxilMetadataHelper. If your serializer/consumer needs more than the raw
// resource tables, extend these helpers.
//

static void MaybeAnnotateCBufferType(hlsl::DxilModule &DM,
                                     llvm::StructType *cbufferStructTy,
                                     unsigned sizeInBytes) {
  if (!cbufferStructTy || cbufferStructTy->getNumElements() != 1)
    return;

  hlsl::DxilTypeSystem &typeSystem = DM.GetTypeSystem();
  hlsl::DxilStructAnnotation *structAnnotation =
      typeSystem.GetStructAnnotation(cbufferStructTy);

  if (!structAnnotation)
    structAnnotation = typeSystem.AddStructAnnotation(cbufferStructTy);

  structAnnotation->SetCBufferSize(sizeInBytes);

  hlsl::DxilFieldAnnotation &dataField = structAnnotation->GetFieldAnnotation(0);
  dataField.SetFieldName("Data");
  dataField.SetCBufferOffset(0);
  dataField.SetCompType(hlsl::CompType::getU32().GetKind());
  dataField.SetCBVarUsed(true);

  typeSystem.FinishStructAnnotation(*structAnnotation);
}

static void MaybeAnnotateCBufferType(hlsl::DxilModule &DM,
                                     llvm::StructType *cbufferStructTy,
                                     const CBufferSchema &schema) {
  if (!cbufferStructTy)
    return;

  std::vector<llvm::Type *> elementTypes;
  std::vector<int> fieldMapping;
  BuildCBufferSchemaLayout(schema, cbufferStructTy->getContext(), elementTypes,
                          fieldMapping);

  hlsl::DxilTypeSystem &typeSystem = DM.GetTypeSystem();
  hlsl::DxilStructAnnotation *structAnnotation =
      typeSystem.GetStructAnnotation(cbufferStructTy);
  if (!structAnnotation)
    structAnnotation = typeSystem.AddStructAnnotation(cbufferStructTy);

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

static void MaybeAnnotateTexture2DType(hlsl::DxilModule &DM,
                                       GlobalVariable *gv) {
  (void)DM;
  (void)gv;

  // TODO:
  // If your exact path requires HLSL object/template type annotations for
  // Texture2D<float4>, populate them here.
}

static void MaybeAnnotateSamplerType(hlsl::DxilModule &DM,
                                     GlobalVariable *gv) {
  (void)DM;
  (void)gv;

  // TODO:
  // Populate sampler-specific type annotations if needed.
}

// ============================================================
// Resource add helpers
// ============================================================
bool AddCBuffer(Module &M,
                hlsl::DxilModule &DM,
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
      std::cerr << "CBuffer schema size does not match the requested buffer "
                   "size.\n";
      return false;
    }
  }

  if (HasGlobalNameConflict(M, desc.name) ||
      HasBindingConflict(DM.GetCBuffers(),
                         desc.binding.GetSpace(),
                         desc.binding.GetBindPoint())) {
    std::cerr << "CBuffer name or binding already exists: " << desc.name << "\n";
    return false;
  }

  llvm::StructType *cbufferStructTy = GetOrCreateResolvedCBufferType(M, desc);
  llvm::Constant *symbol = CreateCBufferSymbol(M, desc);
  if (desc.schema != nullptr)
    MaybeAnnotateCBufferType(DM, cbufferStructTy, *desc.schema);
  else
    MaybeAnnotateCBufferType(DM, cbufferStructTy, sizeInBytes);

  std::unique_ptr<hlsl::DxilCBuffer> cb =
      std::make_unique<hlsl::DxilCBuffer>();

  cb->SetGlobalSymbol(symbol);
  cb->SetGlobalName(desc.name);
  cb->SetHLSLType(symbol->getType());
  cb->SetSpaceID(desc.binding.GetSpace());
  cb->SetLowerBound(desc.binding.GetBindPoint());
  cb->SetRangeSize(1);
  cb->SetID(static_cast<unsigned>(DM.GetCBuffers().size()));
  cb->SetSize(sizeInBytes);

  DM.AddCBuffer(std::move(cb));
  return true;
}

bool AddTextureSRV(Module &M,
                   hlsl::DxilModule &DM,
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

  if (HasGlobalNameConflict(M, desc.name) ||
      HasBindingConflict(DM.GetSRVs(),
                         desc.binding.GetSpace(),
                         desc.binding.GetBindPoint())) {
    std::cerr << "Texture SRV name or binding already exists: " << desc.name
              << "\n";
    return false;
  }

  llvm::Constant *symbol = CreateTextureSymbol(M, desc);
  if (!symbol) {
    std::cerr << "Failed to create a canonical texture symbol for " << desc.name
              << ".\n";
    return false;
  }

  std::unique_ptr<hlsl::DxilResource> tex =
      std::make_unique<hlsl::DxilResource>();

  tex->SetGlobalSymbol(symbol);
  tex->SetGlobalName(desc.name);
  tex->SetHLSLType(symbol->getType());
  tex->SetSpaceID(desc.binding.GetSpace());
  tex->SetLowerBound(desc.binding.GetBindPoint());
  tex->SetRangeSize(1);
  tex->SetID(static_cast<unsigned>(DM.GetSRVs().size()));

  tex->SetKind(desc.kind);
  tex->SetCompType(GetTextureCompType(desc.elementKind));
  tex->SetSampleCount(0);
  tex->SetRW(desc.isReadWrite);

  // Optional TODOs depending on your exact branch:
  // - tex->SetHLSLType(...)
  // - template/type annotation for Texture2D<float4>
  // - explicit link to resource global symbol if required by downstream passes

  DM.AddSRV(std::move(tex));
  return true;
}

bool AddTexture2DSRV(Module &M,
                     hlsl::DxilModule &DM,
                     const TextureResourceDesc &desc) {
  return AddTextureSRV(M, DM, desc);
}

bool AddTextureUAV(Module &M,
                   hlsl::DxilModule &DM,
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

  if (HasGlobalNameConflict(M, desc.name) ||
      HasBindingConflict(DM.GetUAVs(),
                         desc.binding.GetSpace(),
                         desc.binding.GetBindPoint())) {
    std::cerr << "Texture UAV name or binding already exists: " << desc.name
              << "\n";
    return false;
  }

  llvm::Constant *symbol = CreateTextureSymbol(M, desc);
  if (!symbol) {
    std::cerr << "Failed to create a canonical texture symbol for " << desc.name
              << ".\n";
    return false;
  }

  std::unique_ptr<hlsl::DxilResource> tex =
      std::make_unique<hlsl::DxilResource>();

  tex->SetGlobalSymbol(symbol);
  tex->SetGlobalName(desc.name);
  tex->SetHLSLType(symbol->getType());
  tex->SetSpaceID(desc.binding.GetSpace());
  tex->SetLowerBound(desc.binding.GetBindPoint());
  tex->SetRangeSize(1);
  tex->SetID(static_cast<unsigned>(DM.GetUAVs().size()));

  tex->SetKind(desc.kind);
  tex->SetCompType(GetTextureCompType(desc.elementKind));
  tex->SetSampleCount(0);
  tex->SetRW(true);

  DM.AddUAV(std::move(tex));
  return true;
}

bool AddSampler(Module &M,
                hlsl::DxilModule &DM,
                const SamplerDesc &desc) {
  if (desc.name.empty()) {
    std::cerr << "Refusing to add sampler with an empty name.\n";
    return false;
  }

  if (HasGlobalNameConflict(M, desc.name) ||
      HasBindingConflict(DM.GetSamplers(),
                         desc.binding.GetSpace(),
                         desc.binding.GetBindPoint())) {
    std::cerr << "Sampler name or binding already exists: " << desc.name << "\n";
    return false;
  }

  GlobalVariable *gv = EnsureSamplerGlobal(M, desc);
  KeepGlobalAlive(DM, gv);
  MaybeAnnotateSamplerType(DM, gv);

  std::unique_ptr<hlsl::DxilSampler> sampler =
      std::make_unique<hlsl::DxilSampler>();

  sampler->SetGlobalSymbol(gv);
  sampler->SetGlobalName(desc.name);
  sampler->SetHLSLType(gv->getType());
  sampler->SetSpaceID(desc.binding.GetSpace());
  sampler->SetLowerBound(desc.binding.GetBindPoint());
  sampler->SetRangeSize(1);
  sampler->SetID(static_cast<unsigned>(DM.GetSamplers().size()));
  sampler->SetKind(hlsl::DXIL::ResourceKind::Sampler);
  sampler->SetSamplerKind(hlsl::DXIL::SamplerKind::Default);

  DM.AddSampler(std::move(sampler));
  return true;
}

static void TraceMessage(bool traceEnabled, const char *message) {
  if (traceEnabled)
    std::cerr << "[trace] " << message << "\n";
}

static bool IsDxOpCall(const llvm::Instruction &instruction,
                       llvm::StringRef functionName) {
  const llvm::CallInst *call = llvm::dyn_cast<llvm::CallInst>(&instruction);
  const llvm::Function *callee = call != nullptr ? call->getCalledFunction()
                                                 : nullptr;
  return callee != nullptr && callee->getName() == functionName;
}

static bool TryGetDxilOpCode(const llvm::Instruction &instruction,
                             hlsl::OP::OpCode &opCode) {
  if (!hlsl::OP::IsDxilOpFuncCallInst(&instruction))
    return false;

  opCode = hlsl::OP::GetDxilOpFuncCallInst(&instruction);
  return true;
}

static bool IsDxOpCall(const llvm::Instruction &instruction,
                       hlsl::OP::OpCode opCode) {
  hlsl::OP::OpCode actualOpCode;
  return TryGetDxilOpCode(instruction, actualOpCode) && actualOpCode == opCode;
}

static bool IsConstantIntValue(const llvm::Value *value, uint64_t expected) {
  const llvm::ConstantInt *constantInt =
      llvm::dyn_cast<llvm::ConstantInt>(value);
  return constantInt != nullptr && constantInt->getZExtValue() == expected;
}

static bool TryCaptureValue(const std::string &captureName,
                            llvm::Value *value,
                            std::unordered_map<std::string, llvm::Value *> &captures) {
  if (captureName.empty())
    return true;

  auto it = captures.find(captureName);
  if (it == captures.end()) {
    captures.emplace(captureName, value);
    return true;
  }

  return it->second == value;
}

static bool MatchCapturedValueConstraint(
    const std::string &captureName,
    llvm::Value *value,
    const std::unordered_map<std::string, llvm::Value *> &captures) {
  if (captureName.empty())
    return true;

  auto it = captures.find(captureName);
  return it != captures.end() && it->second == value;
}

static bool MergeDxilMatchCaptures(
    const std::unordered_map<std::string, llvm::Value *> &sourceCaptures,
    std::unordered_map<std::string, llvm::Value *> &targetCaptures) {
  for (const auto &entry : sourceCaptures) {
    if (!TryCaptureValue(entry.first, entry.second, targetCaptures))
      return false;
  }

  return true;
}

static bool MatchDxilOperandPattern(
    llvm::Value *value,
    const DxilOperandPattern &pattern,
    std::unordered_map<std::string, llvm::Value *> &captures,
    hlsl::DxilModule *dxilModule);

static bool TryGetConstantStructIntField(const llvm::Value *value,
                                         unsigned fieldIndex,
                                         uint64_t &result) {
  const llvm::Constant *constantValue = llvm::dyn_cast<llvm::Constant>(value);
  if (constantValue == nullptr)
    return false;

  if (llvm::isa<llvm::ConstantAggregateZero>(constantValue)) {
    result = 0;
    return true;
  }

  const llvm::ConstantStruct *constantStruct =
      llvm::dyn_cast<llvm::ConstantStruct>(constantValue);
  if (constantStruct == nullptr || fieldIndex >= constantStruct->getNumOperands())
    return false;

  const llvm::ConstantInt *fieldConstant =
      llvm::dyn_cast<llvm::ConstantInt>(constantStruct->getOperand(fieldIndex));
  if (fieldConstant == nullptr)
    return false;

  result = fieldConstant->getZExtValue();
  return true;
}

static const hlsl::DxilResourceBase *FindResourceByBinding(
    hlsl::DxilModule &dxilModule,
    hlsl::DXIL::ResourceClass resourceClass,
    unsigned bindPoint,
    unsigned space) {
  switch (resourceClass) {
  case hlsl::DXIL::ResourceClass::SRV:
    for (const auto &srv : dxilModule.GetSRVs()) {
      if (srv->GetLowerBound() == bindPoint && srv->GetSpaceID() == space)
        return srv.get();
    }
    break;
  case hlsl::DXIL::ResourceClass::UAV:
    for (const auto &uav : dxilModule.GetUAVs()) {
      if (uav->GetLowerBound() == bindPoint && uav->GetSpaceID() == space)
        return uav.get();
    }
    break;
  case hlsl::DXIL::ResourceClass::CBuffer:
    for (const auto &cbuffer : dxilModule.GetCBuffers()) {
      if (cbuffer->GetLowerBound() == bindPoint &&
          cbuffer->GetSpaceID() == space) {
        return cbuffer.get();
      }
    }
    break;
  case hlsl::DXIL::ResourceClass::Sampler:
    for (const auto &sampler : dxilModule.GetSamplers()) {
      if (sampler->GetLowerBound() == bindPoint && sampler->GetSpaceID() == space)
        return sampler.get();
    }
    break;
  default:
    break;
  }

  return nullptr;
}

static const hlsl::DxilResourceBase *FindResourceByName(
    hlsl::DxilModule &dxilModule,
    hlsl::DXIL::ResourceClass resourceClass,
    llvm::StringRef resourceName) {
  switch (resourceClass) {
  case hlsl::DXIL::ResourceClass::SRV:
    for (const auto &srv : dxilModule.GetSRVs()) {
      if (srv->GetGlobalName() == resourceName)
        return srv.get();
    }
    break;
  case hlsl::DXIL::ResourceClass::UAV:
    for (const auto &uav : dxilModule.GetUAVs()) {
      if (uav->GetGlobalName() == resourceName)
        return uav.get();
    }
    break;
  case hlsl::DXIL::ResourceClass::CBuffer:
    for (const auto &cbuffer : dxilModule.GetCBuffers()) {
      if (cbuffer->GetGlobalName() == resourceName)
        return cbuffer.get();
    }
    break;
  case hlsl::DXIL::ResourceClass::Sampler:
    for (const auto &sampler : dxilModule.GetSamplers()) {
      if (sampler->GetGlobalName() == resourceName)
        return sampler.get();
    }
    break;
  default:
    break;
  }

  return nullptr;
}

static const hlsl::DxilResourceBase *FindResourceByOrdinal(
    hlsl::DxilModule &dxilModule,
    hlsl::DXIL::ResourceClass resourceClass,
    unsigned resourceIndex) {
  switch (resourceClass) {
  case hlsl::DXIL::ResourceClass::SRV: {
    const auto &srvs = dxilModule.GetSRVs();
    return resourceIndex < srvs.size() ? srvs[resourceIndex].get() : nullptr;
  }
  case hlsl::DXIL::ResourceClass::UAV: {
    const auto &uavs = dxilModule.GetUAVs();
    return resourceIndex < uavs.size() ? uavs[resourceIndex].get() : nullptr;
  }
  case hlsl::DXIL::ResourceClass::CBuffer: {
    const auto &cbuffers = dxilModule.GetCBuffers();
    return resourceIndex < cbuffers.size() ? cbuffers[resourceIndex].get() : nullptr;
  }
  case hlsl::DXIL::ResourceClass::Sampler: {
    const auto &samplers = dxilModule.GetSamplers();
    return resourceIndex < samplers.size() ? samplers[resourceIndex].get() : nullptr;
  }
  default:
    return nullptr;
  }
}

static bool TryResolveHandleResource(llvm::Value *value,
                                     hlsl::DxilModule &dxilModule,
                                     hlsl::DXIL::ResourceClass preferredResourceClass,
                                     const hlsl::DxilResourceBase *&resourceOut) {
  resourceOut = nullptr;

  llvm::CallInst *call = llvm::dyn_cast<llvm::CallInst>(value);
  if (call == nullptr)
    return false;

  llvm::CallInst *createHandleCall = call;
  if (IsDxOpCall(*call, hlsl::OP::OpCode::AnnotateHandle)) {
    createHandleCall = llvm::dyn_cast<llvm::CallInst>(call->getArgOperand(1));
  }

  if (createHandleCall == nullptr ||
      !IsDxOpCall(*createHandleCall, hlsl::OP::OpCode::CreateHandleFromBinding)) {
    return false;
  }

  uint64_t lowerBound = 0;
  uint64_t spaceId = 0;
  uint64_t resourceClassValue = 0;
  if (!TryGetConstantStructIntField(createHandleCall->getArgOperand(1),
                                    0,
                                    lowerBound) ||
      !TryGetConstantStructIntField(createHandleCall->getArgOperand(1),
                                    2,
                                    spaceId) ||
      !TryGetConstantStructIntField(createHandleCall->getArgOperand(1),
                                    3,
                                    resourceClassValue)) {
    return false;
  }

  uint64_t handleIndex = lowerBound;
  if (const llvm::ConstantInt *handleIndexConstant =
          llvm::dyn_cast<llvm::ConstantInt>(createHandleCall->getArgOperand(2))) {
    handleIndex = handleIndexConstant->getZExtValue();
  }

  hlsl::DXIL::ResourceClass resolvedResourceClass =
      static_cast<hlsl::DXIL::ResourceClass>(resourceClassValue);
  if (resolvedResourceClass == hlsl::DXIL::ResourceClass::Invalid &&
      preferredResourceClass != hlsl::DXIL::ResourceClass::Invalid) {
    resolvedResourceClass = preferredResourceClass;
  }

  resourceOut = FindResourceByBinding(
      dxilModule,
      resolvedResourceClass,
      static_cast<unsigned>(handleIndex),
      static_cast<unsigned>(spaceId));
  if (resourceOut == nullptr &&
      resolvedResourceClass != hlsl::DXIL::ResourceClass::Invalid) {
    resourceOut = FindResourceByOrdinal(dxilModule,
                                        resolvedResourceClass,
                                        static_cast<unsigned>(handleIndex));
  }
  return resourceOut != nullptr;
}

static bool MatchResourceHandlePattern(
    llvm::Value *value,
    const DxilOperandPattern &pattern,
    std::unordered_map<std::string, llvm::Value *> &captures,
    hlsl::DxilModule *dxilModule) {
  if (dxilModule == nullptr)
    return false;

  const hlsl::DxilResourceBase *resource = nullptr;
  const hlsl::DXIL::ResourceClass preferredResourceClass =
      pattern.matchResourceClass ? pattern.resourceClass
                                 : hlsl::DXIL::ResourceClass::Invalid;
  if (!TryResolveHandleResource(value,
                                *dxilModule,
                                preferredResourceClass,
                                resource)) {
    return false;
  }

  if (pattern.matchResourceClass && resource->GetClass() != pattern.resourceClass)
    return false;
  if (pattern.matchAnyTexture) {
    const hlsl::DxilResource *typedResource =
      dynamic_cast<const hlsl::DxilResource *>(resource);
    if (typedResource == nullptr || !typedResource->IsAnyTexture())
      return false;
  }
  if (pattern.matchResourceKind && resource->GetKind() != pattern.resourceKind)
    return false;
  if (!pattern.resourceName.empty() && resource->GetGlobalName() != pattern.resourceName)
    return false;
  if (!pattern.resourceNameLikePattern.empty()) {
    llvm::Regex resourceNameRegex(pattern.resourceNameLikePattern);
    if (!resourceNameRegex.match(resource->GetGlobalName()))
      return false;
  }
  if (pattern.resourceBindPoint >= 0 &&
      resource->GetLowerBound() != static_cast<unsigned>(pattern.resourceBindPoint)) {
    return false;
  }
  if (pattern.resourceSpace >= 0 &&
      resource->GetSpaceID() != static_cast<unsigned>(pattern.resourceSpace)) {
    return false;
  }

  return true;
}

static bool MatchInstructionPattern(
    llvm::Instruction *instruction,
    const DxilOperandPattern &pattern,
    std::unordered_map<std::string, llvm::Value *> &captures,
    hlsl::DxilModule *dxilModule) {
  if (instruction == nullptr || instruction->getOpcode() != pattern.instructionOpcode)
    return false;

  for (const DxilOperandPattern &operandPattern : pattern.operandPatterns) {
    if (operandPattern.operandIndex >= instruction->getNumOperands())
      return false;
    if (!MatchDxilOperandPattern(instruction->getOperand(operandPattern.operandIndex),
                                 operandPattern,
                                 captures,
                                 dxilModule)) {
      return false;
    }
  }

  return true;
}

static bool MatchDxilCallPattern(
    llvm::CallInst *call,
    const DxilCallPattern &pattern,
    std::unordered_map<std::string, llvm::Value *> &captures,
    hlsl::DxilModule *dxilModule) {
  if (call == nullptr)
    return false;

  llvm::Function *callee = call->getCalledFunction();
  if (callee == nullptr)
    return false;

  if (pattern.matchDxilOpCode) {
    if (!IsDxOpCall(*call, pattern.dxilOpCode))
      return false;
  } else if (!pattern.calleeName.empty() && callee->getName() != pattern.calleeName) {
    return false;
  }

  if (!TryCaptureValue(pattern.captureName, call, captures))
    return false;

  for (const DxilOperandPattern &operandPattern : pattern.operandPatterns) {
    if (operandPattern.operandIndex >= call->getNumArgOperands())
      return false;
    if (!MatchDxilOperandPattern(call->getArgOperand(operandPattern.operandIndex),
                                 operandPattern,
                                 captures,
                                 dxilModule)) {
      return false;
    }
  }

  return true;
}

static bool MatchDxilOperandPattern(
    llvm::Value *value,
    const DxilOperandPattern &pattern,
    std::unordered_map<std::string, llvm::Value *> &captures,
    hlsl::DxilModule *dxilModule) {
  if (!TryCaptureValue(pattern.captureName, value, captures))
    return false;

  if (!MatchCapturedValueConstraint(pattern.matchCaptureName, value, captures))
    return false;

  if (pattern.kind == DxilOperandPatternKind::Any)
    return true;

  if (pattern.kind == DxilOperandPatternKind::ConstantInt)
    return IsConstantIntValue(value, pattern.constantIntValue);

  if (pattern.kind == DxilOperandPatternKind::Instruction) {
    llvm::Instruction *instruction = llvm::dyn_cast<llvm::Instruction>(value);
    return MatchInstructionPattern(instruction, pattern, captures, dxilModule);
  }

  if (pattern.kind == DxilOperandPatternKind::ResourceHandle) {
    return MatchResourceHandlePattern(value, pattern, captures, dxilModule);
  }

  llvm::CallInst *call = llvm::dyn_cast<llvm::CallInst>(value);
  if (call == nullptr)
    return false;

  DxilCallPattern nestedPattern;
  nestedPattern.calleeName = pattern.calleeName;
  nestedPattern.matchDxilOpCode = pattern.matchDxilOpCode;
  nestedPattern.dxilOpCode = pattern.dxilOpCode;
  nestedPattern.captureName = pattern.captureName;
  nestedPattern.operandPatterns = pattern.operandPatterns;
  return MatchDxilCallPattern(call, nestedPattern, captures, dxilModule);
}

bool FindDxilCallMatch(llvm::Function &function,
                       const DxilCallPattern &pattern,
                       DxilMatchResult &result,
                       hlsl::DxilModule *dxilModule) {
  for (llvm::BasicBlock &basicBlock : function) {
    for (llvm::Instruction &instruction : basicBlock) {
      llvm::CallInst *call = llvm::dyn_cast<llvm::CallInst>(&instruction);
      if (call == nullptr)
        continue;

      std::unordered_map<std::string, llvm::Value *> captures;
      if (!MatchDxilCallPattern(call, pattern, captures, dxilModule))
        continue;

      result.rootCall = call;
      result.captures = std::move(captures);
      return true;
    }
  }

  return false;
}

unsigned CollectDxilCallMatches(llvm::Function &function,
                                const DxilCallPattern &pattern,
                                std::vector<DxilMatchResult> &results,
                                hlsl::DxilModule *dxilModule) {
  results.clear();

  for (llvm::BasicBlock &basicBlock : function) {
    for (llvm::Instruction &instruction : basicBlock) {
      llvm::CallInst *call = llvm::dyn_cast<llvm::CallInst>(&instruction);
      if (call == nullptr)
        continue;

      std::unordered_map<std::string, llvm::Value *> captures;
      if (!MatchDxilCallPattern(call, pattern, captures, dxilModule))
        continue;

      DxilMatchResult result;
      result.rootCall = call;
      result.captures = std::move(captures);
      results.push_back(std::move(result));
    }
  }

  return static_cast<unsigned>(results.size());
}

static bool IsPrunableDxilInstruction(const llvm::Instruction &instruction) {
  if (const llvm::CallInst *call = llvm::dyn_cast<llvm::CallInst>(&instruction)) {
    const llvm::Function *callee = call->getCalledFunction();
    if (callee == nullptr)
      return false;
    if (call->doesNotAccessMemory() || call->onlyReadsMemory())
      return true;

    const llvm::StringRef calleeName = callee->getName();
    return calleeName == "dx.op.annotateHandle" ||
           calleeName == "dx.op.createHandleFromBinding";
  }

  return !instruction.mayHaveSideEffects();
}

static void CollectPrunableOperands(
    llvm::Instruction *instruction,
    std::unordered_set<llvm::Instruction *> &visited,
    std::vector<llvm::WeakTrackingVH> &postOrder) {
  if (instruction == nullptr || !visited.insert(instruction).second)
    return;

  postOrder.emplace_back(instruction);
  for (llvm::Use &operandUse : instruction->operands()) {
    llvm::Instruction *operandInstruction =
        llvm::dyn_cast<llvm::Instruction>(operandUse.get());
    if (operandInstruction == nullptr || !operandInstruction->use_empty())
      continue;

    CollectPrunableOperands(operandInstruction, visited, postOrder);
  }
}

static void PruneDeadDxilTree(llvm::Instruction *root) {
  if (root == nullptr)
    return;

  std::unordered_set<llvm::Instruction *> visited;
  std::vector<llvm::WeakTrackingVH> postOrder;
  CollectPrunableOperands(root, visited, postOrder);

  for (auto it = postOrder.rbegin(); it != postOrder.rend(); ++it) {
    llvm::Instruction *candidate = llvm::dyn_cast_or_null<llvm::Instruction>(
        static_cast<llvm::Value *>(*it));
    if (candidate == nullptr || !candidate->use_empty() ||
        !IsPrunableDxilInstruction(*candidate)) {
      continue;
    }

    if (llvm::isInstructionTriviallyDead(candidate)) {
      llvm::RecursivelyDeleteTriviallyDeadInstructions(candidate);
      continue;
    }

    candidate->eraseFromParent();
  }
}

static llvm::Constant *CreateResBindConstant(llvm::Type *resBindType,
                                             unsigned bindPoint,
                                             unsigned space,
                                             unsigned resourceClass) {
  llvm::StructType *structType = llvm::dyn_cast<llvm::StructType>(resBindType);
  if (structType == nullptr || structType->getNumElements() != 4)
    return nullptr;

  llvm::Type *lowerType = structType->getElementType(0);
  llvm::Type *upperType = structType->getElementType(1);
  llvm::Type *spaceType = structType->getElementType(2);
  llvm::Type *classType = structType->getElementType(3);

  llvm::Constant *fields[] = {
      llvm::ConstantInt::get(lowerType, bindPoint),
      llvm::ConstantInt::get(upperType, bindPoint),
      llvm::ConstantInt::get(spaceType, space),
      llvm::ConstantInt::get(classType, resourceClass),
  };
  return llvm::ConstantStruct::get(structType, fields);
}

static void PruneCandidateInstructions(
    const std::vector<llvm::Instruction *> &candidates) {
  for (llvm::Instruction *candidate : candidates) {
    if (candidate == nullptr)
      continue;

    if (llvm::isInstructionTriviallyDead(candidate)) {
      llvm::RecursivelyDeleteTriviallyDeadInstructions(candidate);
      continue;
    }

    PruneDeadDxilTree(candidate);
  }
}

void PruneInstructionRoots(const std::vector<llvm::Instruction *> &roots) {
  PruneCandidateInstructions(roots);
}

void PruneFunctionDeadCode(llvm::Function &function) {
  bool changed = false;
  do {
    changed = false;

    std::vector<llvm::WeakTrackingVH> candidates;
    for (llvm::BasicBlock &basicBlock : function) {
      for (llvm::Instruction &instruction : basicBlock) {
        if (!instruction.use_empty())
          continue;
        if (instruction.isTerminator())
          continue;

        candidates.emplace_back(&instruction);
      }
    }

    for (llvm::WeakTrackingVH &candidateHandle : candidates) {
      llvm::Instruction *candidate = llvm::dyn_cast_or_null<llvm::Instruction>(
          static_cast<llvm::Value *>(candidateHandle));
      if (candidate == nullptr || !candidate->use_empty())
        continue;

      if (llvm::isInstructionTriviallyDead(candidate)) {
        llvm::RecursivelyDeleteTriviallyDeadInstructions(candidate);
        changed = true;
        continue;
      }

      const llvm::WeakTrackingVH pruneProbe(candidate);
      PruneDeadDxilTree(candidate);
      if (static_cast<llvm::Value *>(pruneProbe) == nullptr)
        changed = true;
    }
  } while (changed);
}

static llvm::Instruction *ResolveMatchInstruction(
    const DxilMatchResult &match,
    const std::string &captureName) {
  if (captureName.empty())
    return match.rootCall;

  return llvm::dyn_cast_or_null<llvm::Instruction>(match.GetCapture(captureName));
}

static void AppendUniqueInstruction(
    std::vector<llvm::Instruction *> &instructions,
    llvm::Instruction *instruction) {
  if (instruction == nullptr)
    return;

  if (std::find(instructions.begin(), instructions.end(), instruction) ==
      instructions.end()) {
    instructions.push_back(instruction);
  }
}

static bool CollectInstructionRange(llvm::Instruction *start,
                                    llvm::Instruction *end,
                                    std::vector<llvm::Instruction *> &range) {
  range.clear();
  if (start == nullptr || end == nullptr || start->getParent() != end->getParent())
    return false;

  bool foundStart = false;
  for (llvm::Instruction &instruction : *start->getParent()) {
    if (&instruction == start)
      foundStart = true;
    if (!foundStart)
      continue;

    range.push_back(&instruction);
    if (&instruction == end)
      return true;
  }

  range.clear();
  return false;
}

static llvm::Argument *GetFunctionArg(llvm::Function *function, unsigned index) {
  if (function == nullptr || index >= function->arg_size())
    return nullptr;

  auto argIt = function->arg_begin();
  std::advance(argIt, index);
  return &*argIt;
}

static const hlsl::DxilResourceBase *ResolveEmitResource(
    const std::string &resourceName,
    const ResourceBindingDesc &resourceBinding,
    hlsl::DxilModule &dxilModule) {
  const hlsl::DxilResourceBase *resource = nullptr;
  if (!resourceName.empty()) {
    resource = FindResourceByName(
        dxilModule, resourceBinding.GetResourceClass(), resourceName);
  }
  if (resource == nullptr && !resourceBinding.IsAutoBinding()) {
    resource = FindResourceByBinding(dxilModule,
                                     resourceBinding.GetResourceClass(),
                                     resourceBinding.GetBindPoint(),
                                     resourceBinding.GetSpace());
  }
  return resource;
}

static llvm::Constant *BuildEmitResourceBindingConstant(
    hlsl::OP &dxilOp,
    hlsl::DxilModule &dxilModule,
    const ResourceBindingDesc &resourceBinding) {
  return hlsl::resource_helper::getAsConstant(resourceBinding.GetDxilBinding(),
                                              dxilOp.GetResourceBindingType(),
                                              *dxilModule.GetShaderModel());
}

static llvm::Constant *BuildEmitResourcePropertiesConstant(
    hlsl::OP &dxilOp,
    hlsl::DxilModule &dxilModule,
    const hlsl::DxilResourceBase &resource) {
  return hlsl::resource_helper::getAsConstant(
      hlsl::resource_helper::loadPropsFromResourceBase(&resource),
      dxilOp.GetResourcePropertiesType(),
      *dxilModule.GetShaderModel());
}

static llvm::Value *ResolveEmitOperandValue(
    const DxilRewriteEmitOperand &operand,
    llvm::Type *argType,
    llvm::IRBuilder<> &builder,
    llvm::Module &module,
    hlsl::DxilModule &dxilModule,
    const DxilMatchResult &match,
    const std::unordered_map<std::string, llvm::Value *> *temporaryValues) {
  switch (operand.kind) {
  case DxilRewriteEmitOperandKind::Capture:
    return match.GetCapture(operand.captureName);
  case DxilRewriteEmitOperandKind::Temporary:
    if (temporaryValues == nullptr)
      return nullptr;
    {
      auto temporaryIt = temporaryValues->find(operand.temporaryName);
      return temporaryIt != temporaryValues->end() ? temporaryIt->second : nullptr;
    }
  case DxilRewriteEmitOperandKind::ConstantInt: {
    llvm::IntegerType *integerType = llvm::dyn_cast<llvm::IntegerType>(argType);
    if (integerType == nullptr)
      return nullptr;
    return llvm::ConstantInt::get(integerType, operand.constantIntValue);
  }
  case DxilRewriteEmitOperandKind::ResourceHandle: {
    hlsl::OP dxilOp(module.getContext(), &module);
    dxilOp.InitWithMinPrecision(dxilModule.GetUseMinPrecision());

    const hlsl::DxilResourceBase *resource =
      ResolveEmitResource(operand.resourceName, operand.resourceBinding, dxilModule);
    if (resource == nullptr)
      return nullptr;

    llvm::Constant *resourceBindingConstant =
      BuildEmitResourceBindingConstant(dxilOp, dxilModule, operand.resourceBinding);
    llvm::Constant *resourcePropsConstant =
      BuildEmitResourcePropertiesConstant(dxilOp, dxilModule, *resource);
    llvm::Function *createHandleFunction = dxilOp.GetOpFunc(
        hlsl::OP::OpCode::CreateHandleFromBinding,
        dxilOp.GetHandleType());
    llvm::Function *annotateHandleFunction = dxilOp.GetOpFunc(
        hlsl::OP::OpCode::AnnotateHandle,
        dxilOp.GetHandleType());
    if (resourceBindingConstant == nullptr || resourcePropsConstant == nullptr ||
        createHandleFunction == nullptr || annotateHandleFunction == nullptr) {
      return nullptr;
    }

    llvm::Argument *createHandleOpcodeArgument = GetFunctionArg(createHandleFunction, 0);
    llvm::Argument *createHandleIndexArgument = GetFunctionArg(createHandleFunction, 2);
    llvm::Argument *createHandleNonUniformArgument =
        GetFunctionArg(createHandleFunction, 3);
    llvm::Argument *annotateHandleOpcodeArgument =
        GetFunctionArg(annotateHandleFunction, 0);
    if (createHandleOpcodeArgument == nullptr ||
        createHandleIndexArgument == nullptr ||
        createHandleNonUniformArgument == nullptr ||
        annotateHandleOpcodeArgument == nullptr) {
      return nullptr;
    }

    llvm::Value *rawHandle = builder.CreateCall(
        createHandleFunction,
        {llvm::ConstantInt::get(createHandleOpcodeArgument->getType(),
                                static_cast<uint64_t>(hlsl::OP::OpCode::CreateHandleFromBinding)),
         resourceBindingConstant,
         llvm::ConstantInt::get(createHandleIndexArgument->getType(),
                                operand.resourceBinding.GetBindPoint()),
         llvm::ConstantInt::get(createHandleNonUniformArgument->getType(), 0)});
    return builder.CreateCall(
        annotateHandleFunction,
        {llvm::ConstantInt::get(annotateHandleOpcodeArgument->getType(),
                                static_cast<uint64_t>(hlsl::OP::OpCode::AnnotateHandle)),
         rawHandle,
         resourcePropsConstant});
  }
  case DxilRewriteEmitOperandKind::Undef:
    return llvm::UndefValue::get(argType);
  }

  return nullptr;
}

static bool BuildDeclarativeSequenceRewriteResult(
    const DxilRewriteRule &rule,
    llvm::Instruction *replacementTarget,
    llvm::IRBuilder<> &builder,
    llvm::Module &module,
    hlsl::DxilModule &dxilModule,
    const DxilMatchResult &match,
    DxilRewriteResult &result) {
  if (replacementTarget == nullptr || rule.emittedSequence.values.empty() ||
      rule.emittedSequence.replacementValueName.empty()) {
    return false;
  }

  std::unordered_map<std::string, llvm::Value *> temporaryValues;
  hlsl::OP dxilOp(module.getContext(), &module);
  dxilOp.InitWithMinPrecision(dxilModule.GetUseMinPrecision());

  for (const DxilRewriteEmitValue &value : rule.emittedSequence.values) {
    if (value.name.empty())
      return false;

    llvm::Value *emittedValue = nullptr;
    if (value.kind == DxilRewriteEmitValueKind::DxOpCall) {
      llvm::Type *emittedResultType = GetEmitValueScalarType(
          value, module.getContext(), replacementTarget->getType());
      if (emittedResultType == nullptr)
        return false;

      llvm::Function *emittedFunction =
          dxilOp.GetOpFunc(value.dxilOpCode, emittedResultType);
      if (emittedFunction == nullptr)
        return false;

      std::vector<DxilRewriteEmitOperand> emitOperands = value.operands;
      std::sort(emitOperands.begin(),
                emitOperands.end(),
                [](const DxilRewriteEmitOperand &lhs,
                   const DxilRewriteEmitOperand &rhs) {
                  return lhs.operandIndex < rhs.operandIndex;
                });

      std::vector<llvm::Value *> emittedArgs;
      emittedArgs.reserve(emittedFunction->arg_size());
      llvm::Argument *opcodeArgument = GetFunctionArg(emittedFunction, 0);
      if (opcodeArgument == nullptr)
        return false;
      emittedArgs.push_back(llvm::ConstantInt::get(
          opcodeArgument->getType(), static_cast<uint64_t>(value.dxilOpCode)));

      size_t emitOperandIndex = 0;
      for (unsigned argIndex = 1; argIndex < emittedFunction->arg_size(); ++argIndex) {
        if (emitOperandIndex >= emitOperands.size() ||
            emitOperands[emitOperandIndex].operandIndex != argIndex) {
          return false;
        }

        llvm::Argument *arg = GetFunctionArg(emittedFunction, argIndex);
        if (arg == nullptr)
          return false;

        llvm::Value *argValue = ResolveEmitOperandValue(emitOperands[emitOperandIndex++],
                                                        arg->getType(),
                                                        builder,
                                                        module,
                                                        dxilModule,
                                                        match,
                                                        &temporaryValues);
        if (argValue == nullptr || argValue->getType() != arg->getType())
          return false;

        emittedArgs.push_back(argValue);
      }

      emittedValue = builder.CreateCall(emittedFunction, emittedArgs);
    } else if (value.kind == DxilRewriteEmitValueKind::ExtractValue) {
      auto aggregateIt = temporaryValues.find(value.aggregateName);
      if (aggregateIt == temporaryValues.end())
        return false;
      emittedValue = builder.CreateExtractValue(aggregateIt->second, value.extractIndex);
    } else if (value.kind == DxilRewriteEmitValueKind::BinaryInstruction) {
      llvm::Type *resultType =
          GetEmitValueScalarType(value, module.getContext(), replacementTarget->getType());
      if (resultType == nullptr)
        return false;

      std::vector<DxilRewriteEmitOperand> emitOperands = value.operands;
      std::sort(emitOperands.begin(),
                emitOperands.end(),
                [](const DxilRewriteEmitOperand &lhs,
                   const DxilRewriteEmitOperand &rhs) {
                  return lhs.operandIndex < rhs.operandIndex;
                });
      if (emitOperands.size() != 2 || emitOperands[0].operandIndex != 0 ||
          emitOperands[1].operandIndex != 1) {
        return false;
      }

      llvm::Value *lhs = ResolveEmitOperandValue(emitOperands[0],
                                                 resultType,
                                                 builder,
                                                 module,
                                                 dxilModule,
                                                 match,
                                                 &temporaryValues);
      llvm::Value *rhs = ResolveEmitOperandValue(emitOperands[1],
                                                 resultType,
                                                 builder,
                                                 module,
                                                 dxilModule,
                                                 match,
                                                 &temporaryValues);
      if (lhs == nullptr || rhs == nullptr || lhs->getType() != resultType ||
          rhs->getType() != resultType) {
        return false;
      }

      emittedValue = builder.CreateBinOp(
          static_cast<llvm::Instruction::BinaryOps>(value.instructionOpcode),
          lhs,
          rhs);
    } else if (value.kind == DxilRewriteEmitValueKind::CastInstruction) {
      llvm::Type *resultType =
          GetEmitValueScalarType(value, module.getContext(), replacementTarget->getType());
      if (resultType == nullptr)
        return false;

      std::vector<DxilRewriteEmitOperand> emitOperands = value.operands;
      std::sort(emitOperands.begin(),
                emitOperands.end(),
                [](const DxilRewriteEmitOperand &lhs,
                   const DxilRewriteEmitOperand &rhs) {
                  return lhs.operandIndex < rhs.operandIndex;
                });
      if (emitOperands.size() != 1 || emitOperands[0].operandIndex != 0)
        return false;

      llvm::Value *source = ResolveEmitOperandValue(emitOperands[0],
                                                    nullptr,
                                                    builder,
                                                    module,
                                                    dxilModule,
                                                    match,
                                                    &temporaryValues);
      if (source == nullptr)
        return false;

      emittedValue = builder.CreateCast(
          static_cast<llvm::Instruction::CastOps>(value.castOpcode),
          source,
          resultType);
    } else if (value.kind == DxilRewriteEmitValueKind::CreateHandleForResource) {
      const hlsl::DxilResourceBase *resource =
          ResolveEmitResource(value.resourceName, value.resourceBinding, dxilModule);
      if (resource == nullptr)
        return false;

      llvm::Constant *resourceBindingConstant =
          BuildEmitResourceBindingConstant(dxilOp, dxilModule, value.resourceBinding);
      llvm::Function *createHandleFunction =
          dxilOp.GetOpFunc(hlsl::OP::OpCode::CreateHandleFromBinding,
                           dxilOp.GetHandleType());
      if (resourceBindingConstant == nullptr || createHandleFunction == nullptr)
        return false;

      llvm::Argument *opcodeArgument = GetFunctionArg(createHandleFunction, 0);
      llvm::Argument *indexArgument = GetFunctionArg(createHandleFunction, 2);
      llvm::Argument *nonUniformArgument = GetFunctionArg(createHandleFunction, 3);
      if (opcodeArgument == nullptr || indexArgument == nullptr ||
          nonUniformArgument == nullptr) {
        return false;
      }

      emittedValue = builder.CreateCall(
          createHandleFunction,
          {llvm::ConstantInt::get(
               opcodeArgument->getType(),
               static_cast<uint64_t>(hlsl::OP::OpCode::CreateHandleFromBinding)),
           resourceBindingConstant,
           llvm::ConstantInt::get(indexArgument->getType(),
                                  value.resourceBinding.GetBindPoint()),
           llvm::ConstantInt::get(nonUniformArgument->getType(), 0)});
    } else if (value.kind ==
               DxilRewriteEmitValueKind::AnnotateHandleForResource) {
      auto handleIt = temporaryValues.find(value.handleName);
      if (handleIt == temporaryValues.end())
        return false;

      const hlsl::DxilResourceBase *resource =
          ResolveEmitResource(value.resourceName, value.resourceBinding, dxilModule);
      if (resource == nullptr)
        return false;

      llvm::Constant *resourcePropsConstant =
          BuildEmitResourcePropertiesConstant(dxilOp, dxilModule, *resource);
      llvm::Function *annotateHandleFunction =
          dxilOp.GetOpFunc(hlsl::OP::OpCode::AnnotateHandle,
                           dxilOp.GetHandleType());
      if (resourcePropsConstant == nullptr || annotateHandleFunction == nullptr)
        return false;

      llvm::Argument *opcodeArgument = GetFunctionArg(annotateHandleFunction, 0);
      if (opcodeArgument == nullptr)
        return false;

      emittedValue = builder.CreateCall(
          annotateHandleFunction,
          {llvm::ConstantInt::get(
               opcodeArgument->getType(),
               static_cast<uint64_t>(hlsl::OP::OpCode::AnnotateHandle)),
           handleIt->second,
           resourcePropsConstant});
    } else {
      return false;
    }

    if (emittedValue == nullptr)
      return false;

    temporaryValues[value.name] = emittedValue;
  }

  auto replacementIt =
      temporaryValues.find(rule.emittedSequence.replacementValueName);
  if (replacementIt == temporaryValues.end())
    return false;

  result = DxilRewriteResult{};
  result.success = true;
  result.replacementValue = replacementIt->second;
  for (const std::string &captureName : rule.pruneCaptureNames) {
    llvm::Instruction *instruction = llvm::dyn_cast_or_null<llvm::Instruction>(
        match.GetCapture(captureName));
    if (instruction != nullptr)
      result.pruneRoots.push_back(instruction);
  }
  return true;
}

static bool BuildDeclarativeRewriteResult(const DxilRewriteRule &rule,
                                          llvm::Instruction *replacementTarget,
                                          llvm::IRBuilder<> &builder,
                                          llvm::Module &module,
                                          hlsl::DxilModule &dxilModule,
                                          const DxilMatchResult &match,
                                          DxilRewriteResult &result) {
  if (!rule.emittedSequence.values.empty()) {
    return BuildDeclarativeSequenceRewriteResult(rule,
                                                 replacementTarget,
                                                 builder,
                                                 module,
                                                 dxilModule,
                                                 match,
                                                 result);
  }

  result = DxilRewriteResult{};
  result.success = true;

  if (rule.emittedCall.enabled) {
    if (replacementTarget == nullptr)
      return false;

    hlsl::OP dxilOp(module.getContext(), &module);
    dxilOp.InitWithMinPrecision(dxilModule.GetUseMinPrecision());
    llvm::Function *emittedFunction =
        dxilOp.GetOpFunc(rule.emittedCall.dxilOpCode, replacementTarget->getType());
    if (emittedFunction == nullptr)
      return false;

    std::vector<DxilRewriteEmitOperand> emitOperands = rule.emittedCall.operands;
    std::sort(emitOperands.begin(),
              emitOperands.end(),
              [](const DxilRewriteEmitOperand &lhs,
                 const DxilRewriteEmitOperand &rhs) {
                return lhs.operandIndex < rhs.operandIndex;
              });

    std::vector<llvm::Value *> emittedArgs;
    emittedArgs.reserve(emittedFunction->arg_size());
    llvm::Argument *opcodeArgument = GetFunctionArg(emittedFunction, 0);
    if (opcodeArgument == nullptr)
      return false;
    emittedArgs.push_back(llvm::ConstantInt::get(
        opcodeArgument->getType(),
        static_cast<uint64_t>(rule.emittedCall.dxilOpCode)));

    size_t emitOperandIndex = 0;
    for (unsigned argIndex = 1; argIndex < emittedFunction->arg_size(); ++argIndex) {
      if (emitOperandIndex >= emitOperands.size() ||
          emitOperands[emitOperandIndex].operandIndex != argIndex) {
        return false;
      }

      const DxilRewriteEmitOperand &operand = emitOperands[emitOperandIndex++];
      llvm::Argument *arg = GetFunctionArg(emittedFunction, argIndex);
      if (arg == nullptr)
        return false;
      llvm::Type *argType = arg->getType();
      llvm::Value *argValue = nullptr;
      switch (operand.kind) {
      case DxilRewriteEmitOperandKind::Capture:
        argValue = match.GetCapture(operand.captureName);
        break;
      case DxilRewriteEmitOperandKind::ConstantInt: {
        llvm::IntegerType *integerType = llvm::dyn_cast<llvm::IntegerType>(argType);
        if (integerType == nullptr)
          return false;
        argValue = llvm::ConstantInt::get(integerType, operand.constantIntValue);
        break;
      }
      case DxilRewriteEmitOperandKind::ResourceHandle: {
        const hlsl::DxilResourceBase *resource = nullptr;
        if (!operand.resourceName.empty()) {
          resource = FindResourceByName(dxilModule,
                                        operand.resourceBinding.GetResourceClass(),
                                        operand.resourceName);
        }
        if (resource == nullptr && !operand.resourceBinding.IsAutoBinding()) {
          resource = FindResourceByBinding(dxilModule,
                                          operand.resourceBinding.GetResourceClass(),
                                          operand.resourceBinding.GetBindPoint(),
                                          operand.resourceBinding.GetSpace());
        }
        if (resource == nullptr)
          return false;

    llvm::Constant *resourceBindingConstant = hlsl::resource_helper::getAsConstant(
            operand.resourceBinding.GetDxilBinding(),
            dxilOp.GetResourceBindingType(),
      *dxilModule.GetShaderModel());
    llvm::Constant *resourcePropsConstant = hlsl::resource_helper::getAsConstant(
      hlsl::resource_helper::loadPropsFromResourceBase(resource),
            dxilOp.GetResourcePropertiesType(),
      *dxilModule.GetShaderModel());
        llvm::Function *createHandleFunction = dxilOp.GetOpFunc(
            hlsl::OP::OpCode::CreateHandleFromBinding,
            dxilOp.GetHandleType());
        llvm::Function *annotateHandleFunction = dxilOp.GetOpFunc(
            hlsl::OP::OpCode::AnnotateHandle,
            dxilOp.GetHandleType());
        if (resourceBindingConstant == nullptr || resourcePropsConstant == nullptr ||
            createHandleFunction == nullptr || annotateHandleFunction == nullptr) {
          return false;
        }

        llvm::Argument *createHandleOpcodeArgument = GetFunctionArg(createHandleFunction, 0);
        llvm::Argument *createHandleIndexArgument = GetFunctionArg(createHandleFunction, 2);
        llvm::Argument *createHandleNonUniformArgument =
          GetFunctionArg(createHandleFunction, 3);
        llvm::Argument *annotateHandleOpcodeArgument =
          GetFunctionArg(annotateHandleFunction, 0);
        if (createHandleOpcodeArgument == nullptr ||
          createHandleIndexArgument == nullptr ||
          createHandleNonUniformArgument == nullptr ||
          annotateHandleOpcodeArgument == nullptr) {
          return false;
        }

        llvm::Value *rawHandle = builder.CreateCall(
            createHandleFunction,
          {llvm::ConstantInt::get(createHandleOpcodeArgument->getType(),
                                    static_cast<uint64_t>(hlsl::OP::OpCode::CreateHandleFromBinding)),
             resourceBindingConstant,
           llvm::ConstantInt::get(createHandleIndexArgument->getType(),
                                    operand.resourceBinding.GetBindPoint()),
           llvm::ConstantInt::get(createHandleNonUniformArgument->getType(), 0)});
        argValue = builder.CreateCall(
            annotateHandleFunction,
          {llvm::ConstantInt::get(annotateHandleOpcodeArgument->getType(),
                                    static_cast<uint64_t>(hlsl::OP::OpCode::AnnotateHandle)),
             rawHandle,
             resourcePropsConstant});
        break;
      }
      case DxilRewriteEmitOperandKind::Undef:
        argValue = llvm::UndefValue::get(argType);
        break;
      }

      if (argValue == nullptr || argValue->getType() != argType)
        return false;

      emittedArgs.push_back(argValue);
    }

    llvm::Value *replacementValue = builder.CreateCall(emittedFunction, emittedArgs);
    if (rule.emittedCall.extractIndex >= 0) {
      replacementValue = builder.CreateExtractValue(
          replacementValue,
          static_cast<unsigned>(rule.emittedCall.extractIndex));
    }
    result.replacementValue = replacementValue;
  } else {
    if (rule.replacementCaptureName.empty())
      return false;

    llvm::Value *replacementValue = match.GetCapture(rule.replacementCaptureName);
    if (replacementValue == nullptr)
      return false;

    result.replacementValue = replacementValue;
  }

  for (const std::string &captureName : rule.pruneCaptureNames) {
    llvm::Instruction *instruction = llvm::dyn_cast_or_null<llvm::Instruction>(
        match.GetCapture(captureName));
    if (instruction != nullptr)
      result.pruneRoots.push_back(instruction);
  }

  return true;
}

bool ApplyDxilRewriteRules(llvm::Function &function,
                           llvm::Module &module,
                           hlsl::DxilModule &dxilModule,
                           const std::vector<DxilRewriteRule> &rules,
                           unsigned *appliedRuleCount) {
  unsigned appliedCount = 0;

  for (const DxilRewriteRule &rule : rules) {
    while (true) {
      std::vector<DxilMatchResult> matches;
      CollectDxilCallMatches(function, rule.pattern, matches, &dxilModule);
      bool appliedRule = false;

      for (const DxilMatchResult &match : matches) {
        DxilMatchResult effectiveMatch = match;
        bool missingBinding = false;
        for (const DxilCallPattern &bindingPattern : rule.bindingPatterns) {
          DxilMatchResult bindingMatch;
          if (!FindDxilCallMatch(function, bindingPattern, bindingMatch, &dxilModule) ||
              !MergeDxilMatchCaptures(bindingMatch.captures, effectiveMatch.captures)) {
            missingBinding = true;
            break;
          }
        }
        if (missingBinding)
          continue;

        if (rule.predicate && !rule.predicate(effectiveMatch))
          continue;

        llvm::Instruction *replaceInstruction =
            ResolveMatchInstruction(effectiveMatch, rule.replaceCaptureName);
        llvm::Instruction *rangeStartInstruction =
            ResolveMatchInstruction(effectiveMatch, rule.rangeStartCaptureName);
        llvm::Instruction *rangeEndInstruction =
            ResolveMatchInstruction(effectiveMatch, rule.rangeEndCaptureName);

        llvm::Instruction *anchorInstruction = nullptr;
        if (rule.mode == DxilRewriteMode::After) {
          anchorInstruction = rangeEndInstruction != nullptr ? rangeEndInstruction
                                                             : (replaceInstruction != nullptr
                                                                    ? replaceInstruction
                                                                    : effectiveMatch.rootCall);
        } else {
          anchorInstruction = rangeStartInstruction != nullptr ? rangeStartInstruction
                                                               : (replaceInstruction != nullptr
                                                                      ? replaceInstruction
                                                                      : effectiveMatch.rootCall);
        }
        if (anchorInstruction == nullptr)
          return false;

        llvm::IRBuilder<> builder(anchorInstruction->getContext());
        if (rule.mode == DxilRewriteMode::After) {
          llvm::BasicBlock::iterator insertIt(anchorInstruction);
          ++insertIt;
          builder.SetInsertPoint(anchorInstruction->getParent(), insertIt);
        } else {
          builder.SetInsertPoint(anchorInstruction);
        }

        llvm::Instruction *replacementTarget =
          replaceInstruction != nullptr ? replaceInstruction : effectiveMatch.rootCall;

        DxilRewriteResult rewriteResult;
        if (rule.replacementCallback) {
          rewriteResult =
              rule.replacementCallback(effectiveMatch, builder, module, dxilModule);
        } else if (!BuildDeclarativeRewriteResult(rule,
                                                  replacementTarget,
                                                  builder,
                                                  module,
                                                  dxilModule,
                                                  effectiveMatch,
                                                  rewriteResult)) {
          return false;
        }

        if (!rewriteResult.success)
          return false;

        std::vector<llvm::Instruction *> pruneCandidates =
            std::move(rewriteResult.pruneRoots);

        if (rule.mode == DxilRewriteMode::Replace ||
            rule.mode == DxilRewriteMode::ReplaceRange) {
          if (replacementTarget == nullptr)
            return false;

          if (!rewriteResult.handledReplacement) {
            if (rewriteResult.replacementValue == nullptr)
              return false;
            replacementTarget->replaceAllUsesWith(rewriteResult.replacementValue);
          }

          AppendUniqueInstruction(pruneCandidates, replacementTarget);
        }

        if (rule.mode == DxilRewriteMode::ReplaceRange) {
          llvm::Instruction *rangeStart =
              rangeStartInstruction != nullptr ? rangeStartInstruction
                                              : (replaceInstruction != nullptr
                                                     ? replaceInstruction
                                                     : effectiveMatch.rootCall);
          llvm::Instruction *rangeEnd =
              rangeEndInstruction != nullptr ? rangeEndInstruction : rangeStart;

          std::vector<llvm::Instruction *> rangeInstructions;
          if (!CollectInstructionRange(rangeStart, rangeEnd, rangeInstructions))
            return false;

          for (llvm::Instruction *instruction : rangeInstructions)
            AppendUniqueInstruction(pruneCandidates, instruction);
        }

        if (rule.pruneDeadInstructions)
          PruneCandidateInstructions(pruneCandidates);

        ++appliedCount;
        appliedRule = true;
        break;
      }

      if (!appliedRule)
        break;
    }
  }

  if (appliedRuleCount != nullptr)
    *appliedRuleCount = appliedCount;

  return true;
}

static void AppendRecipeDiagnostic(DxilRecipeContext &context,
                                   const std::string &message) {
  context.diagnostics.push_back(message);
  if (context.traceEnabled)
    std::cerr << message << "\n";
}

static DxilRecipeStepResult FailRecipeStep(DxilRecipeContext &context,
                                           const std::string &message) {
  context.lastError = message;
  AppendRecipeDiagnostic(context, message);
  DxilRecipeStepResult result;
  result.success = false;
  return result;
}

DxilRecipeStepResult MakeRecipeStepSuccess(bool changed,
                                           unsigned matchCount,
                                           bool invalidatedAnalyses) {
  DxilRecipeStepResult result;
  result.success = true;
  result.changed = changed;
  result.matchCount = matchCount;
  result.invalidatedAnalyses = invalidatedAnalyses;
  return result;
}

DxilRecipeStepResult MakeRecipeStepFailure(DxilRecipeContext &context,
                                           std::string message) {
  return FailRecipeStep(context, message);
}

static DxilRecipeStepResult ApplyRecipeRewriteRules(
    DxilRecipeContext &context,
    const std::vector<DxilRewriteRule> &rules,
    DxilRecipeRuleApplicationMode mode,
    bool required,
    const std::string &stepName) {
  if (context.module == nullptr || context.dxilModule == nullptr) {
    return FailRecipeStep(context,
                          stepName + ": recipe context is missing module state");
  }

  context.entryFunction = context.dxilModule->GetEntryFunction();
  if (context.entryFunction == nullptr) {
    return FailRecipeStep(context,
                          stepName + ": failed to locate DXIL entry function");
  }

  unsigned totalMatches = 0;
  do {
    unsigned appliedRuleCount = 0;
    if (!ApplyDxilRewriteRules(*context.entryFunction,
                               *context.module,
                               *context.dxilModule,
                               rules,
                               &appliedRuleCount)) {
      return FailRecipeStep(context,
                            stepName + ": rewrite rule application failed");
    }

    totalMatches += appliedRuleCount;
    if (mode == DxilRecipeRuleApplicationMode::Once || appliedRuleCount == 0)
      break;
  } while (true);

  if (required && totalMatches == 0) {
    return FailRecipeStep(context,
                          stepName + ": no rewrite matches were applied");
  }

  DxilRecipeStepResult result;
  result.changed = totalMatches != 0;
  result.matchCount = totalMatches;
  result.invalidatedAnalyses = totalMatches != 0;
  return result;
}

DxilRecipeStep MakeCustomRecipeStep(std::string name,
                                    DxilRecipeStepExecutor execute) {
  return DxilRecipeStep{std::move(name), std::move(execute)};
}

DxilRecipeStep MakeAddTextureStep(std::string id, TextureResourceDesc desc) {
  return DxilRecipeStep{
      "add_texture:" + id,
      [id = std::move(id), desc](DxilRecipeContext &context) {
        if (context.module == nullptr || context.dxilModule == nullptr) {
          return FailRecipeStep(context,
                                "add_texture: recipe context is missing module state");
        }

        TextureResourceDesc resolvedDesc = desc;
        if (resolvedDesc.binding.IsAutoBinding()) {
          resolvedDesc.binding.SetBindPoint(
              FindNextAvailableBinding(context.dxilModule->GetSRVs(),
                                       resolvedDesc.binding.GetSpace()));
        }

        if (!AddTextureSRV(*context.module, *context.dxilModule, resolvedDesc)) {
          return FailRecipeStep(context,
                                "add_texture: failed to add texture resource '" +
                                    id + "'");
        }

        context.textures[id] = resolvedDesc;
        DxilRecipeStepResult result;
        result.changed = true;
        result.invalidatedAnalyses = true;
        return result;
      }};
}

DxilRecipeStep MakeAddTextureUAVStep(std::string id, TextureResourceDesc desc) {
  return DxilRecipeStep{
      "add_texture_uav:" + id,
      [id = std::move(id), desc](DxilRecipeContext &context) {
        if (context.module == nullptr || context.dxilModule == nullptr) {
          return FailRecipeStep(context,
                                "add_texture_uav: recipe context is missing module state");
        }

        TextureResourceDesc resolvedDesc = desc;
        if (resolvedDesc.binding.IsAutoBinding()) {
          resolvedDesc.binding.SetBindPoint(
              FindNextAvailableBinding(context.dxilModule->GetUAVs(),
                                       resolvedDesc.binding.GetSpace()));
        }

        if (!AddTextureUAV(*context.module, *context.dxilModule, resolvedDesc)) {
          return FailRecipeStep(context,
                                "add_texture_uav: failed to add texture UAV '" +
                                    id + "'");
        }

        context.uavs[id] = resolvedDesc;
        DxilRecipeStepResult result;
        result.changed = true;
        result.invalidatedAnalyses = true;
        return result;
      }};
}

DxilRecipeStep MakeAddCBufferStep(std::string id, CBufferDesc desc) {
  std::shared_ptr<CBufferSchema> schemaCopy;
  if (desc.schema != nullptr)
    schemaCopy = std::make_shared<CBufferSchema>(*desc.schema);

  return DxilRecipeStep{
      "add_cbuffer:" + id,
      [id = std::move(id), desc, schemaCopy](DxilRecipeContext &context) {
        if (context.module == nullptr || context.dxilModule == nullptr) {
          return FailRecipeStep(context,
                                "add_cbuffer: recipe context is missing module state");
        }

        CBufferDesc resolvedDesc = desc;
        if (schemaCopy)
          resolvedDesc.schema = schemaCopy.get();
        if (resolvedDesc.binding.IsAutoBinding()) {
          resolvedDesc.binding.SetBindPoint(
              FindNextAvailableBinding(context.dxilModule->GetCBuffers(),
                                       resolvedDesc.binding.GetSpace()));
        }

        if (!AddCBuffer(*context.module, *context.dxilModule, resolvedDesc)) {
          return FailRecipeStep(context,
                                "add_cbuffer: failed to add cbuffer '" + id + "'");
        }

        context.cbuffers[id] = resolvedDesc;
        DxilRecipeStepResult result;
        result.changed = true;
        result.invalidatedAnalyses = true;
        return result;
      }};
}

DxilRecipeStep MakeAddSamplerStep(std::string id, SamplerDesc desc) {
  return DxilRecipeStep{
      "add_sampler:" + id,
      [id = std::move(id), desc](DxilRecipeContext &context) {
        if (context.module == nullptr || context.dxilModule == nullptr) {
          return FailRecipeStep(context,
                                "add_sampler: recipe context is missing module state");
        }

        SamplerDesc resolvedDesc = desc;
        if (resolvedDesc.binding.IsAutoBinding()) {
          resolvedDesc.binding.SetBindPoint(
              FindNextAvailableBinding(context.dxilModule->GetSamplers(),
                                       resolvedDesc.binding.GetSpace()));
        }

        if (!AddSampler(*context.module, *context.dxilModule, resolvedDesc)) {
          return FailRecipeStep(context,
                                "add_sampler: failed to add sampler '" + id + "'");
        }

        context.samplers[id] = resolvedDesc;
        DxilRecipeStepResult result;
        result.changed = true;
        result.invalidatedAnalyses = true;
        return result;
      }};
}

DxilRecipeStep MakeApplyRewriteRulesStep(
    std::string name,
    std::vector<DxilRewriteRule> rules,
    DxilRecipeRuleApplicationMode mode,
    bool required) {
  return DxilRecipeStep{
      name,
      [rules = std::move(rules), mode, required, name](DxilRecipeContext &context) {
        return ApplyRecipeRewriteRules(context, rules, mode, required, name);
      }};
}

DxilRecipeStep MakeRefreshResourcesStep(std::string name) {
  return DxilRecipeStep{
      name,
      [name](DxilRecipeContext &context) {
        if (context.dxilModule == nullptr) {
          return FailRecipeStep(context,
                                name + ": recipe context is missing DXIL module");
        }

        RefreshDxilAfterResourceMutation(*context.dxilModule, context.traceEnabled);
        DxilRecipeStepResult result;
        result.changed = true;
        return result;
      }};
}

DxilRecipeStep MakePruneDeadCodeStep(std::string name) {
  return DxilRecipeStep{
      name,
      [name](DxilRecipeContext &context) {
        if (context.dxilModule == nullptr) {
          return FailRecipeStep(context,
                                name + ": recipe context is missing DXIL module");
        }

        context.entryFunction = context.dxilModule->GetEntryFunction();
        if (context.entryFunction == nullptr) {
          return FailRecipeStep(context,
                                name + ": failed to locate DXIL entry function");
        }

        PruneFunctionDeadCode(*context.entryFunction);
        return MakeRecipeStepSuccess(true, 0, true);
      }};
}

DxilRecipeStep MakeVerifyModuleStep(std::string name) {
  return DxilRecipeStep{
      name,
      [name](DxilRecipeContext &context) {
        if (context.module == nullptr) {
          return FailRecipeStep(context,
                                name + ": recipe context is missing module");
        }

        std::string verifyErrors;
        llvm::raw_string_ostream errorStream(verifyErrors);
        if (llvm::verifyModule(*context.module, &errorStream)) {
          errorStream.flush();
          return FailRecipeStep(context,
                                name + ": module verification failed: " + verifyErrors);
        }

        return DxilRecipeStepResult{};
      }};
}

DxilRecipeStep MakeExpectTextureStep(std::string id, std::string name) {
  return DxilRecipeStep{
      name,
      [id = std::move(id), name](DxilRecipeContext &context) {
        if (context.dxilModule == nullptr) {
          return FailRecipeStep(context,
                                name + ": recipe context is missing DXIL module");
        }

        auto textureIt = context.textures.find(id);
        if (textureIt == context.textures.end()) {
          return FailRecipeStep(context,
                                name + ": unknown texture id '" + id + "'");
        }

        const TextureResourceDesc &desc = textureIt->second;
        const hlsl::DxilResourceBase *resource = FindResourceByBinding(
            *context.dxilModule,
            hlsl::DXIL::ResourceClass::SRV,
          desc.binding.GetBindPoint(),
          desc.binding.GetSpace());
        if (resource == nullptr || resource->GetGlobalName() != desc.name) {
          return FailRecipeStep(context,
                                name + ": expected texture resource '" + desc.name +
                                    "' is missing");
        }

        return DxilRecipeStepResult{};
      }};
}

DxilRecipeStep MakeExpectCBufferStep(std::string id, std::string name) {
  return DxilRecipeStep{
      name,
      [id = std::move(id), name](DxilRecipeContext &context) {
        if (context.dxilModule == nullptr) {
          return FailRecipeStep(context,
                                name + ": recipe context is missing DXIL module");
        }

        auto cbufferIt = context.cbuffers.find(id);
        if (cbufferIt == context.cbuffers.end()) {
          return FailRecipeStep(context,
                                name + ": unknown cbuffer id '" + id + "'");
        }

        const CBufferDesc &desc = cbufferIt->second;
        const hlsl::DxilResourceBase *resource = FindResourceByBinding(
            *context.dxilModule,
            hlsl::DXIL::ResourceClass::CBuffer,
          desc.binding.GetBindPoint(),
          desc.binding.GetSpace());
        if (resource == nullptr || resource->GetGlobalName() != desc.name) {
          return FailRecipeStep(context,
                                name + ": expected cbuffer '" + desc.name +
                                    "' is missing");
        }

        return DxilRecipeStepResult{};
      }};
}

DxilRecipeStep MakeExpectTextureUAVStep(std::string id, std::string name) {
  return DxilRecipeStep{
      name,
      [id = std::move(id), name](DxilRecipeContext &context) {
        if (context.dxilModule == nullptr) {
          return FailRecipeStep(context,
                                name + ": recipe context is missing DXIL module");
        }

        auto textureIt = context.uavs.find(id);
        if (textureIt == context.uavs.end()) {
          return FailRecipeStep(context,
                                name + ": unknown UAV id '" + id + "'");
        }

        const TextureResourceDesc &desc = textureIt->second;
        const hlsl::DxilResourceBase *resource = FindResourceByBinding(
            *context.dxilModule,
            hlsl::DXIL::ResourceClass::UAV,
            desc.binding.GetBindPoint(),
            desc.binding.GetSpace());
        if (resource == nullptr || resource->GetGlobalName() != desc.name) {
          return FailRecipeStep(context,
                                name + ": expected texture UAV '" + desc.name +
                                    "' is missing");
        }

        return DxilRecipeStepResult{};
      }};
}

bool ExecuteDxilRecipe(const DxilRecipe &recipe,
                       Module &module,
                       hlsl::DxilModule &dxilModule,
                       DxilRecipeContext *outContext,
                       bool traceEnabled) {
  DxilRecipeExecutionOptions options;
  options.traceEnabled = traceEnabled;
  return ExecuteDxilRecipe(recipe,
                           module,
                           dxilModule,
                           options,
                           outContext);
}

bool ExecuteDxilRecipe(const DxilRecipe &recipe,
                       Module &module,
                       hlsl::DxilModule &dxilModule,
                       const DxilRecipeExecutionOptions &options,
                       DxilRecipeContext *outContext) {
  DxilRecipeContext context;
  context.module = &module;
  context.dxilModule = &dxilModule;
  context.entryFunction = dxilModule.GetEntryFunction();
  context.traceEnabled = options.traceEnabled;
  context.inputs = options.inputs;
  context.state = options.initialState;

  for (const DxilRecipeStep &step : recipe.GetSteps()) {
    if (!step.execute) {
      context.lastError = "recipe step has no executor: " + step.name;
      if (outContext != nullptr)
        *outContext = context;
      return false;
    }

    DxilRecipeStepResult result = step.execute(context);
    context.totalRuleMatches += result.matchCount;
    context.entryFunction = dxilModule.GetEntryFunction();
    if (!result.success) {
      if (outContext != nullptr)
        *outContext = context;
      return false;
    }
  }

  if (outContext != nullptr)
    *outContext = context;
  return true;
}

static bool SupportsTextureSampleInjection(const TextureResourceDesc &desc) {
  return desc.binding.GetResourceClass() == hlsl::DXIL::ResourceClass::SRV &&
         desc.kind == hlsl::DXIL::ResourceKind::Texture2D &&
         desc.elementKind == hlsl::DXIL::ComponentType::F32 && desc.vectorWidth == 4 &&
         !desc.isReadWrite;
}

bool InjectTextureSampleIntoEntryPoint(Module &M,
                                       hlsl::DxilModule &DM,
                                       const TextureResourceDesc &desc,
                                       bool traceEnabled) {
  if (!SupportsTextureSampleInjection(desc)) {
    TraceMessage(traceEnabled,
                 "sample injection: unsupported texture shape, skipping");
    return true;
  }

  llvm::Function *entryFunction = DM.GetEntryFunction();
  if (entryFunction == nullptr || entryFunction->empty()) {
    std::cerr << "Failed to locate the DXIL entry function for texture sample injection.\n";
    return false;
  }

  DxilCallPattern sampleChainPattern;
  sampleChainPattern.matchDxilOpCode = true;
  sampleChainPattern.dxilOpCode = hlsl::OP::OpCode::Sample;
  sampleChainPattern.captureName = "prototypeSampleCall";
  DxilOperandPattern sampledRawTextureHandle;
  sampledRawTextureHandle.operandIndex = 1;
  sampledRawTextureHandle.kind = DxilOperandPatternKind::DxOpCall;
  sampledRawTextureHandle.captureName = "prototypeRawTextureHandle";
  sampledRawTextureHandle.matchDxilOpCode = true;
  sampledRawTextureHandle.dxilOpCode = hlsl::OP::OpCode::CreateHandleFromBinding;

  DxilOperandPattern sampledTextureHandle;
  sampledTextureHandle.operandIndex = 1;
  sampledTextureHandle.kind = DxilOperandPatternKind::DxOpCall;
  sampledTextureHandle.captureName = "prototypeAnnotatedTextureHandle";
  sampledTextureHandle.matchDxilOpCode = true;
  sampledTextureHandle.dxilOpCode = hlsl::OP::OpCode::AnnotateHandle;
  sampledTextureHandle.operandPatterns = {sampledRawTextureHandle};

  DxilOperandPattern sampledSamplerHandle;
  sampledSamplerHandle.operandIndex = 2;
  sampledSamplerHandle.kind = DxilOperandPatternKind::Any;
  sampledSamplerHandle.captureName = "prototypeSamplerHandle";

  sampleChainPattern.operandPatterns = {
      sampledTextureHandle,
      sampledSamplerHandle,
  };

  DxilMatchResult sampleChainMatch;
  if (!FindDxilCallMatch(*entryFunction, sampleChainPattern, sampleChainMatch)) {
    std::cerr << "Failed to find an existing sample.f32 call to clone for the injected texture.\n";
    return false;
  }

  llvm::CallInst *prototypeSampleCall = sampleChainMatch.rootCall;
  llvm::CallInst *prototypeAnnotatedTextureHandle =
      sampleChainMatch.GetCallCapture("prototypeAnnotatedTextureHandle");
  llvm::CallInst *prototypeRawTextureHandle =
      sampleChainMatch.GetCallCapture("prototypeRawTextureHandle");
  llvm::Value *prototypeSamplerHandle =
      sampleChainMatch.GetCapture("prototypeSamplerHandle");
  if (prototypeAnnotatedTextureHandle == nullptr ||
      prototypeRawTextureHandle == nullptr ||
      !IsDxOpCall(*prototypeAnnotatedTextureHandle, hlsl::OP::OpCode::AnnotateHandle) ||
      !IsDxOpCall(*prototypeRawTextureHandle, hlsl::OP::OpCode::CreateHandleFromBinding) ||
      prototypeSamplerHandle == nullptr) {
    std::cerr << "Failed to resolve the existing texture sample handle chain.\n";
    return false;
  }

  llvm::CallInst *redStore = nullptr;
  llvm::CallInst *greenStore = nullptr;
  llvm::CallInst *blueStore = nullptr;
  for (llvm::BasicBlock &basicBlock : *entryFunction) {
    for (llvm::Instruction &instruction : basicBlock) {
      if (!IsDxOpCall(instruction, "dx.op.storeOutput.f32"))
        continue;

      llvm::CallInst *storeCall = llvm::cast<llvm::CallInst>(&instruction);
      if (!IsConstantIntValue(storeCall->getArgOperand(1), 0) ||
          !IsConstantIntValue(storeCall->getArgOperand(2), 0)) {
        continue;
      }

      if (IsConstantIntValue(storeCall->getArgOperand(3), 0))
        redStore = storeCall;
      else if (IsConstantIntValue(storeCall->getArgOperand(3), 1))
        greenStore = storeCall;
      else if (IsConstantIntValue(storeCall->getArgOperand(3), 2))
        blueStore = storeCall;
    }
  }

  if (redStore == nullptr || greenStore == nullptr || blueStore == nullptr) {
    std::cerr << "Failed to locate the final RGB output stores for texture sample injection.\n";
    return false;
  }

  llvm::IRBuilder<> sampleBuilder(prototypeSampleCall);
  llvm::Constant *resBind = CreateResBindConstant(
      prototypeRawTextureHandle->getArgOperand(1)->getType(),
      desc.binding.GetBindPoint(),
      desc.binding.GetSpace(),
      0);
  if (resBind == nullptr) {
    std::cerr << "Failed to create a resource binding constant for the injected texture sample.\n";
    return false;
  }

  llvm::Value *newRawTextureHandle = sampleBuilder.CreateCall(
      prototypeRawTextureHandle->getCalledFunction(),
      {prototypeRawTextureHandle->getArgOperand(0), resBind,
       llvm::ConstantInt::get(prototypeRawTextureHandle->getArgOperand(2)->getType(),
                              desc.binding.GetBindPoint()),
       prototypeRawTextureHandle->getArgOperand(3)});

  llvm::Value *newAnnotatedTextureHandle = sampleBuilder.CreateCall(
      prototypeAnnotatedTextureHandle->getCalledFunction(),
      {prototypeAnnotatedTextureHandle->getArgOperand(0), newRawTextureHandle,
       prototypeAnnotatedTextureHandle->getArgOperand(2)});

  std::vector<llvm::Value *> sampleArgs;
  const unsigned sampleArgCount = prototypeSampleCall->getNumArgOperands();
  sampleArgs.reserve(sampleArgCount);
  for (unsigned argIndex = 0; argIndex < sampleArgCount; ++argIndex) {
    sampleArgs.push_back(argIndex == 1 ? newAnnotatedTextureHandle
                                       : prototypeSampleCall->getArgOperand(argIndex));
  }

  llvm::Value *newSample = sampleBuilder.CreateCall(
      prototypeSampleCall->getCalledFunction(), sampleArgs);
    llvm::Value *newSampleRed = sampleBuilder.CreateExtractValue(newSample, 0);
    llvm::Value *newSampleGreen = sampleBuilder.CreateExtractValue(newSample, 1);
    llvm::Value *newSampleBlue = sampleBuilder.CreateExtractValue(newSample, 2);

  llvm::Constant *blendWeight = llvm::ConstantFP::get(
      llvm::Type::getFloatTy(M.getContext()), 0.25f);
  llvm::Value *sampleContributions[] = {newSampleRed, newSampleGreen,
                                        newSampleBlue};
  llvm::CallInst *stores[] = {redStore, greenStore, blueStore};
  for (unsigned channelIndex = 0; channelIndex < 3; ++channelIndex) {
    llvm::IRBuilder<> storeBuilder(stores[channelIndex]);
    llvm::Value *scaledSample = storeBuilder.CreateFMul(
        sampleContributions[channelIndex], blendWeight);
    llvm::Value *blendedOutput = storeBuilder.CreateFAdd(
        stores[channelIndex]->getArgOperand(4), scaledSample);
    stores[channelIndex]->setArgOperand(4, blendedOutput);
  }

  TraceMessage(traceEnabled, "sample injection: cloned a texture sample into the entry function");
  return true;
}

bool LoadDxilContainerForMutation(const void *containerData,
                                  size_t containerSize,
                                  DxilLoadedShaderState &shader,
                                  bool restoreReflection) {
  shader.module.reset();
  shader.reflectionContext.reset();
  shader.dxilModule = nullptr;

  if (containerData == nullptr && containerSize != 0) {
    std::cerr << "LoadDxilContainerForMutation received a null container pointer.\n";
    return false;
  }

  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(containerData);
  shader.inputBytes.assign(bytes, bytes + containerSize);

  DxilProgramBitcode dxilBitcode;
  if (!ExtractProgramBitcodeFromContainerPart(shader.inputBytes,
                                              hlsl::DFCC_DXIL,
                                              dxilBitcode)) {
    std::cerr << "Failed to extract DXIL program bitcode.\n";
    return false;
  }

  shader.module = ParseDxilBitcode(dxilBitcode.ptr,
                                   dxilBitcode.size,
                                   shader.context);
  if (!shader.module) {
    std::cerr << "Failed to parse DXIL bitcode.\n";
    return false;
  }

  if (!LoadDxilState(*shader.module, shader.dxilModule) ||
      shader.dxilModule == nullptr) {
    std::cerr << "Failed to load DxilModule state.\n";
    return false;
  }

  if (restoreReflection) {
    shader.reflectionContext = std::make_unique<LLVMContext>();
    RestoreOriginalResourceReflection(shader.inputBytes,
                                      *shader.dxilModule,
                                      *shader.reflectionContext);
  }

  return true;
}

bool LoadDxilContainerForMutation(const std::vector<uint8_t> &containerBytes,
                                  DxilLoadedShaderState &shader,
                                  bool restoreReflection) {
  return LoadDxilContainerForMutation(containerBytes.data(),
                                      containerBytes.size(),
                                      shader,
                                      restoreReflection);
}

bool ReloadDxilContainerFromMemory(const std::vector<uint8_t> &containerBytes,
                                   LLVMContext &context,
                                   std::unique_ptr<Module> &module,
                                   hlsl::DxilModule *&dxilModule) {
  DxilProgramBitcode patchedDxilBitcode;
  if (!ExtractProgramBitcodeFromContainerPart(containerBytes,
                                              hlsl::DFCC_DXIL,
                                              patchedDxilBitcode)) {
    std::cerr << "Failed to extract DXIL bitcode from patched container.\n";
    return false;
  }

  module = ParseDxilBitcode(patchedDxilBitcode.ptr,
                            patchedDxilBitcode.size,
                            context);
  if (!module) {
    std::cerr << "Failed to parse patched DXIL bitcode.\n";
    return false;
  }

  if (!LoadDxilState(*module, dxilModule) || dxilModule == nullptr) {
    std::cerr << "Failed to load DxilModule from patched container.\n";
    return false;
  }

  return true;
}

bool PatchDxilContainerInMemory(const DxilRecipe &recipe,
                                const void *inputData,
                                size_t inputSize,
                                std::vector<uint8_t> &outputContainer,
                                const DxilContainerPatchOptions &options,
                                DxilRecipeContext *outContext) {
  ScopedPatchCoInitialize coinit;
  const bool traceEnabled = options.recipeExecutionOptions.traceEnabled;

  TraceMessage(traceEnabled, "patch: load container");

  DxilLoadedShaderState shader;
  if (!LoadDxilContainerForMutation(inputData,
                                    inputSize,
                                    shader,
                                    options.restoreReflection)) {
    return false;
  }

  TraceMessage(traceEnabled, "patch: execute recipe");
  if (!ExecuteDxilRecipe(recipe,
                         *shader.module,
                         *shader.dxilModule,
                         options.recipeExecutionOptions,
                         outContext)) {
    return false;
  }

  if (options.refreshResources) {
    TraceMessage(traceEnabled, "patch: refresh resources");
    RefreshDxilAfterResourceMutation(*shader.dxilModule,
                                     options.recipeExecutionOptions.traceEnabled);
  }

  TraceMessage(traceEnabled, "patch: verify module");
  if (options.verifyModule && !VerifyModuleOrReportInternal(*shader.module))
    return false;

  TraceMessage(traceEnabled, "patch: serialize container");
  return SerializePatchedContainer(*shader.dxilModule,
                                   SerializeModuleToBitcode(*shader.module),
                                   outputContainer);
}

bool PatchDxilContainerInMemory(const DxilRecipe &recipe,
                                const std::vector<uint8_t> &inputContainer,
                                std::vector<uint8_t> &outputContainer,
                                const DxilContainerPatchOptions &options,
                                DxilRecipeContext *outContext) {
  return PatchDxilContainerInMemory(recipe,
                                    inputContainer.data(),
                                    inputContainer.size(),
                                    outputContainer,
                                    options,
                                    outContext);
}

static std::string TrimRecipeText(std::string value) {
  const size_t first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return std::string();

  const size_t last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

static std::vector<std::string> TokenizeRecipeLine(const std::string &line) {
  std::vector<std::string> tokens;
  std::string current;
  bool inQuotes = false;

  for (char ch : line) {
    if (ch == '"') {
      inQuotes = !inQuotes;
      continue;
    }

    if (!inQuotes && (ch == ' ' || ch == '\t')) {
      if (!current.empty()) {
        tokens.push_back(current);
        current.clear();
      }
      continue;
    }

    current.push_back(ch);
  }

  if (!current.empty())
    tokens.push_back(current);

  return tokens;
}

static bool ParseRecipeAssignments(
    const std::vector<std::string> &tokens,
    size_t startIndex,
    std::unordered_map<std::string, std::string> &assignments,
    std::string &error) {
  for (size_t index = startIndex; index < tokens.size(); ++index) {
    const size_t equalsPos = tokens[index].find('=');
    if (equalsPos == std::string::npos || equalsPos == 0 ||
        equalsPos + 1 >= tokens[index].size()) {
      error = "expected key=value token, got '" + tokens[index] + "'";
      return false;
    }

    assignments[tokens[index].substr(0, equalsPos)] =
        tokens[index].substr(equalsPos + 1);
  }

  return true;
}

static const std::string *FindRecipeAssignment(
    const std::unordered_map<std::string, std::string> &assignments,
    const std::string &key) {
  const auto it = assignments.find(key);
  return it != assignments.end() ? &it->second : nullptr;
}

static bool ParseRecipeUnsignedValue(const std::string &text,
                                     unsigned &value,
                                     std::string &error) {
  try {
    size_t consumed = 0;
    const unsigned long parsed = std::stoul(text, &consumed, 0);
    if (consumed != text.size()) {
      error = "invalid unsigned integer '" + text + "'";
      return false;
    }
    value = static_cast<unsigned>(parsed);
    return true;
  } catch (const std::exception &) {
    error = "invalid unsigned integer '" + text + "'";
    return false;
  }
}

static bool ParseRecipeBoolValue(const std::string &text,
                                 bool &value,
                                 std::string &error) {
  if (text == "true" || text == "1") {
    value = true;
    return true;
  }

  if (text == "false" || text == "0") {
    value = false;
    return true;
  }

  error = "invalid boolean '" + text + "'";
  return false;
}

static std::string LowercaseRecipeToken(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return text;
}

static bool ParseRecipeOpCode(const std::string &text,
                              hlsl::OP::OpCode &opCode,
                              std::string &error) {
  const std::string lowered = LowercaseRecipeToken(text);
  for (unsigned index = 0;
       index < static_cast<unsigned>(hlsl::OP::OpCode::NumOpCodes);
       ++index) {
    const hlsl::OP::OpCode candidate = static_cast<hlsl::OP::OpCode>(index);
    const char *candidateName = hlsl::OP::GetOpCodeName(candidate);
    if (candidateName == nullptr)
      continue;
    if (LowercaseRecipeToken(candidateName) == lowered) {
      opCode = candidate;
      return true;
    }
  }

  error = "unsupported dx.op opcode '" + text + "'";
  return false;
}

static bool ParseRecipeInstructionOpcode(const std::string &text,
                                         unsigned &opcode,
                                         std::string &error) {
  const std::string lowered = LowercaseRecipeToken(text);
  for (unsigned index = 1; index < llvm::Instruction::OtherOpsEnd; ++index) {
    const char *candidateName = llvm::Instruction::getOpcodeName(index);
    if (candidateName == nullptr)
      continue;
    if (LowercaseRecipeToken(candidateName) == lowered) {
      opcode = index;
      return true;
    }
  }

  error = "unsupported LLVM instruction opcode '" + text + "'";
  return false;
}

static bool TryResolveParsedRecipeResourceRef(
    const std::string &id,
    const std::unordered_map<std::string, TextureResourceDesc> &textures,
    const std::unordered_map<std::string, TextureResourceDesc> &uavs,
    const std::unordered_map<std::string, CBufferDesc> &cbuffers,
    const std::unordered_map<std::string, SamplerDesc> &samplers,
    ParsedRecipeResourceRef &resourceRef) {
  resourceRef = ParsedRecipeResourceRef{};

  auto textureIt = textures.find(id);
  if (textureIt != textures.end()) {
    resourceRef.found = true;
    resourceRef.resourceName = textureIt->second.name;
    resourceRef.binding = textureIt->second.binding;
    resourceRef.binding.SetResourceClass(hlsl::DXIL::ResourceClass::SRV);
    return true;
  }

  auto uavIt = uavs.find(id);
  if (uavIt != uavs.end()) {
    resourceRef.found = true;
    resourceRef.resourceName = uavIt->second.name;
    resourceRef.binding = uavIt->second.binding;
    resourceRef.binding.SetResourceClass(hlsl::DXIL::ResourceClass::UAV);
    return true;
  }

  auto cbufferIt = cbuffers.find(id);
  if (cbufferIt != cbuffers.end()) {
    resourceRef.found = true;
    resourceRef.resourceName = cbufferIt->second.name;
    resourceRef.binding = cbufferIt->second.binding;
    resourceRef.binding.SetResourceClass(hlsl::DXIL::ResourceClass::CBuffer);
    return true;
  }

  auto samplerIt = samplers.find(id);
  if (samplerIt != samplers.end()) {
    resourceRef.found = true;
    resourceRef.resourceName = samplerIt->second.name;
    resourceRef.binding = samplerIt->second.binding;
    resourceRef.binding.SetResourceClass(hlsl::DXIL::ResourceClass::Sampler);
    return true;
  }

  return true;
}

static bool ParseRecipeRewriteMode(const std::string &text,
                                   DxilRewriteMode &mode,
                                   std::string &error) {
  static const RecipeParseEntry<DxilRewriteMode> kRewriteModeEntries[] = {
      {"Replace", DxilRewriteMode::Replace},
      {"ReplaceRange", DxilRewriteMode::ReplaceRange},
  };
  if (ParseRecipeValueByTable(text, mode, kRewriteModeEntries))
    return true;

  error = "unsupported rewrite mode '" + text + "'";
  return false;
}

static bool ParseRecipeRuleApplicationMode(
    const std::string &text,
    DxilRecipeRuleApplicationMode &mode,
    std::string &error) {
  static const RecipeParseEntry<DxilRecipeRuleApplicationMode>
      kRuleApplicationModeEntries[] = {
          {"Once", DxilRecipeRuleApplicationMode::Once},
          {"UntilNoMatch", DxilRecipeRuleApplicationMode::UntilNoMatch},
      };
  if (ParseRecipeValueByTable(text, mode, kRuleApplicationModeEntries))
    return true;

  error = "unsupported rule application mode '" + text + "'";
  return false;
}

static std::vector<std::string> SplitRecipeList(const std::string &text,
                                                char delimiter) {
  std::vector<std::string> parts;
  std::string current;
  for (char ch : text) {
    if (ch == delimiter) {
      const std::string trimmed = TrimRecipeText(current);
      if (!trimmed.empty())
        parts.push_back(trimmed);
      current.clear();
      continue;
    }
    current.push_back(ch);
  }

  const std::string trimmed = TrimRecipeText(current);
  if (!trimmed.empty())
    parts.push_back(trimmed);
  return parts;
}

static bool ParseRecipeResourceKind(const std::string &text,
                                    hlsl::DXIL::ResourceKind &kind,
                                    std::string &error) {
  static const RecipeParseEntry<hlsl::DXIL::ResourceKind> kResourceKindEntries[] = {
      {"Texture1D", hlsl::DXIL::ResourceKind::Texture1D},
      {"Texture2D", hlsl::DXIL::ResourceKind::Texture2D},
      {"Texture2DMS", hlsl::DXIL::ResourceKind::Texture2DMS},
      {"Texture3D", hlsl::DXIL::ResourceKind::Texture3D},
      {"TextureCube", hlsl::DXIL::ResourceKind::TextureCube},
      {"Texture1DArray", hlsl::DXIL::ResourceKind::Texture1DArray},
      {"Texture2DArray", hlsl::DXIL::ResourceKind::Texture2DArray},
      {"Texture2DMSArray", hlsl::DXIL::ResourceKind::Texture2DMSArray},
      {"TextureCubeArray", hlsl::DXIL::ResourceKind::TextureCubeArray},
      {"TypedBuffer", hlsl::DXIL::ResourceKind::TypedBuffer},
      {"RawBuffer", hlsl::DXIL::ResourceKind::RawBuffer},
      {"StructuredBuffer", hlsl::DXIL::ResourceKind::StructuredBuffer},
      {"CBuffer", hlsl::DXIL::ResourceKind::CBuffer},
      {"Sampler", hlsl::DXIL::ResourceKind::Sampler},
      {"TBuffer", hlsl::DXIL::ResourceKind::TBuffer},
      {"RTAccelerationStructure",
       hlsl::DXIL::ResourceKind::RTAccelerationStructure},
      {"FeedbackTexture2D", hlsl::DXIL::ResourceKind::FeedbackTexture2D},
      {"FeedbackTexture2DArray",
       hlsl::DXIL::ResourceKind::FeedbackTexture2DArray},
  };
  if (ParseRecipeValueByTable(text, kind, kResourceKindEntries))
    return true;

  error = "unsupported resource kind '" + text + "'";
  return false;
}

static bool ParseRecipeResourceClass(const std::string &text,
                                     hlsl::DXIL::ResourceClass &resourceClass,
                                     std::string &error) {
  static const RecipeParseEntry<hlsl::DXIL::ResourceClass>
      kResourceClassEntries[] = {
          {"SRV", hlsl::DXIL::ResourceClass::SRV},
          {"UAV", hlsl::DXIL::ResourceClass::UAV},
          {"CBuffer", hlsl::DXIL::ResourceClass::CBuffer},
          {"Sampler", hlsl::DXIL::ResourceClass::Sampler},
      };
  if (ParseRecipeValueByTable(text, resourceClass, kResourceClassEntries))
    return true;

  error = "unsupported resource class '" + text + "'";
  return false;
}

static bool ParseRecipeComponentType(const std::string &text,
                                     hlsl::DXIL::ComponentType &componentType,
                                     std::string &error) {
  static const RecipeParseEntry<hlsl::DXIL::ComponentType>
      kComponentTypeEntries[] = {
          {"F32", hlsl::DXIL::ComponentType::F32},
          {"U32", hlsl::DXIL::ComponentType::U32},
          {"I32", hlsl::DXIL::ComponentType::I32},
      };
  if (ParseRecipeValueByTable(text, componentType, kComponentTypeEntries))
    return true;

  error = "unsupported component type '" + text + "'";
  return false;
}

static bool ParseRecipeCompTypeKind(const std::string &text,
                                    hlsl::CompType::Kind &compType,
                                    std::string &error) {
  static const RecipeParseEntry<hlsl::CompType::Kind> kCompTypeKindEntries[] = {
      {"F32", hlsl::CompType::getF32().GetKind()},
      {"U32", hlsl::CompType::getU32().GetKind()},
      {"I32", hlsl::CompType::getI32().GetKind()},
  };
  if (ParseRecipeValueByTable(text, compType, kCompTypeKindEntries))
    return true;

  error = "unsupported cbuffer field type '" + text + "'";
  return false;
}

static bool ParseRecipeBinaryInstructionOpcode(const std::string &text,
                                              unsigned &instructionOpcode,
                                              std::string &error) {
  static const RecipeParseEntry<unsigned> kBinaryInstructionOpcodeEntries[] = {
      {"Add", llvm::Instruction::Add},
      {"Mul", llvm::Instruction::Mul},
      {"And", llvm::Instruction::And},
      {"URem", llvm::Instruction::URem},
  };
  if (ParseRecipeValueByTable(text,
                              instructionOpcode,
                              kBinaryInstructionOpcodeEntries)) {
    return true;
  }

  error = "unsupported binary instruction opcode '" + text + "'";
  return false;
}

static void SortOperandPatternTree(std::vector<DxilOperandPattern> &operandPatterns) {
  std::sort(operandPatterns.begin(),
            operandPatterns.end(),
            [](const DxilOperandPattern &lhs, const DxilOperandPattern &rhs) {
              return lhs.operandIndex < rhs.operandIndex;
            });

  for (DxilOperandPattern &operandPattern : operandPatterns)
    SortOperandPatternTree(operandPattern.operandPatterns);
}

static bool BuildPendingOperandPatternChildren(
    const std::vector<PendingRewriteRuleBlock::PendingOperandPattern> &flatPatterns,
    llvm::StringRef rootCaptureName,
    llvm::StringRef parentCaptureName,
    std::vector<DxilOperandPattern> &operandPatternsOut,
    std::string &error) {
  operandPatternsOut.clear();

  for (const PendingRewriteRuleBlock::PendingOperandPattern &flatPattern : flatPatterns) {
    const bool isTopLevel = flatPattern.parentCaptureName.empty() ||
                            (!rootCaptureName.empty() &&
                             flatPattern.parentCaptureName == rootCaptureName);
    const bool matchesParent = parentCaptureName.empty()
                                   ? isTopLevel
                                   : flatPattern.parentCaptureName == parentCaptureName;
    if (!matchesParent)
      continue;

    DxilOperandPattern operandPattern = flatPattern.pattern;
    if (!operandPattern.captureName.empty()) {
      if (!BuildPendingOperandPatternChildren(flatPatterns,
                                              rootCaptureName,
                                              operandPattern.captureName,
                                              operandPattern.operandPatterns,
                                              error)) {
        return false;
      }
    }

    operandPatternsOut.push_back(std::move(operandPattern));
  }

  SortOperandPatternTree(operandPatternsOut);
  return true;
}

static bool BuildPendingOperandPatternTree(
    const std::vector<PendingRewriteRuleBlock::PendingOperandPattern> &flatPatterns,
    llvm::StringRef rootCaptureName,
    std::vector<DxilOperandPattern> &operandPatternsOut,
    std::string &error) {
  std::unordered_set<std::string> knownCaptures;
  if (!rootCaptureName.empty())
    knownCaptures.insert(rootCaptureName.str());

  for (const PendingRewriteRuleBlock::PendingOperandPattern &flatPattern : flatPatterns) {
    if (!flatPattern.pattern.captureName.empty())
      knownCaptures.insert(flatPattern.pattern.captureName);
  }

  for (const PendingRewriteRuleBlock::PendingOperandPattern &flatPattern : flatPatterns) {
    if (flatPattern.parentCaptureName.empty())
      continue;
    if (knownCaptures.find(flatPattern.parentCaptureName) == knownCaptures.end()) {
      error = "unknown parent capture '" + flatPattern.parentCaptureName + "'";
      return false;
    }
  }

  return BuildPendingOperandPatternChildren(flatPatterns,
                                            rootCaptureName,
                                            llvm::StringRef(),
                                            operandPatternsOut,
                                            error);
}

static bool ParseRecipeCastInstructionOpcode(const std::string &text,
                                             unsigned &castOpcode,
                                             std::string &error) {
  static const RecipeParseEntry<unsigned> kCastInstructionOpcodeEntries[] = {
      {"UIToFP", llvm::Instruction::UIToFP},
      {"FPToUI", llvm::Instruction::FPToUI},
      {"SIToFP", llvm::Instruction::SIToFP},
      {"FPToSI", llvm::Instruction::FPToSI},
      {"BitCast", llvm::Instruction::BitCast},
  };
  if (ParseRecipeValueByTable(text, castOpcode, kCastInstructionOpcodeEntries))
    return true;

  error = "unsupported cast instruction opcode '" + text + "'";
  return false;
}

static bool ParseRecipeBinding(
    const std::unordered_map<std::string, std::string> &assignments,
    ResourceBindingDesc &binding,
    std::string &error) {
  const std::string *bindText = FindRecipeAssignment(assignments, "bind");
  const std::string *spaceText = FindRecipeAssignment(assignments, "space");
  unsigned space = 0;
  if (spaceText != nullptr &&
      !ParseRecipeUnsignedValue(*spaceText, space, error)) {
    return false;
  }

  if (bindText == nullptr || *bindText == "auto") {
    binding.Auto(space);
    return true;
  }

  unsigned bindPoint = 0;
  if (!ParseRecipeUnsignedValue(*bindText, bindPoint, error))
    return false;

  binding.Register(bindPoint, space);
  return true;
}

static bool FailRecipeParse(DxilRecipeParseResult &result,
                            const std::string &sourceName,
                            unsigned lineNumber,
                            const std::string &message) {
  result.error = sourceName + ":" + std::to_string(lineNumber) + ": " + message;
  return false;
}

struct YamlRecipeMapping {
  std::unordered_map<std::string, llvm::yaml::Node *> values;
};

static unsigned GetYamlRecipeLine(llvm::StringRef recipeText,
                                  const llvm::yaml::Node *node) {
  if (node == nullptr)
    return 0;

  const char *pointer = node->getSourceRange().Start.getPointer();
  if (pointer == nullptr || pointer < recipeText.begin() ||
      pointer > recipeText.end()) {
    return 0;
  }

  unsigned lineNumber = 1;
  for (const char *current = recipeText.begin(); current < pointer; ++current) {
    if (*current == '\n')
      ++lineNumber;
  }

  return lineNumber;
}

static bool FailYamlRecipeParse(DxilRecipeParseResult &result,
                                llvm::StringRef sourceName,
                                llvm::StringRef recipeText,
                                const llvm::yaml::Node *node,
                                const std::string &message) {
  const unsigned lineNumber = GetYamlRecipeLine(recipeText, node);
  result.error = sourceName.str();
  if (lineNumber != 0)
    result.error += ":" + std::to_string(lineNumber);
  result.error += ": " + message;
  return false;
}

static bool GetYamlRecipeScalarString(llvm::yaml::Node *node,
                                      std::string &value) {
  if (node == nullptr)
    return false;

  if (llvm::yaml::ScalarNode *scalarNode =
          llvm::dyn_cast<llvm::yaml::ScalarNode>(node)) {
    llvm::SmallVector<char, 64> storage;
    value = scalarNode->getValue(storage).str();
    return true;
  }

  if (llvm::yaml::BlockScalarNode *blockScalarNode =
          llvm::dyn_cast<llvm::yaml::BlockScalarNode>(node)) {
    value = blockScalarNode->getValue().str();
    return true;
  }

  return false;
}

static bool ParseYamlRecipeBool(llvm::yaml::Node *node,
                                bool &value,
                                std::string &error) {
  std::string text;
  if (!GetYamlRecipeScalarString(node, text)) {
    error = "expected boolean scalar";
    return false;
  }

  return ParseRecipeBoolValue(text, value, error);
}

static bool ParseYamlRecipeUnsigned(llvm::yaml::Node *node,
                                    unsigned &value,
                                    std::string &error) {
  std::string text;
  if (!GetYamlRecipeScalarString(node, text)) {
    error = "expected unsigned integer scalar";
    return false;
  }

  return ParseRecipeUnsignedValue(text, value, error);
}

static bool CollectYamlRecipeMapping(llvm::yaml::Node *node,
                                     YamlRecipeMapping &mapping,
                                     std::string &error) {
  llvm::yaml::MappingNode *mappingNode =
      llvm::dyn_cast<llvm::yaml::MappingNode>(node);
  if (mappingNode == nullptr) {
    error = "expected mapping";
    return false;
  }

  mapping.values.clear();
  for (llvm::yaml::KeyValueNode &entry : *mappingNode) {
    std::string key;
    if (!GetYamlRecipeScalarString(entry.getKey(), key)) {
      error = "expected scalar mapping key";
      return false;
    }

    if (!mapping.values.emplace(key, entry.getValue()).second) {
      error = "duplicate key '" + key + "'";
      return false;
    }
  }

  return true;
}

static bool CollectYamlRecipeSequence(llvm::yaml::Node *node,
                                      std::vector<llvm::yaml::Node *> &entries,
                                      std::string &error) {
  llvm::yaml::SequenceNode *sequenceNode =
      llvm::dyn_cast<llvm::yaml::SequenceNode>(node);
  if (sequenceNode == nullptr) {
    error = "expected sequence";
    return false;
  }

  entries.clear();
  for (llvm::yaml::Node &entry : *sequenceNode)
    entries.push_back(&entry);
  return true;
}

static bool ValidateYamlRecipeFields(
    const YamlRecipeMapping &mapping,
    std::initializer_list<const char *> allowedFields,
    std::string &error) {
  for (const auto &entry : mapping.values) {
    bool allowed = false;
    for (const char *fieldName : allowedFields) {
      if (entry.first == fieldName) {
        allowed = true;
        break;
      }
    }

    if (!allowed) {
      error = "unknown field '" + entry.first + "'";
      return false;
    }
  }

  return true;
}

static llvm::yaml::Node *FindYamlRecipeField(const YamlRecipeMapping &mapping,
                                             const char *fieldName) {
  auto it = mapping.values.find(fieldName);
  return it != mapping.values.end() ? it->second : nullptr;
}

static bool ParseYamlRecipeBindingNode(llvm::yaml::Node *node,
                                       ResourceBindingDesc &binding,
                                       std::string &error) {
  if (node == nullptr)
    return true;

  YamlRecipeMapping mapping;
  if (!CollectYamlRecipeMapping(node, mapping, error) ||
      !ValidateYamlRecipeFields(mapping, {"bind", "space"}, error)) {
    return false;
  }

  unsigned space = 0;
  if (llvm::yaml::Node *spaceNode = FindYamlRecipeField(mapping, "space")) {
    if (!ParseYamlRecipeUnsigned(spaceNode, space, error))
      return false;
  }

  llvm::yaml::Node *bindNode = FindYamlRecipeField(mapping, "bind");
  if (bindNode == nullptr) {
    binding.Auto(space);
    return true;
  }

  std::string bindText;
  if (!GetYamlRecipeScalarString(bindNode, bindText)) {
    error = "binding bind must be a scalar";
    return false;
  }

  if (bindText == "auto") {
    binding.Auto(space);
    return true;
  }

  unsigned bindPoint = 0;
  if (!ParseRecipeUnsignedValue(bindText, bindPoint, error))
    return false;

  binding.Register(bindPoint, space);
  return true;
}

static bool ParseYamlOperandPatternNode(llvm::yaml::Node *node,
                                        DxilOperandPattern &operandPattern,
                                        std::string &error);

static bool ParseYamlOperandPatternList(llvm::yaml::Node *node,
                                        std::vector<DxilOperandPattern> &operands,
                                        std::string &error) {
  operands.clear();
  if (node == nullptr)
    return true;

  std::vector<llvm::yaml::Node *> entries;
  if (!CollectYamlRecipeSequence(node, entries, error))
    return false;

  for (llvm::yaml::Node *entry : entries) {
    DxilOperandPattern operandPattern;
    if (!ParseYamlOperandPatternNode(entry, operandPattern, error))
      return false;
    operands.push_back(std::move(operandPattern));
  }

  SortOperandPatternTree(operands);
  return true;
}

static bool ParseYamlOperandPatternNode(llvm::yaml::Node *node,
                                        DxilOperandPattern &operandPattern,
                                        std::string &error) {
  YamlRecipeMapping mapping;
  if (!CollectYamlRecipeMapping(node, mapping, error) ||
      !ValidateYamlRecipeFields(mapping,
                                {"index", "kind", "capture", "value", "opcode",
                                 "operands"},
                                error)) {
    return false;
  }

  llvm::yaml::Node *indexNode = FindYamlRecipeField(mapping, "index");
  llvm::yaml::Node *kindNode = FindYamlRecipeField(mapping, "kind");
  if (indexNode == nullptr || kindNode == nullptr) {
    error = "operand requires index and kind";
    return false;
  }

  operandPattern = DxilOperandPattern();
  if (!ParseYamlRecipeUnsigned(indexNode, operandPattern.operandIndex, error))
    return false;

  std::string kindText;
  if (!GetYamlRecipeScalarString(kindNode, kindText)) {
    error = "operand kind must be a scalar";
    return false;
  }

  const std::string loweredKind = LowercaseRecipeToken(kindText);
  if (loweredKind == "any") {
    operandPattern.kind = DxilOperandPatternKind::Any;
  } else if (loweredKind == "constant_int") {
    llvm::yaml::Node *valueNode = FindYamlRecipeField(mapping, "value");
    if (valueNode == nullptr) {
      error = "constant_int operands require value";
      return false;
    }

    unsigned value = 0;
    if (!ParseYamlRecipeUnsigned(valueNode, value, error))
      return false;

    operandPattern.kind = DxilOperandPatternKind::ConstantInt;
    operandPattern.constantIntValue = value;
  } else if (loweredKind == "instruction") {
    llvm::yaml::Node *opcodeNode = FindYamlRecipeField(mapping, "opcode");
    if (opcodeNode == nullptr) {
      error = "instruction operands require opcode";
      return false;
    }

    std::string opcodeText;
    if (!GetYamlRecipeScalarString(opcodeNode, opcodeText) ||
        !ParseRecipeInstructionOpcode(opcodeText,
                                      operandPattern.instructionOpcode,
                                      error)) {
      if (error.empty())
        error = "instruction opcode must be a scalar";
      return false;
    }

    operandPattern.kind = DxilOperandPatternKind::Instruction;
  } else if (loweredKind == "dxop") {
    llvm::yaml::Node *opcodeNode = FindYamlRecipeField(mapping, "opcode");
    if (opcodeNode == nullptr) {
      error = "dxop operands require opcode";
      return false;
    }

    std::string opcodeText;
    if (!GetYamlRecipeScalarString(opcodeNode, opcodeText) ||
        !ParseRecipeOpCode(opcodeText, operandPattern.dxilOpCode, error)) {
      if (error.empty())
        error = "dxop opcode must be a scalar";
      return false;
    }

    operandPattern.kind = DxilOperandPatternKind::DxOpCall;
    operandPattern.matchDxilOpCode = true;
  } else {
    error = "unsupported operand kind '" + kindText + "'";
    return false;
  }

  if (llvm::yaml::Node *captureNode = FindYamlRecipeField(mapping, "capture")) {
    if (!GetYamlRecipeScalarString(captureNode, operandPattern.captureName)) {
      error = "operand capture must be a scalar";
      return false;
    }
  }

  if (!ParseYamlOperandPatternList(FindYamlRecipeField(mapping, "operands"),
                                   operandPattern.operandPatterns,
                                   error)) {
    return false;
  }

  return true;
}

static bool ParseYamlBindingPatternNode(llvm::yaml::Node *node,
                                        DxilCallPattern &pattern,
                                        std::string &error) {
  YamlRecipeMapping mapping;
  if (!CollectYamlRecipeMapping(node, mapping, error) ||
      !ValidateYamlRecipeFields(mapping,
                                {"kind", "capture", "opcode", "operands"},
                                error)) {
    return false;
  }

  std::string kindText;
  if (!GetYamlRecipeScalarString(FindYamlRecipeField(mapping, "kind"), kindText)) {
    error = "binding kind must be a scalar";
    return false;
  }
  if (LowercaseRecipeToken(kindText) != "dxop") {
    error = "unsupported binding kind '" + kindText + "'";
    return false;
  }

  llvm::yaml::Node *captureNode = FindYamlRecipeField(mapping, "capture");
  llvm::yaml::Node *opcodeNode = FindYamlRecipeField(mapping, "opcode");
  if (captureNode == nullptr || opcodeNode == nullptr) {
    error = "bindings require capture and opcode";
    return false;
  }

  std::string captureName;
  std::string opcodeText;
  if (!GetYamlRecipeScalarString(captureNode, captureName)) {
    error = "binding capture must be a scalar";
    return false;
  }
  hlsl::OP::OpCode opcode = static_cast<hlsl::OP::OpCode>(0);
  if (!GetYamlRecipeScalarString(opcodeNode, opcodeText) ||
      !ParseRecipeOpCode(opcodeText, opcode, error)) {
    if (error.empty())
      error = "binding opcode must be a scalar";
    return false;
  }

  pattern = DxOpCall(opcode).Capture(captureName).Build();
  pattern.operandPatterns.push_back(
      ConstantIntOperand(0, static_cast<uint64_t>(opcode)).Build());
  if (!ParseYamlOperandPatternList(FindYamlRecipeField(mapping, "operands"),
                                   pattern.operandPatterns,
                                   error)) {
    return false;
  }

  if (pattern.operandPatterns.empty() ||
      pattern.operandPatterns.front().operandIndex != 0) {
    pattern.operandPatterns.insert(
        pattern.operandPatterns.begin(),
        ConstantIntOperand(0, static_cast<uint64_t>(opcode)).Build());
  }
  SortOperandPatternTree(pattern.operandPatterns);
  return true;
}

static bool ParseYamlEmitOperandNode(
    llvm::yaml::Node *node,
    const std::unordered_map<std::string, TextureResourceDesc> &parsedTextures,
    const std::unordered_map<std::string, TextureResourceDesc> &parsedUavs,
    const std::unordered_map<std::string, CBufferDesc> &parsedCBuffers,
    const std::unordered_map<std::string, SamplerDesc> &parsedSamplers,
    DxilRewriteEmitOperand &operand,
    std::string &error) {
  YamlRecipeMapping mapping;
  if (!CollectYamlRecipeMapping(node, mapping, error) ||
      !ValidateYamlRecipeFields(mapping,
                                {"index", "kind", "capture", "id", "value"},
                                error)) {
    return false;
  }

  llvm::yaml::Node *indexNode = FindYamlRecipeField(mapping, "index");
  llvm::yaml::Node *kindNode = FindYamlRecipeField(mapping, "kind");
  if (indexNode == nullptr || kindNode == nullptr) {
    error = "emit operand requires index and kind";
    return false;
  }

  operand = DxilRewriteEmitOperand();
  if (!ParseYamlRecipeUnsigned(indexNode, operand.operandIndex, error))
    return false;

  std::string kindText;
  if (!GetYamlRecipeScalarString(kindNode, kindText)) {
    error = "emit operand kind must be a scalar";
    return false;
  }

  const std::string loweredKind = LowercaseRecipeToken(kindText);
  if (loweredKind == "capture") {
    llvm::yaml::Node *captureNode = FindYamlRecipeField(mapping, "capture");
    if (captureNode == nullptr ||
        !GetYamlRecipeScalarString(captureNode, operand.captureName)) {
      error = "capture emit operands require capture";
      return false;
    }
    operand.kind = DxilRewriteEmitOperandKind::Capture;
  } else if (loweredKind == "temporary") {
    llvm::yaml::Node *idNode = FindYamlRecipeField(mapping, "id");
    if (idNode == nullptr ||
        !GetYamlRecipeScalarString(idNode, operand.temporaryName)) {
      error = "temporary emit operands require id";
      return false;
    }
    operand.kind = DxilRewriteEmitOperandKind::Temporary;
  } else if (loweredKind == "constant_int") {
    llvm::yaml::Node *valueNode = FindYamlRecipeField(mapping, "value");
    unsigned value = 0;
    if (valueNode == nullptr || !ParseYamlRecipeUnsigned(valueNode, value, error)) {
      if (error.empty())
        error = "constant_int emit operands require value";
      return false;
    }
    operand.kind = DxilRewriteEmitOperandKind::ConstantInt;
    operand.constantIntValue = value;
  } else if (loweredKind == "resource") {
    llvm::yaml::Node *idNode = FindYamlRecipeField(mapping, "id");
    std::string resourceId;
    if (idNode == nullptr || !GetYamlRecipeScalarString(idNode, resourceId)) {
      error = "resource emit operands require id";
      return false;
    }

    ParsedRecipeResourceRef resourceRef;
    TryResolveParsedRecipeResourceRef(resourceId,
                                     parsedTextures,
                                     parsedUavs,
                                     parsedCBuffers,
                                     parsedSamplers,
                                     resourceRef);
    if (!resourceRef.found) {
      error = "unknown resource id '" + resourceId + "'";
      return false;
    }

    operand.kind = DxilRewriteEmitOperandKind::ResourceHandle;
    operand.resourceName = resourceRef.resourceName;
    operand.resourceBinding = resourceRef.binding;
  } else if (loweredKind == "undef") {
    operand.kind = DxilRewriteEmitOperandKind::Undef;
  } else {
    error = "unsupported emit operand kind '" + kindText + "'";
    return false;
  }

  return true;
}

static bool ParseYamlEmitOperandList(
    llvm::yaml::Node *node,
    const std::unordered_map<std::string, TextureResourceDesc> &parsedTextures,
    const std::unordered_map<std::string, TextureResourceDesc> &parsedUavs,
    const std::unordered_map<std::string, CBufferDesc> &parsedCBuffers,
    const std::unordered_map<std::string, SamplerDesc> &parsedSamplers,
    std::vector<DxilRewriteEmitOperand> &operands,
    std::string &error) {
  operands.clear();
  if (node == nullptr)
    return true;

  std::vector<llvm::yaml::Node *> entries;
  if (!CollectYamlRecipeSequence(node, entries, error))
    return false;

  for (llvm::yaml::Node *entry : entries) {
    DxilRewriteEmitOperand operand;
    if (!ParseYamlEmitOperandNode(entry,
                                  parsedTextures,
                                  parsedUavs,
                                  parsedCBuffers,
                                  parsedSamplers,
                                  operand,
                                  error)) {
      return false;
    }
    operands.push_back(std::move(operand));
  }

  return true;
}

static bool ParseYamlRewriteRuleNode(
    llvm::yaml::Node *node,
    const std::unordered_map<std::string, TextureResourceDesc> &parsedTextures,
    const std::unordered_map<std::string, TextureResourceDesc> &parsedUavs,
    const std::unordered_map<std::string, CBufferDesc> &parsedCBuffers,
    const std::unordered_map<std::string, SamplerDesc> &parsedSamplers,
    DxilRewriteRule &rule,
    std::string &ruleId,
    std::string &error) {
  YamlRecipeMapping mapping;
  if (!CollectYamlRecipeMapping(node, mapping, error) ||
      !ValidateYamlRecipeFields(mapping,
                                {"id", "name", "match", "bindings", "emit",
                                 "replace_with", "replace_with_capture"},
                                error)) {
    return false;
  }

  llvm::yaml::Node *idNode = FindYamlRecipeField(mapping, "id");
  llvm::yaml::Node *matchNode = FindYamlRecipeField(mapping, "match");
  if (idNode == nullptr || matchNode == nullptr ||
      !GetYamlRecipeScalarString(idNode, ruleId)) {
    error = "rewrite_rule requires id and match";
    return false;
  }

  rule = DxilRewriteRule();
  rule.name = ruleId;
  if (llvm::yaml::Node *nameNode = FindYamlRecipeField(mapping, "name")) {
    if (!GetYamlRecipeScalarString(nameNode, rule.name)) {
      error = "rewrite rule name must be a scalar";
      return false;
    }
  }

  YamlRecipeMapping matchMapping;
  if (!CollectYamlRecipeMapping(matchNode, matchMapping, error) ||
      !ValidateYamlRecipeFields(matchMapping,
                                {"opcode", "capture", "replace", "mode",
                                 "prune_dead", "prune_captures", "operands"},
                                error)) {
    return false;
  }

  llvm::yaml::Node *opcodeNode = FindYamlRecipeField(matchMapping, "opcode");
  llvm::yaml::Node *replaceNode = FindYamlRecipeField(matchMapping, "replace");
  if (opcodeNode == nullptr || replaceNode == nullptr) {
    error = "rewrite rule match requires opcode and replace";
    return false;
  }

  std::string opcodeText;
  hlsl::OP::OpCode opcode = static_cast<hlsl::OP::OpCode>(0);
  if (!GetYamlRecipeScalarString(opcodeNode, opcodeText) ||
      !ParseRecipeOpCode(opcodeText, opcode, error)) {
    if (error.empty())
      error = "rewrite rule opcode must be a scalar";
    return false;
  }

  if (!GetYamlRecipeScalarString(replaceNode, rule.replaceCaptureName)) {
    error = "rewrite rule replace must be a scalar";
    return false;
  }

  std::string rootCaptureName = rule.replaceCaptureName;
  if (llvm::yaml::Node *captureNode = FindYamlRecipeField(matchMapping, "capture")) {
    if (!GetYamlRecipeScalarString(captureNode, rootCaptureName)) {
      error = "rewrite rule capture must be a scalar";
      return false;
    }
  }

  if (llvm::yaml::Node *modeNode = FindYamlRecipeField(matchMapping, "mode")) {
    std::string modeText;
    if (!GetYamlRecipeScalarString(modeNode, modeText) ||
        !ParseRecipeRewriteMode(modeText, rule.mode, error)) {
      if (error.empty())
        error = "rewrite rule mode must be a scalar";
      return false;
    }
  }

  if (llvm::yaml::Node *pruneDeadNode = FindYamlRecipeField(matchMapping, "prune_dead")) {
    if (!ParseYamlRecipeBool(pruneDeadNode, rule.pruneDeadInstructions, error))
      return false;
  }

  if (llvm::yaml::Node *pruneCapturesNode = FindYamlRecipeField(matchMapping, "prune_captures")) {
    std::vector<llvm::yaml::Node *> entries;
    if (!CollectYamlRecipeSequence(pruneCapturesNode, entries, error))
      return false;

    for (llvm::yaml::Node *entry : entries) {
      std::string captureName;
      if (!GetYamlRecipeScalarString(entry, captureName)) {
        error = "prune_captures entries must be scalars";
        return false;
      }
      rule.pruneCaptureNames.push_back(std::move(captureName));
    }
  }

  std::vector<DxilOperandPattern> rootOperands;
  rootOperands.push_back(ConstantIntOperand(0, static_cast<uint64_t>(opcode)).Build());
  std::vector<DxilOperandPattern> nestedOperands;
  if (!ParseYamlOperandPatternList(FindYamlRecipeField(matchMapping, "operands"),
                                   nestedOperands,
                                   error)) {
    return false;
  }
  rootOperands.insert(rootOperands.end(), nestedOperands.begin(), nestedOperands.end());
  SortOperandPatternTree(rootOperands);
  rule.pattern = DxOpCall(opcode).Capture(rootCaptureName).Args(std::move(rootOperands)).Build();

  if (llvm::yaml::Node *bindingsNode = FindYamlRecipeField(mapping, "bindings")) {
    std::vector<llvm::yaml::Node *> entries;
    if (!CollectYamlRecipeSequence(bindingsNode, entries, error))
      return false;

    for (llvm::yaml::Node *entry : entries) {
      DxilCallPattern bindingPattern;
      if (!ParseYamlBindingPatternNode(entry, bindingPattern, error))
        return false;
      rule.bindingPatterns.push_back(std::move(bindingPattern));
    }
  }

  if (llvm::yaml::Node *captureNode = FindYamlRecipeField(mapping, "replace_with_capture")) {
    if (!GetYamlRecipeScalarString(captureNode, rule.replacementCaptureName)) {
      error = "replace_with_capture must be a scalar";
      return false;
    }
  }

  if (llvm::yaml::Node *emitNode = FindYamlRecipeField(mapping, "emit")) {
    std::vector<llvm::yaml::Node *> emitEntries;
    if (!CollectYamlRecipeSequence(emitNode, emitEntries, error))
      return false;

    for (llvm::yaml::Node *emitEntry : emitEntries) {
      YamlRecipeMapping emitMapping;
      if (!CollectYamlRecipeMapping(emitEntry, emitMapping, error) ||
          !ValidateYamlRecipeFields(emitMapping,
                                    {"kind", "id", "resource", "handle",
                                     "opcode", "type", "aggregate", "index",
                                     "operands"},
                                    error)) {
        return false;
      }

      llvm::yaml::Node *kindNode = FindYamlRecipeField(emitMapping, "kind");
      std::string kindText;
      if (kindNode == nullptr || !GetYamlRecipeScalarString(kindNode, kindText)) {
        error = "emit entries require kind";
        return false;
      }

      const std::string loweredKind = LowercaseRecipeToken(kindText);
      if (loweredKind == "create_handle") {
        std::string id;
        std::string resourceId;
        if (!GetYamlRecipeScalarString(FindYamlRecipeField(emitMapping, "id"), id) ||
            !GetYamlRecipeScalarString(FindYamlRecipeField(emitMapping, "resource"),
                                       resourceId)) {
          error = "create_handle emit entries require id and resource";
          return false;
        }

        ParsedRecipeResourceRef resourceRef;
        TryResolveParsedRecipeResourceRef(resourceId,
                                         parsedTextures,
                                         parsedUavs,
                                         parsedCBuffers,
                                         parsedSamplers,
                                         resourceRef);
        if (!resourceRef.found) {
          error = "unknown resource id '" + resourceId + "'";
          return false;
        }

        rule.emittedSequence.values.push_back(
            EmitCreateHandleValue(id, resourceRef.resourceName, resourceRef.binding));
      } else if (loweredKind == "annotate_handle") {
        std::string id;
        std::string handle;
        std::string resourceId;
        if (!GetYamlRecipeScalarString(FindYamlRecipeField(emitMapping, "id"), id) ||
            !GetYamlRecipeScalarString(FindYamlRecipeField(emitMapping, "handle"),
                                       handle) ||
            !GetYamlRecipeScalarString(FindYamlRecipeField(emitMapping, "resource"),
                                       resourceId)) {
          error = "annotate_handle emit entries require id, handle, and resource";
          return false;
        }

        ParsedRecipeResourceRef resourceRef;
        TryResolveParsedRecipeResourceRef(resourceId,
                                         parsedTextures,
                                         parsedUavs,
                                         parsedCBuffers,
                                         parsedSamplers,
                                         resourceRef);
        if (!resourceRef.found) {
          error = "unknown resource id '" + resourceId + "'";
          return false;
        }

        rule.emittedSequence.values.push_back(EmitAnnotateHandleValue(
            id, handle, resourceRef.resourceName, resourceRef.binding));
      } else if (loweredKind == "call") {
        std::string id;
        std::string opcodeTextValue;
        if (!GetYamlRecipeScalarString(FindYamlRecipeField(emitMapping, "id"), id) ||
            !GetYamlRecipeScalarString(FindYamlRecipeField(emitMapping, "opcode"),
                                       opcodeTextValue)) {
          error = "call emit entries require id and opcode";
          return false;
        }

        DxilRewriteEmitValue emittedValue;
        emittedValue.name = id;
        emittedValue.kind = DxilRewriteEmitValueKind::DxOpCall;
        if (!ParseRecipeOpCode(opcodeTextValue, emittedValue.dxilOpCode, error))
          return false;

        if (llvm::yaml::Node *typeNode = FindYamlRecipeField(emitMapping, "type")) {
          std::string typeText;
          if (!GetYamlRecipeScalarString(typeNode, typeText) ||
              !ParseRecipeComponentType(typeText,
                                        emittedValue.resultComponentType,
                                        error)) {
            if (error.empty())
              error = "call emit type must be a scalar";
            return false;
          }
          emittedValue.hasExplicitResultComponentType = true;
        }

        if (!ParseYamlEmitOperandList(FindYamlRecipeField(emitMapping, "operands"),
                                      parsedTextures,
                                      parsedUavs,
                                      parsedCBuffers,
                                      parsedSamplers,
                                      emittedValue.operands,
                                      error)) {
          return false;
        }

        rule.emittedSequence.values.push_back(std::move(emittedValue));
      } else if (loweredKind == "extract") {
        std::string id;
        std::string aggregate;
        unsigned extractIndex = 0;
        if (!GetYamlRecipeScalarString(FindYamlRecipeField(emitMapping, "id"), id) ||
            !GetYamlRecipeScalarString(FindYamlRecipeField(emitMapping, "aggregate"),
                                       aggregate) ||
            !ParseYamlRecipeUnsigned(FindYamlRecipeField(emitMapping, "index"),
                                     extractIndex,
                                     error)) {
          if (error.empty())
            error = "extract emit entries require id, aggregate, and index";
          return false;
        }

        rule.emittedSequence.values.push_back(
            EmitExtractValue(id, aggregate, extractIndex));
      } else if (loweredKind == "binop") {
        std::string id;
        std::string opcodeTextValue;
        std::string typeText;
        unsigned instructionOpcode = 0;
        hlsl::DXIL::ComponentType componentType = hlsl::DXIL::ComponentType::Invalid;
        if (!GetYamlRecipeScalarString(FindYamlRecipeField(emitMapping, "id"), id) ||
            !GetYamlRecipeScalarString(FindYamlRecipeField(emitMapping, "opcode"),
                                       opcodeTextValue) ||
            !GetYamlRecipeScalarString(FindYamlRecipeField(emitMapping, "type"),
                                       typeText) ||
            !ParseRecipeBinaryInstructionOpcode(opcodeTextValue,
                                                instructionOpcode,
                                                error) ||
            !ParseRecipeComponentType(typeText, componentType, error)) {
          if (error.empty())
            error = "binop emit entries require id, opcode, and type";
          return false;
        }

        DxilRewriteEmitValue emittedValue =
            EmitBinaryInstructionValue(id, instructionOpcode, componentType, {});
        if (!ParseYamlEmitOperandList(FindYamlRecipeField(emitMapping, "operands"),
                                      parsedTextures,
                                      parsedUavs,
                                      parsedCBuffers,
                                      parsedSamplers,
                                      emittedValue.operands,
                                      error)) {
          return false;
        }
        rule.emittedSequence.values.push_back(std::move(emittedValue));
      } else if (loweredKind == "cast") {
        std::string id;
        std::string opcodeTextValue;
        std::string typeText;
        unsigned castOpcode = 0;
        hlsl::DXIL::ComponentType componentType = hlsl::DXIL::ComponentType::Invalid;
        if (!GetYamlRecipeScalarString(FindYamlRecipeField(emitMapping, "id"), id) ||
            !GetYamlRecipeScalarString(FindYamlRecipeField(emitMapping, "opcode"),
                                       opcodeTextValue) ||
            !GetYamlRecipeScalarString(FindYamlRecipeField(emitMapping, "type"),
                                       typeText) ||
            !ParseRecipeCastInstructionOpcode(opcodeTextValue, castOpcode, error) ||
            !ParseRecipeComponentType(typeText, componentType, error)) {
          if (error.empty())
            error = "cast emit entries require id, opcode, and type";
          return false;
        }

        DxilRewriteEmitValue emittedValue =
            EmitCastInstructionValue(id, castOpcode, componentType, {});
        if (!ParseYamlEmitOperandList(FindYamlRecipeField(emitMapping, "operands"),
                                      parsedTextures,
                                      parsedUavs,
                                      parsedCBuffers,
                                      parsedSamplers,
                                      emittedValue.operands,
                                      error)) {
          return false;
        }
        rule.emittedSequence.values.push_back(std::move(emittedValue));
      } else {
        error = "unsupported emit kind '" + kindText + "'";
        return false;
      }
    }
  }

  if (llvm::yaml::Node *replaceWithNode = FindYamlRecipeField(mapping, "replace_with")) {
    if (!GetYamlRecipeScalarString(replaceWithNode,
                                   rule.emittedSequence.replacementValueName)) {
      error = "replace_with must be a scalar";
      return false;
    }
  }

  const bool hasCaptureReplacement = !rule.replacementCaptureName.empty();
  const bool hasEmittedReplacement = !rule.emittedSequence.replacementValueName.empty();
  if (hasCaptureReplacement == hasEmittedReplacement) {
    error = "rewrite rules must provide exactly one of replace_with_capture or replace_with";
    return false;
  }

  return true;
}

struct YamlRecipeBindingModel {
  std::string bind = "auto";
  unsigned space = 0;
};

struct YamlRecipeTextureModel {
  std::string id;
  std::string name;
  std::string kind;
  std::string element;
  unsigned width = 0;
  YamlRecipeBindingModel binding;
};

struct YamlRecipeFieldModel {
  std::string name;
  std::string type;
  unsigned width = 0;
  unsigned offset = 0;
};

struct YamlRecipeCBufferModel {
  std::string id;
  std::string name;
  std::string type;
  unsigned size = 0;
  YamlRecipeBindingModel binding;
  std::vector<YamlRecipeFieldModel> fields;
};

struct YamlRecipeSamplerModel {
  std::string id;
  std::string name;
  YamlRecipeBindingModel binding;
};

struct YamlRecipeResourcesModel {
  std::vector<YamlRecipeTextureModel> textures;
  std::vector<YamlRecipeTextureModel> texture_uavs;
  std::vector<YamlRecipeCBufferModel> cbuffers;
  std::vector<YamlRecipeSamplerModel> samplers;
};

struct YamlRecipeOperandModel {
  unsigned index = 0;
  std::string kind;
  std::string capture;
  unsigned value = 0;
  std::string opcode;
  std::string resource_class;
  std::string resource_kind;
  std::string resource_name;
  std::string resource_name_like;
  int bind = -1;
  int space = -1;
  std::vector<YamlRecipeOperandModel> operands;
};

struct YamlRecipeBindingPatternModel {
  std::string kind;
  std::string capture;
  std::string opcode;
  std::vector<YamlRecipeOperandModel> operands;
};

struct YamlRecipeEmitOperandModel {
  unsigned index = 0;
  std::string kind;
  std::string capture;
  std::string id;
  unsigned value = 0;
};

struct YamlRecipeEmitModel {
  std::string kind;
  std::string id;
  std::string resource;
  std::string handle;
  std::string opcode;
  std::string type;
  std::string aggregate;
  unsigned index = 0;
  std::vector<YamlRecipeEmitOperandModel> operands;
};

struct YamlRecipeMatchModel {
  std::string opcode;
  std::string capture;
  std::string replace;
  std::string mode = "Replace";
  bool prune_dead = true;
  std::vector<std::string> prune_captures;
  std::vector<YamlRecipeOperandModel> operands;
};

struct YamlRecipeRuleModel {
  std::string id;
  std::string name;
  YamlRecipeMatchModel match;
  std::vector<YamlRecipeBindingPatternModel> bindings;
  std::vector<YamlRecipeEmitModel> emit;
  std::string replace_with;
  std::string replace_with_capture;
};

struct YamlRecipeStepModel {
  std::string kind;
  std::string id;
  std::string rule;
  std::string name;
  std::string mode = "Once";
  bool required = true;
};

struct YamlRecipeOptionsModel {
  bool restore_reflection = true;
  bool refresh_resources = false;
  bool verify_module = true;
};

struct YamlRecipeDocumentModel {
  unsigned version = 1;
  YamlRecipeOptionsModel options;
  YamlRecipeResourcesModel resources;
  std::vector<YamlRecipeRuleModel> rewrite_rules;
  std::vector<YamlRecipeStepModel> steps;
};

LLVM_YAML_IS_SEQUENCE_VECTOR(YamlRecipeFieldModel)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlRecipeTextureModel)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlRecipeCBufferModel)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlRecipeSamplerModel)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlRecipeOperandModel)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlRecipeBindingPatternModel)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlRecipeEmitOperandModel)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlRecipeEmitModel)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlRecipeRuleModel)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlRecipeStepModel)
LLVM_YAML_IS_SEQUENCE_VECTOR(std::string)

namespace llvm {
namespace yaml {

template <> struct MappingTraits<YamlRecipeBindingModel> {
  static void mapping(IO &io, YamlRecipeBindingModel &binding) {
    io.mapOptional("bind", binding.bind, std::string("auto"));
    io.mapOptional("space", binding.space, 0u);
  }
};

template <> struct MappingTraits<YamlRecipeTextureModel> {
  static void mapping(IO &io, YamlRecipeTextureModel &texture) {
    io.mapRequired("id", texture.id);
    io.mapRequired("name", texture.name);
    io.mapRequired("kind", texture.kind);
    io.mapRequired("element", texture.element);
    io.mapRequired("width", texture.width);
    io.mapOptional("binding", texture.binding);
  }
};

template <> struct MappingTraits<YamlRecipeFieldModel> {
  static void mapping(IO &io, YamlRecipeFieldModel &field) {
    io.mapRequired("name", field.name);
    io.mapRequired("type", field.type);
    io.mapRequired("width", field.width);
    io.mapRequired("offset", field.offset);
  }
};

template <> struct MappingTraits<YamlRecipeCBufferModel> {
  static void mapping(IO &io, YamlRecipeCBufferModel &cbuffer) {
    io.mapRequired("id", cbuffer.id);
    io.mapRequired("name", cbuffer.name);
    io.mapRequired("type", cbuffer.type);
    io.mapRequired("size", cbuffer.size);
    io.mapOptional("binding", cbuffer.binding);
    io.mapRequired("fields", cbuffer.fields);
  }
};

template <> struct MappingTraits<YamlRecipeSamplerModel> {
  static void mapping(IO &io, YamlRecipeSamplerModel &sampler) {
    io.mapRequired("id", sampler.id);
    io.mapRequired("name", sampler.name);
    io.mapOptional("binding", sampler.binding);
  }
};

template <> struct MappingTraits<YamlRecipeResourcesModel> {
  static void mapping(IO &io, YamlRecipeResourcesModel &resources) {
    io.mapOptional("textures", resources.textures);
    io.mapOptional("texture_uavs", resources.texture_uavs);
    io.mapOptional("cbuffers", resources.cbuffers);
    io.mapOptional("samplers", resources.samplers);
  }
};

template <> struct MappingTraits<YamlRecipeOperandModel> {
  static void mapping(IO &io, YamlRecipeOperandModel &operand) {
    io.mapRequired("index", operand.index);
    io.mapRequired("kind", operand.kind);
    io.mapOptional("capture", operand.capture);
    io.mapOptional("value", operand.value, 0u);
    io.mapOptional("opcode", operand.opcode);
    io.mapOptional("resource_class", operand.resource_class);
    io.mapOptional("resource_kind", operand.resource_kind);
    io.mapOptional("resource_name", operand.resource_name);
    io.mapOptional("resource_name_like", operand.resource_name_like);
    io.mapOptional("bind", operand.bind, -1);
    io.mapOptional("space", operand.space, -1);
    io.mapOptional("operands", operand.operands);
  }
};

template <> struct MappingTraits<YamlRecipeBindingPatternModel> {
  static void mapping(IO &io, YamlRecipeBindingPatternModel &binding) {
    io.mapRequired("kind", binding.kind);
    io.mapRequired("capture", binding.capture);
    io.mapRequired("opcode", binding.opcode);
    io.mapOptional("operands", binding.operands);
  }
};

template <> struct MappingTraits<YamlRecipeEmitOperandModel> {
  static void mapping(IO &io, YamlRecipeEmitOperandModel &operand) {
    io.mapRequired("index", operand.index);
    io.mapRequired("kind", operand.kind);
    io.mapOptional("capture", operand.capture);
    io.mapOptional("id", operand.id);
    io.mapOptional("value", operand.value, 0u);
  }
};

template <> struct MappingTraits<YamlRecipeEmitModel> {
  static void mapping(IO &io, YamlRecipeEmitModel &emit) {
    io.mapRequired("kind", emit.kind);
    io.mapOptional("id", emit.id);
    io.mapOptional("resource", emit.resource);
    io.mapOptional("handle", emit.handle);
    io.mapOptional("opcode", emit.opcode);
    io.mapOptional("type", emit.type);
    io.mapOptional("aggregate", emit.aggregate);
    io.mapOptional("index", emit.index, 0u);
    io.mapOptional("operands", emit.operands);
  }
};

template <> struct MappingTraits<YamlRecipeMatchModel> {
  static void mapping(IO &io, YamlRecipeMatchModel &match) {
    io.mapRequired("opcode", match.opcode);
    io.mapRequired("replace", match.replace);
    io.mapOptional("capture", match.capture);
    io.mapOptional("mode", match.mode, std::string("Replace"));
    io.mapOptional("prune_dead", match.prune_dead, true);
    io.mapOptional("prune_captures", match.prune_captures);
    io.mapOptional("operands", match.operands);
  }
};

template <> struct MappingTraits<YamlRecipeRuleModel> {
  static void mapping(IO &io, YamlRecipeRuleModel &rule) {
    io.mapRequired("id", rule.id);
    io.mapOptional("name", rule.name);
    io.mapRequired("match", rule.match);
    io.mapOptional("bindings", rule.bindings);
    io.mapOptional("emit", rule.emit);
    io.mapOptional("replace_with", rule.replace_with);
    io.mapOptional("replace_with_capture", rule.replace_with_capture);
  }
};

template <> struct MappingTraits<YamlRecipeStepModel> {
  static void mapping(IO &io, YamlRecipeStepModel &step) {
    io.mapRequired("kind", step.kind);
    io.mapOptional("id", step.id);
    io.mapOptional("rule", step.rule);
    io.mapOptional("name", step.name);
    io.mapOptional("mode", step.mode, std::string("Once"));
    io.mapOptional("required", step.required, true);
  }
};

template <> struct MappingTraits<YamlRecipeOptionsModel> {
  static void mapping(IO &io, YamlRecipeOptionsModel &options) {
    io.mapOptional("restore_reflection", options.restore_reflection, true);
    io.mapOptional("refresh_resources", options.refresh_resources, false);
    io.mapOptional("verify_module", options.verify_module, true);
  }
};

template <> struct MappingTraits<YamlRecipeDocumentModel> {
  static void mapping(IO &io, YamlRecipeDocumentModel &document) {
    io.mapOptional("version", document.version, 1u);
    io.mapOptional("options", document.options);
    io.mapOptional("resources", document.resources);
    io.mapOptional("rewrite_rules", document.rewrite_rules);
    io.mapOptional("steps", document.steps);
  }
};

} // namespace yaml
} // namespace llvm

static bool ParseYamlRecipeBindingModel(const YamlRecipeBindingModel &bindingModel,
                                        ResourceBindingDesc &binding,
                                        std::string &error) {
  if (bindingModel.bind == "auto") {
    binding.Auto(bindingModel.space);
    return true;
  }

  unsigned bindPoint = 0;
  if (!ParseRecipeUnsignedValue(bindingModel.bind, bindPoint, error))
    return false;

  binding.Register(bindPoint, bindingModel.space);
  return true;
}

static bool ParseYamlRecipeOperandModel(const YamlRecipeOperandModel &operandModel,
                                        DxilOperandPattern &operandPattern,
                                        std::string &error) {
  operandPattern = DxilOperandPattern();
  operandPattern.operandIndex = operandModel.index;
  operandPattern.captureName = operandModel.capture;

  const std::string loweredKind = LowercaseRecipeToken(operandModel.kind);
  if (loweredKind == "any") {
    operandPattern.kind = DxilOperandPatternKind::Any;
  } else if (loweredKind == "constant_int") {
    operandPattern.kind = DxilOperandPatternKind::ConstantInt;
    operandPattern.constantIntValue = operandModel.value;
  } else if (loweredKind == "resource_handle") {
    operandPattern.kind = DxilOperandPatternKind::ResourceHandle;
  } else if (loweredKind == "instruction") {
    operandPattern.kind = DxilOperandPatternKind::Instruction;
    if (operandModel.opcode.empty() ||
        !ParseRecipeInstructionOpcode(operandModel.opcode,
                                      operandPattern.instructionOpcode,
                                      error)) {
      if (error.empty())
        error = "instruction operands require opcode";
      return false;
    }
  } else if (loweredKind == "dxop") {
    operandPattern.kind = DxilOperandPatternKind::DxOpCall;
    operandPattern.matchDxilOpCode = true;
    if (operandModel.opcode.empty() ||
        !ParseRecipeOpCode(operandModel.opcode, operandPattern.dxilOpCode, error)) {
      if (error.empty())
        error = "dxop operands require opcode";
      return false;
    }
  } else {
    error = "unsupported operand kind '" + operandModel.kind + "'";
    return false;
  }

  if (!operandModel.resource_class.empty()) {
    if (!ParseRecipeResourceClass(operandModel.resource_class,
                                  operandPattern.resourceClass,
                                  error)) {
      return false;
    }
    operandPattern.matchResourceClass = true;
  }

  if (!operandModel.resource_kind.empty()) {
    if (!ParseRecipeResourceKind(operandModel.resource_kind,
                                 operandPattern.resourceKind,
                                 error)) {
      return false;
    }
    operandPattern.matchResourceKind = true;
  }

  operandPattern.resourceName = operandModel.resource_name;
  operandPattern.resourceNameLikePattern = operandModel.resource_name_like;
  if (!operandPattern.resourceNameLikePattern.empty()) {
    std::string regexError;
    llvm::Regex resourceNameRegex(operandPattern.resourceNameLikePattern);
    if (!resourceNameRegex.isValid(regexError)) {
      error = "invalid resource_name_like regex '" +
              operandPattern.resourceNameLikePattern + "': " + regexError;
      return false;
    }
  }

  if (operandModel.bind >= 0)
    operandPattern.resourceBindPoint = operandModel.bind;
  if (operandModel.space >= 0)
    operandPattern.resourceSpace = operandModel.space;

  operandPattern.operandPatterns.clear();
  for (const YamlRecipeOperandModel &childModel : operandModel.operands) {
    DxilOperandPattern childPattern;
    if (!ParseYamlRecipeOperandModel(childModel, childPattern, error))
      return false;
    operandPattern.operandPatterns.push_back(std::move(childPattern));
  }

  SortOperandPatternTree(operandPattern.operandPatterns);
  return true;
}

static bool ParseYamlRecipeEmitOperandModel(
    const YamlRecipeEmitOperandModel &operandModel,
    const std::unordered_map<std::string, TextureResourceDesc> &parsedTextures,
    const std::unordered_map<std::string, TextureResourceDesc> &parsedUavs,
    const std::unordered_map<std::string, CBufferDesc> &parsedCBuffers,
    const std::unordered_map<std::string, SamplerDesc> &parsedSamplers,
    DxilRewriteEmitOperand &emitOperand,
    std::string &error) {
  emitOperand = DxilRewriteEmitOperand();
  emitOperand.operandIndex = operandModel.index;

  const std::string loweredKind = LowercaseRecipeToken(operandModel.kind);
  if (loweredKind == "capture") {
    if (operandModel.capture.empty()) {
      error = "capture emit operands require capture";
      return false;
    }
    emitOperand.kind = DxilRewriteEmitOperandKind::Capture;
    emitOperand.captureName = operandModel.capture;
  } else if (loweredKind == "temporary") {
    if (operandModel.id.empty()) {
      error = "temporary emit operands require id";
      return false;
    }
    emitOperand.kind = DxilRewriteEmitOperandKind::Temporary;
    emitOperand.temporaryName = operandModel.id;
  } else if (loweredKind == "constant_int") {
    emitOperand.kind = DxilRewriteEmitOperandKind::ConstantInt;
    emitOperand.constantIntValue = operandModel.value;
  } else if (loweredKind == "resource") {
    ParsedRecipeResourceRef resourceRef;
    TryResolveParsedRecipeResourceRef(operandModel.id,
                                     parsedTextures,
                                     parsedUavs,
                                     parsedCBuffers,
                                     parsedSamplers,
                                     resourceRef);
    if (!resourceRef.found) {
      error = "unknown resource id '" + operandModel.id + "'";
      return false;
    }
    emitOperand.kind = DxilRewriteEmitOperandKind::ResourceHandle;
    emitOperand.resourceName = resourceRef.resourceName;
    emitOperand.resourceBinding = resourceRef.binding;
  } else if (loweredKind == "undef") {
    emitOperand.kind = DxilRewriteEmitOperandKind::Undef;
  } else {
    error = "unsupported emit operand kind '" + operandModel.kind + "'";
    return false;
  }

  return true;
}

static bool ParseDxilRecipeTextAsYaml(llvm::StringRef recipeText,
                                      DxilRecipeParseResult &result,
                                      llvm::StringRef sourceName) {
  result = DxilRecipeParseResult();
  YamlRecipeDocumentModel document;
  llvm::yaml::Input input(recipeText);
  input >> document;
  if (input.error()) {
    result.error = sourceName.str() + ": " + input.error().message();
    return false;
  }

  if (document.version != 1) {
    result.error = sourceName.str() + ": unsupported recipe schema version";
    return false;
  }

  result.patchOptions.restoreReflection = document.options.restore_reflection;
  result.patchOptions.refreshResources = document.options.refresh_resources;
  result.patchOptions.verifyModule = document.options.verify_module;
  bool hasExplicitRefreshOption = true;
  bool requiresResourceRefresh = false;

  std::unordered_map<std::string, DxilRewriteRule> parsedRewriteRules;
  std::unordered_map<std::string, TextureResourceDesc> parsedTextures;
  std::unordered_map<std::string, TextureResourceDesc> parsedUavs;
  std::unordered_map<std::string, CBufferDesc> parsedCBuffers;
  std::unordered_map<std::string, SamplerDesc> parsedSamplers;
  std::string parseError;

  auto parseTextureModel = [&](const YamlRecipeTextureModel &textureModel,
                               bool isUav,
                               std::unordered_map<std::string, TextureResourceDesc> &outMap) -> bool {
    TextureResourceDesc desc;
    desc.name = textureModel.name;
    if (!ParseRecipeResourceKind(textureModel.kind, desc.kind, parseError) ||
        !ParseRecipeComponentType(textureModel.element, desc.elementKind, parseError)) {
      return false;
    }
    desc.vectorWidth = textureModel.width;
    if (isUav) {
      desc.binding.AsUAV();
      desc.isReadWrite = true;
    } else {
      desc.binding.AsSRV();
      desc.isReadWrite = false;
    }
    if (!ParseYamlRecipeBindingModel(textureModel.binding, desc.binding, parseError))
      return false;
    return outMap.emplace(textureModel.id, std::move(desc)).second;
  };

  for (const YamlRecipeTextureModel &textureModel : document.resources.textures) {
    if (!parseTextureModel(textureModel, false, parsedTextures)) {
      result.error = sourceName.str() + ": invalid texture resource '" + textureModel.id + "': " + parseError;
      return false;
    }
  }
  for (const YamlRecipeTextureModel &textureModel : document.resources.texture_uavs) {
    if (!parseTextureModel(textureModel, true, parsedUavs)) {
      result.error = sourceName.str() + ": invalid texture_uav resource '" + textureModel.id + "': " + parseError;
      return false;
    }
  }

  for (const YamlRecipeCBufferModel &cbufferModel : document.resources.cbuffers) {
    CBufferDesc desc;
    CBufferSchema *schema = new CBufferSchema();
    schema->typeName = cbufferModel.type;
    schema->sizeInBytes = cbufferModel.size;
    for (const YamlRecipeFieldModel &fieldModel : cbufferModel.fields) {
      CBufferFieldDesc field;
      field.name = fieldModel.name;
      if (!ParseRecipeCompTypeKind(fieldModel.type, field.compType, parseError)) {
        delete schema;
        result.error = sourceName.str() + ": invalid cbuffer field '" + fieldModel.name + "': " + parseError;
        return false;
      }
      field.vectorSize = fieldModel.width;
      field.offset = fieldModel.offset;
      schema->fields.push_back(std::move(field));
    }
    desc.name = cbufferModel.name;
    desc.binding.AsCBuffer();
    if (!ParseYamlRecipeBindingModel(cbufferModel.binding, desc.binding, parseError)) {
      delete schema;
      result.error = sourceName.str() + ": invalid cbuffer binding for '" + cbufferModel.id + "': " + parseError;
      return false;
    }
    desc.sizeInBytes = schema->sizeInBytes;
    desc.schema = schema;
    if (!parsedCBuffers.emplace(cbufferModel.id, std::move(desc)).second) {
      delete schema;
      result.error = sourceName.str() + ": duplicate cbuffer id '" + cbufferModel.id + "'";
      return false;
    }
  }

  for (const YamlRecipeSamplerModel &samplerModel : document.resources.samplers) {
    SamplerDesc desc;
    desc.name = samplerModel.name;
    if (!ParseYamlRecipeBindingModel(samplerModel.binding, desc.binding, parseError)) {
      result.error = sourceName.str() + ": invalid sampler binding for '" + samplerModel.id + "': " + parseError;
      return false;
    }
    if (!parsedSamplers.emplace(samplerModel.id, std::move(desc)).second) {
      result.error = sourceName.str() + ": duplicate sampler id '" + samplerModel.id + "'";
      return false;
    }
  }

  for (const YamlRecipeRuleModel &ruleModel : document.rewrite_rules) {
    DxilRewriteRule rule;
    rule.name = ruleModel.name.empty() ? ruleModel.id : ruleModel.name;
    rule.replaceCaptureName = ruleModel.match.replace;
    rule.replacementCaptureName = ruleModel.replace_with_capture;
    rule.pruneDeadInstructions = ruleModel.match.prune_dead;
    rule.pruneCaptureNames = ruleModel.match.prune_captures;
    if (!ParseRecipeRewriteMode(ruleModel.match.mode, rule.mode, parseError)) {
      result.error = sourceName.str() + ": invalid rewrite rule mode for '" + ruleModel.id + "': " + parseError;
      return false;
    }

    hlsl::OP::OpCode rootOpcode = static_cast<hlsl::OP::OpCode>(0);
    if (!ParseRecipeOpCode(ruleModel.match.opcode, rootOpcode, parseError)) {
      result.error = sourceName.str() + ": invalid rewrite rule opcode for '" + ruleModel.id + "': " + parseError;
      return false;
    }

    std::vector<DxilOperandPattern> rootOperands;
    rootOperands.push_back(ConstantIntOperand(0, static_cast<uint64_t>(rootOpcode)).Build());
    for (const YamlRecipeOperandModel &operandModel : ruleModel.match.operands) {
      DxilOperandPattern operandPattern;
      if (!ParseYamlRecipeOperandModel(operandModel, operandPattern, parseError)) {
        result.error = sourceName.str() + ": invalid operand in rewrite rule '" + ruleModel.id + "': " + parseError;
        return false;
      }
      rootOperands.push_back(std::move(operandPattern));
    }
    SortOperandPatternTree(rootOperands);
    const std::string rootCaptureName =
        ruleModel.match.capture.empty() ? rule.replaceCaptureName : ruleModel.match.capture;
    rule.pattern = DxOpCall(rootOpcode).Capture(rootCaptureName).Args(std::move(rootOperands)).Build();

    for (const YamlRecipeBindingPatternModel &bindingModel : ruleModel.bindings) {
      if (LowercaseRecipeToken(bindingModel.kind) != "dxop") {
        result.error = sourceName.str() + ": unsupported binding kind '" + bindingModel.kind + "'";
        return false;
      }

      hlsl::OP::OpCode bindingOpcode = static_cast<hlsl::OP::OpCode>(0);
      if (!ParseRecipeOpCode(bindingModel.opcode, bindingOpcode, parseError)) {
        result.error = sourceName.str() + ": invalid binding opcode for rule '" + ruleModel.id + "': " + parseError;
        return false;
      }

      DxilCallPattern bindingPattern = DxOpCall(bindingOpcode).Capture(bindingModel.capture).Build();
      bindingPattern.operandPatterns.push_back(ConstantIntOperand(0, static_cast<uint64_t>(bindingOpcode)).Build());
      for (const YamlRecipeOperandModel &operandModel : bindingModel.operands) {
        DxilOperandPattern operandPattern;
        if (!ParseYamlRecipeOperandModel(operandModel, operandPattern, parseError)) {
          result.error = sourceName.str() + ": invalid binding operand in rule '" + ruleModel.id + "': " + parseError;
          return false;
        }
        bindingPattern.operandPatterns.push_back(std::move(operandPattern));
      }
      SortOperandPatternTree(bindingPattern.operandPatterns);
      rule.bindingPatterns.push_back(std::move(bindingPattern));
    }

    for (const YamlRecipeEmitModel &emitModel : ruleModel.emit) {
      const std::string loweredKind = LowercaseRecipeToken(emitModel.kind);
      if (loweredKind == "create_handle") {
        ParsedRecipeResourceRef resourceRef;
        TryResolveParsedRecipeResourceRef(emitModel.resource,
                                         parsedTextures,
                                         parsedUavs,
                                         parsedCBuffers,
                                         parsedSamplers,
                                         resourceRef);
        if (!resourceRef.found) {
          result.error = sourceName.str() + ": unknown resource id '" + emitModel.resource + "'";
          return false;
        }
        rule.emittedSequence.values.push_back(
            EmitCreateHandleValue(emitModel.id, resourceRef.resourceName, resourceRef.binding));
      } else if (loweredKind == "annotate_handle") {
        ParsedRecipeResourceRef resourceRef;
        TryResolveParsedRecipeResourceRef(emitModel.resource,
                                         parsedTextures,
                                         parsedUavs,
                                         parsedCBuffers,
                                         parsedSamplers,
                                         resourceRef);
        if (!resourceRef.found) {
          result.error = sourceName.str() + ": unknown resource id '" + emitModel.resource + "'";
          return false;
        }
        rule.emittedSequence.values.push_back(
            EmitAnnotateHandleValue(emitModel.id,
                                    emitModel.handle,
                                    resourceRef.resourceName,
                                    resourceRef.binding));
      } else if (loweredKind == "call") {
        DxilRewriteEmitValue emittedValue;
        emittedValue.name = emitModel.id;
        emittedValue.kind = DxilRewriteEmitValueKind::DxOpCall;
        if (!ParseRecipeOpCode(emitModel.opcode, emittedValue.dxilOpCode, parseError)) {
          result.error = sourceName.str() + ": invalid emit call opcode in rule '" + ruleModel.id + "': " + parseError;
          return false;
        }
        if (!emitModel.type.empty()) {
          if (!ParseRecipeComponentType(emitModel.type, emittedValue.resultComponentType, parseError)) {
            result.error = sourceName.str() + ": invalid emit call type in rule '" + ruleModel.id + "': " + parseError;
            return false;
          }
          emittedValue.hasExplicitResultComponentType = true;
        }
        for (const YamlRecipeEmitOperandModel &operandModel : emitModel.operands) {
          DxilRewriteEmitOperand emitOperand;
          if (!ParseYamlRecipeEmitOperandModel(operandModel,
                                               parsedTextures,
                                               parsedUavs,
                                               parsedCBuffers,
                                               parsedSamplers,
                                               emitOperand,
                                               parseError)) {
            result.error = sourceName.str() + ": invalid emit operand in rule '" + ruleModel.id + "': " + parseError;
            return false;
          }
          emittedValue.operands.push_back(std::move(emitOperand));
        }
        rule.emittedSequence.values.push_back(std::move(emittedValue));
      } else if (loweredKind == "extract") {
        rule.emittedSequence.values.push_back(
            EmitExtractValue(emitModel.id, emitModel.aggregate, emitModel.index));
      } else if (loweredKind == "binop") {
        unsigned instructionOpcode = 0;
        hlsl::DXIL::ComponentType componentType = hlsl::DXIL::ComponentType::Invalid;
        if (!ParseRecipeBinaryInstructionOpcode(emitModel.opcode, instructionOpcode, parseError) ||
            !ParseRecipeComponentType(emitModel.type, componentType, parseError)) {
          result.error = sourceName.str() + ": invalid binop emit in rule '" + ruleModel.id + "': " + parseError;
          return false;
        }
        DxilRewriteEmitValue emittedValue =
            EmitBinaryInstructionValue(emitModel.id, instructionOpcode, componentType, {});
        for (const YamlRecipeEmitOperandModel &operandModel : emitModel.operands) {
          DxilRewriteEmitOperand emitOperand;
          if (!ParseYamlRecipeEmitOperandModel(operandModel,
                                               parsedTextures,
                                               parsedUavs,
                                               parsedCBuffers,
                                               parsedSamplers,
                                               emitOperand,
                                               parseError)) {
            result.error = sourceName.str() + ": invalid binop operand in rule '" + ruleModel.id + "': " + parseError;
            return false;
          }
          emittedValue.operands.push_back(std::move(emitOperand));
        }
        rule.emittedSequence.values.push_back(std::move(emittedValue));
      } else if (loweredKind == "cast") {
        unsigned castOpcode = 0;
        hlsl::DXIL::ComponentType componentType = hlsl::DXIL::ComponentType::Invalid;
        if (!ParseRecipeCastInstructionOpcode(emitModel.opcode, castOpcode, parseError) ||
            !ParseRecipeComponentType(emitModel.type, componentType, parseError)) {
          result.error = sourceName.str() + ": invalid cast emit in rule '" + ruleModel.id + "': " + parseError;
          return false;
        }
        DxilRewriteEmitValue emittedValue =
            EmitCastInstructionValue(emitModel.id, castOpcode, componentType, {});
        for (const YamlRecipeEmitOperandModel &operandModel : emitModel.operands) {
          DxilRewriteEmitOperand emitOperand;
          if (!ParseYamlRecipeEmitOperandModel(operandModel,
                                               parsedTextures,
                                               parsedUavs,
                                               parsedCBuffers,
                                               parsedSamplers,
                                               emitOperand,
                                               parseError)) {
            result.error = sourceName.str() + ": invalid cast operand in rule '" + ruleModel.id + "': " + parseError;
            return false;
          }
          emittedValue.operands.push_back(std::move(emitOperand));
        }
        rule.emittedSequence.values.push_back(std::move(emittedValue));
      } else {
        result.error = sourceName.str() + ": unsupported emit kind '" + emitModel.kind + "'";
        return false;
      }
    }

    rule.emittedSequence.replacementValueName = ruleModel.replace_with;
    if (rule.replacementCaptureName.empty() == rule.emittedSequence.replacementValueName.empty()) {
      result.error = sourceName.str() + ": rewrite rule '" + ruleModel.id + "' must provide exactly one of replace_with or replace_with_capture";
      return false;
    }

    if (!parsedRewriteRules.emplace(ruleModel.id, std::move(rule)).second) {
      result.error = sourceName.str() + ": duplicate rewrite rule id '" + ruleModel.id + "'";
      return false;
    }
  }

  for (const YamlRecipeStepModel &stepModel : document.steps) {
    const std::string loweredKind = LowercaseRecipeToken(stepModel.kind);
      if (loweredKind == "add_texture") {
        auto it = parsedTextures.find(stepModel.id);
        if (it == parsedTextures.end()) {
          result.error = sourceName.str() + ": unknown texture id '" + stepModel.id + "'";
          return false;
        }
        result.recipe.AddStep(MakeAddTextureStep(stepModel.id, it->second));
        requiresResourceRefresh = true;
      } else if (loweredKind == "add_texture_uav") {
        auto it = parsedUavs.find(stepModel.id);
        if (it == parsedUavs.end()) {
          result.error = sourceName.str() + ": unknown texture_uav id '" + stepModel.id + "'";
          return false;
        }
        result.recipe.AddStep(MakeAddTextureUAVStep(stepModel.id, it->second));
        requiresResourceRefresh = true;
      } else if (loweredKind == "add_cbuffer") {
        auto it = parsedCBuffers.find(stepModel.id);
        if (it == parsedCBuffers.end()) {
          result.error = sourceName.str() + ": unknown cbuffer id '" + stepModel.id + "'";
          return false;
        }
        result.recipe.AddStep(MakeAddCBufferStep(stepModel.id, it->second));
        requiresResourceRefresh = true;
      } else if (loweredKind == "add_sampler") {
        auto it = parsedSamplers.find(stepModel.id);
        if (it == parsedSamplers.end()) {
          result.error = sourceName.str() + ": unknown sampler id '" + stepModel.id + "'";
          return false;
        }
        result.recipe.AddStep(MakeAddSamplerStep(stepModel.id, it->second));
        requiresResourceRefresh = true;
      } else if (loweredKind == "apply_rule") {
        auto it = parsedRewriteRules.find(stepModel.rule);
        if (it == parsedRewriteRules.end()) {
          result.error = sourceName.str() + ": unknown rewrite rule '" + stepModel.rule + "'";
          return false;
        }

        DxilRecipeRuleApplicationMode applicationMode = DxilRecipeRuleApplicationMode::Once;
        if (!ParseRecipeRuleApplicationMode(stepModel.mode, applicationMode, parseError)) {
          result.error = sourceName.str() + ": invalid apply_rule mode for '" + stepModel.rule + "': " + parseError;
          return false;
        }

        const std::string stepName = stepModel.name.empty() ?
            ("apply_rule:" + stepModel.rule) : stepModel.name;
        result.recipe.AddStep(
            MakeApplyRewriteRulesStep(stepName, {it->second}, applicationMode, stepModel.required));
      } else if (loweredKind == "expect_texture") {
        result.recipe.AddStep(MakeExpectTextureStep(stepModel.id));
      } else if (loweredKind == "expect_texture_uav") {
        result.recipe.AddStep(MakeExpectTextureUAVStep(stepModel.id));
      } else if (loweredKind == "expect_cbuffer") {
        result.recipe.AddStep(MakeExpectCBufferStep(stepModel.id));
      } else if (loweredKind == "refresh_resources") {
        result.recipe.AddStep(MakeRefreshResourcesStep());
      } else if (loweredKind == "prune_dead_code") {
        result.recipe.AddStep(MakePruneDeadCodeStep());
      } else if (loweredKind == "verify_module") {
        result.recipe.AddStep(MakeVerifyModuleStep());
      } else {
        result.error = sourceName.str() + ": unsupported step kind '" + stepModel.kind + "'";
        return false;
      }
  }

  if (!hasExplicitRefreshOption)
    result.patchOptions.refreshResources = requiresResourceRefresh;

  return true;
}

bool ParseDxilRecipeText(llvm::StringRef recipeText,
                         DxilRecipeParseResult &result,
                         llvm::StringRef sourceName) {
  return ParseDxilRecipeTextAsYaml(recipeText, result, sourceName);

  result = DxilRecipeParseResult();
  result.patchOptions.restoreReflection = true;
  result.patchOptions.refreshResources = false;
  result.patchOptions.verifyModule = true;
  bool hasExplicitRefreshOption = false;
  bool requiresResourceRefresh = false;

  std::unique_ptr<PendingCBufferBlock> pendingCBuffer;
  std::unique_ptr<PendingRewriteRuleBlock> pendingRewriteRule;
  std::unordered_map<std::string, DxilRewriteRule> parsedRewriteRules;
  std::unordered_map<std::string, TextureResourceDesc> parsedTextures;
  std::unordered_map<std::string, TextureResourceDesc> parsedUavs;
  std::unordered_map<std::string, CBufferDesc> parsedCBuffers;
  std::unordered_map<std::string, SamplerDesc> parsedSamplers;
  std::istringstream recipeStream(recipeText.str());
  std::string line;
  unsigned lineNumber = 0;
  const std::string sourceNameString = sourceName.str();
  while (std::getline(recipeStream, line)) {
    ++lineNumber;
    const std::string trimmed = TrimRecipeText(line);
    if (trimmed.empty() || trimmed[0] == '#')
      continue;

    const std::vector<std::string> tokens = TokenizeRecipeLine(trimmed);
    if (tokens.empty())
      continue;

    std::unordered_map<std::string, std::string> assignments;
    std::string parseError;
    if (!ParseRecipeAssignments(tokens, 1, assignments, parseError)) {
      return FailRecipeParse(result, sourceNameString, lineNumber, parseError);
    }

    const std::string &command = tokens[0];
    if (pendingRewriteRule != nullptr) {
      if (command == "emit_cast") {
        const std::string *idText = FindRecipeAssignment(assignments, "id");
        const std::string *opcodeText = FindRecipeAssignment(assignments, "opcode");
        const std::string *typeText = FindRecipeAssignment(assignments, "type");
        if (idText == nullptr || opcodeText == nullptr || typeText == nullptr) {
          return FailRecipeParse(result,
                                 sourceNameString,
                                 lineNumber,
                                 "emit_cast requires id=, opcode=, and type=");
        }

        unsigned castOpcode = 0;
        if (!ParseRecipeCastInstructionOpcode(*opcodeText,
                                              castOpcode,
                                              parseError)) {
          return FailRecipeParse(result,
                                 sourceNameString,
                                 lineNumber,
                                 parseError);
        }

        hlsl::DXIL::ComponentType componentType = hlsl::DXIL::ComponentType::Invalid;
        if (!ParseRecipeComponentType(*typeText, componentType, parseError)) {
          return FailRecipeParse(result,
                                 sourceNameString,
                                 lineNumber,
                                 parseError);
        }

        pendingRewriteRule->rule.emittedSequence.values.push_back(
            EmitCastInstructionValue(*idText, castOpcode, componentType, {}));
        pendingRewriteRule->activeEmitValueIndex =
            static_cast<int>(pendingRewriteRule->rule.emittedSequence.values.size()) - 1;
        continue;
      }

      if (command == "emit_binop") {
        const std::string *idText = FindRecipeAssignment(assignments, "id");
        const std::string *opcodeText = FindRecipeAssignment(assignments, "opcode");
        const std::string *typeText = FindRecipeAssignment(assignments, "type");
        if (idText == nullptr || opcodeText == nullptr || typeText == nullptr) {
          return FailRecipeParse(result,
                                 sourceNameString,
                                 lineNumber,
                                 "emit_binop requires id=, opcode=, and type=");
        }

        unsigned instructionOpcode = 0;
        if (!ParseRecipeBinaryInstructionOpcode(*opcodeText,
                                                instructionOpcode,
                                                parseError)) {
          return FailRecipeParse(result,
                                 sourceNameString,
                                 lineNumber,
                                 parseError);
        }

        hlsl::DXIL::ComponentType componentType = hlsl::DXIL::ComponentType::Invalid;
        if (!ParseRecipeComponentType(*typeText, componentType, parseError)) {
          return FailRecipeParse(result,
                                 sourceNameString,
                                 lineNumber,
                                 parseError);
        }

        pendingRewriteRule->rule.emittedSequence.values.push_back(
            EmitBinaryInstructionValue(*idText,
                                       instructionOpcode,
                                       componentType,
                                       {}));
        pendingRewriteRule->activeEmitValueIndex =
            static_cast<int>(pendingRewriteRule->rule.emittedSequence.values.size()) - 1;
        continue;
      }

      if (command == "emit_create_handle") {
        const std::string *idText = FindRecipeAssignment(assignments, "id");
        const std::string *resourceIdText =
            FindRecipeAssignment(assignments, "resource");
        if (idText == nullptr || resourceIdText == nullptr) {
          return FailRecipeParse(result,
                                 sourceNameString,
                                 lineNumber,
                                 "emit_create_handle requires id= and resource=");
        }

        ParsedRecipeResourceRef resourceRef;
        TryResolveParsedRecipeResourceRef(*resourceIdText,
                                         parsedTextures,
                                         parsedUavs,
                                         parsedCBuffers,
                                         parsedSamplers,
                                         resourceRef);
        if (!resourceRef.found) {
          return FailRecipeParse(result,
                                 sourceNameString,
                                 lineNumber,
                                 "unknown resource id '" + *resourceIdText + "'");
        }

        pendingRewriteRule->rule.emittedSequence.values.push_back(
            EmitCreateHandleValue(*idText,
                                  resourceRef.resourceName,
                                  resourceRef.binding));
        pendingRewriteRule->activeEmitValueIndex = -1;
        continue;
      }

      if (command == "emit_annotate_handle") {
        const std::string *idText = FindRecipeAssignment(assignments, "id");
        const std::string *handleText = FindRecipeAssignment(assignments, "handle");
        const std::string *resourceIdText =
            FindRecipeAssignment(assignments, "resource");
        if (idText == nullptr || handleText == nullptr || resourceIdText == nullptr) {
          return FailRecipeParse(result,
                                 sourceNameString,
                                 lineNumber,
                                 "emit_annotate_handle requires id=, handle=, and resource=");
        }

        ParsedRecipeResourceRef resourceRef;
        TryResolveParsedRecipeResourceRef(*resourceIdText,
                                         parsedTextures,
                                         parsedUavs,
                                         parsedCBuffers,
                                         parsedSamplers,
                                         resourceRef);
        if (!resourceRef.found) {
          return FailRecipeParse(result,
                                 sourceNameString,
                                 lineNumber,
                                 "unknown resource id '" + *resourceIdText + "'");
        }

        pendingRewriteRule->rule.emittedSequence.values.push_back(
            EmitAnnotateHandleValue(*idText,
                                    *handleText,
                                    resourceRef.resourceName,
                                    resourceRef.binding));
        pendingRewriteRule->activeEmitValueIndex = -1;
        continue;
      }

      if (command == "emit_call") {
        const std::string *idText = FindRecipeAssignment(assignments, "id");
        const std::string *opcodeText = FindRecipeAssignment(assignments, "opcode");
        if (idText == nullptr || opcodeText == nullptr) {
          return FailRecipeParse(result,
                                 sourceNameString,
                                 lineNumber,
                                 "emit_call requires id= and opcode=");
        }

        DxilRewriteEmitValue emittedValue;
        emittedValue.name = *idText;
        emittedValue.kind = DxilRewriteEmitValueKind::DxOpCall;
        if (!ParseRecipeOpCode(*opcodeText, emittedValue.dxilOpCode, parseError)) {
          return FailRecipeParse(result, sourceNameString, lineNumber, parseError);
        }

        const std::string *typeText = FindRecipeAssignment(assignments, "type");
        if (typeText != nullptr) {
          if (!ParseRecipeComponentType(*typeText,
                                        emittedValue.resultComponentType,
                                        parseError)) {
            return FailRecipeParse(result,
                                   sourceNameString,
                                   lineNumber,
                                   parseError);
          }
          emittedValue.hasExplicitResultComponentType = true;
        }

        pendingRewriteRule->rule.emittedSequence.values.push_back(std::move(emittedValue));
        pendingRewriteRule->activeEmitValueIndex =
            static_cast<int>(pendingRewriteRule->rule.emittedSequence.values.size()) - 1;
        continue;
      }

      if (command == "emit_extract") {
        const std::string *idText = FindRecipeAssignment(assignments, "id");
        const std::string *aggregateText = FindRecipeAssignment(assignments, "aggregate");
        const std::string *indexText = FindRecipeAssignment(assignments, "index");
        if (idText == nullptr || aggregateText == nullptr || indexText == nullptr) {
          return FailRecipeParse(result,
                                 sourceNameString,
                                 lineNumber,
                                 "emit_extract requires id=, aggregate=, and index=");
        }

        unsigned extractIndex = 0;
        if (!ParseRecipeUnsignedValue(*indexText, extractIndex, parseError)) {
          return FailRecipeParse(result, sourceNameString, lineNumber, parseError);
        }

        pendingRewriteRule->rule.emittedSequence.values.push_back(
            EmitExtractValue(*idText, *aggregateText, extractIndex));
        pendingRewriteRule->activeEmitValueIndex = -1;
        continue;
      }

      if (command == "replace_with") {
        const std::string *idText = FindRecipeAssignment(assignments, "id");
        if (idText == nullptr) {
          return FailRecipeParse(result,
                                 sourceNameString,
                                 lineNumber,
                                 "replace_with requires id=");
        }

        pendingRewriteRule->rule.emittedSequence.replacementValueName = *idText;
        continue;
      }

      if (command == "operand") {
        const std::string *indexText = FindRecipeAssignment(assignments, "index");
        const std::string *kindText = FindRecipeAssignment(assignments, "kind");
        const std::string *captureText = FindRecipeAssignment(assignments, "capture");
        const std::string *parentText = FindRecipeAssignment(assignments, "parent");
        if (indexText == nullptr || kindText == nullptr) {
          return FailRecipeParse(result,
                                 sourceNameString,
                                 lineNumber,
                                 "operand requires index= and kind=");
        }

        unsigned operandIndex = 0;
        if (!ParseRecipeUnsignedValue(*indexText, operandIndex, parseError)) {
          return FailRecipeParse(result, sourceNameString, lineNumber, parseError);
        }

        DxilOperandPattern operandPattern;
        operandPattern.operandIndex = operandIndex;
        const std::string loweredKind = LowercaseRecipeToken(*kindText);
        if (loweredKind == "any") {
          operandPattern.kind = DxilOperandPatternKind::Any;
        } else if (loweredKind == "constant_int") {
          const std::string *valueText = FindRecipeAssignment(assignments, "value");
          if (valueText == nullptr) {
            return FailRecipeParse(result,
                                   sourceNameString,
                                   lineNumber,
                                   "constant_int operands require value=");
          }

          unsigned value = 0;
          if (!ParseRecipeUnsignedValue(*valueText, value, parseError)) {
            return FailRecipeParse(result,
                                   sourceNameString,
                                   lineNumber,
                                   parseError);
          }

          operandPattern.kind = DxilOperandPatternKind::ConstantInt;
          operandPattern.constantIntValue = value;
        } else if (loweredKind == "instruction") {
          const std::string *opcodeText = FindRecipeAssignment(assignments, "opcode");
          if (opcodeText == nullptr) {
            return FailRecipeParse(result,
                                   sourceNameString,
                                   lineNumber,
                                   "instruction operands require opcode=");
          }

          unsigned instructionOpcode = 0;
          if (!ParseRecipeInstructionOpcode(*opcodeText,
                                            instructionOpcode,
                                            parseError)) {
            return FailRecipeParse(result,
                                   sourceNameString,
                                   lineNumber,
                                   parseError);
          }

          operandPattern.kind = DxilOperandPatternKind::Instruction;
          operandPattern.instructionOpcode = instructionOpcode;
        } else if (loweredKind == "dxop") {
          const std::string *opcodeText = FindRecipeAssignment(assignments, "opcode");
          if (opcodeText == nullptr) {
            return FailRecipeParse(result,
                                   sourceNameString,
                                   lineNumber,
                                   "dxop operands require opcode=");
          }

          if (!ParseRecipeOpCode(*opcodeText, operandPattern.dxilOpCode, parseError)) {
            return FailRecipeParse(result,
                                   sourceNameString,
                                   lineNumber,
                                   parseError);
          }

          operandPattern.kind = DxilOperandPatternKind::DxOpCall;
          operandPattern.matchDxilOpCode = true;
        } else {
          return FailRecipeParse(result,
                                 sourceNameString,
                                 lineNumber,
                                 "unsupported operand kind '" + *kindText + "'");
        }

        if (captureText != nullptr)
          operandPattern.captureName = *captureText;

        PendingRewriteRuleBlock::PendingOperandPattern pendingOperandPattern;
        if (parentText != nullptr)
          pendingOperandPattern.parentCaptureName = *parentText;
        pendingOperandPattern.pattern = std::move(operandPattern);
        pendingRewriteRule->operandPatterns.push_back(std::move(pendingOperandPattern));
        continue;
      }

      if (command == "emit_operand") {
        const std::string *indexText = FindRecipeAssignment(assignments, "index");
        const std::string *kindText = FindRecipeAssignment(assignments, "kind");
        if (indexText == nullptr || kindText == nullptr) {
          return FailRecipeParse(result,
                                 sourceNameString,
                                 lineNumber,
                                 "emit_operand requires index= and kind=");
        }

        unsigned operandIndex = 0;
        if (!ParseRecipeUnsignedValue(*indexText, operandIndex, parseError)) {
          return FailRecipeParse(result, sourceNameString, lineNumber, parseError);
        }

        const bool useLegacyEmittedCall =
          pendingRewriteRule->activeEmitValueIndex < 0 &&
          pendingRewriteRule->rule.emittedCall.enabled;
        const bool useSequenceValue =
          pendingRewriteRule->activeEmitValueIndex >= 0 &&
          static_cast<size_t>(pendingRewriteRule->activeEmitValueIndex) <
            pendingRewriteRule->rule.emittedSequence.values.size();
        if (!useLegacyEmittedCall &&
          !useSequenceValue) {
          return FailRecipeParse(result,
                                 sourceNameString,
                                 lineNumber,
                     "emit_operand must follow an emit_call, emit_cast, emit_binop, or an emit= rewrite_rule header");
        }

        DxilRewriteEmitOperand emitOperand;
        emitOperand.operandIndex = operandIndex;
        const std::string loweredKind = LowercaseRecipeToken(*kindText);
        if (loweredKind == "capture") {
          const std::string *captureText = FindRecipeAssignment(assignments, "capture");
          if (captureText == nullptr) {
            return FailRecipeParse(result,
                                   sourceNameString,
                                   lineNumber,
                                   "capture emit operands require capture=");
          }
          emitOperand.kind = DxilRewriteEmitOperandKind::Capture;
          emitOperand.captureName = *captureText;
        } else if (loweredKind == "temporary") {
          const std::string *temporaryText =
              FindRecipeAssignment(assignments, "id");
          if (temporaryText == nullptr) {
            return FailRecipeParse(result,
                                   sourceNameString,
                                   lineNumber,
                                   "temporary emit operands require id=");
          }
          emitOperand.kind = DxilRewriteEmitOperandKind::Temporary;
          emitOperand.temporaryName = *temporaryText;
        } else if (loweredKind == "constant_int") {
          const std::string *valueText = FindRecipeAssignment(assignments, "value");
          if (valueText == nullptr) {
            return FailRecipeParse(result,
                                   sourceNameString,
                                   lineNumber,
                                   "constant_int emit operands require value=");
          }
          unsigned constantIntValue = 0;
          if (!ParseRecipeUnsignedValue(*valueText,
                                        constantIntValue,
                                        parseError)) {
            return FailRecipeParse(result,
                                   sourceNameString,
                                   lineNumber,
                                   parseError);
          }
          emitOperand.constantIntValue = constantIntValue;
          emitOperand.kind = DxilRewriteEmitOperandKind::ConstantInt;
        } else if (loweredKind == "resource") {
          const std::string *idText = FindRecipeAssignment(assignments, "id");
          if (idText == nullptr) {
            return FailRecipeParse(result,
                                   sourceNameString,
                                   lineNumber,
                                   "resource emit operands require id=");
          }

          ParsedRecipeResourceRef resourceRef;
          TryResolveParsedRecipeResourceRef(*idText,
                                           parsedTextures,
                                           parsedUavs,
                                           parsedCBuffers,
                                           parsedSamplers,
                                           resourceRef);
          if (!resourceRef.found) {
            return FailRecipeParse(result,
                                   sourceNameString,
                                   lineNumber,
                                   "unknown resource id '" + *idText + "'");
          }

          emitOperand.kind = DxilRewriteEmitOperandKind::ResourceHandle;
          emitOperand.resourceName = resourceRef.resourceName;
          emitOperand.resourceBinding = resourceRef.binding;
        } else if (loweredKind == "undef") {
          emitOperand.kind = DxilRewriteEmitOperandKind::Undef;
        } else {
          return FailRecipeParse(result,
                                 sourceNameString,
                                 lineNumber,
                                 "unsupported emit operand kind '" + *kindText + "'");
        }

        if (useLegacyEmittedCall) {
          pendingRewriteRule->rule.emittedCall.operands.push_back(
              std::move(emitOperand));
        } else {
          pendingRewriteRule->rule.emittedSequence.values[
              static_cast<size_t>(pendingRewriteRule->activeEmitValueIndex)]
              .operands.push_back(std::move(emitOperand));
        }
        continue;
      }

      if (command == "bind_dxop") {
        const std::string *captureText = FindRecipeAssignment(assignments, "capture");
        const std::string *opcodeText = FindRecipeAssignment(assignments, "opcode");
        if (captureText == nullptr || opcodeText == nullptr) {
          return FailRecipeParse(result,
                                 sourceNameString,
                                 lineNumber,
                                 "bind_dxop requires capture= and opcode=");
        }

        hlsl::OP::OpCode bindingOpcode = static_cast<hlsl::OP::OpCode>(0);
        if (!ParseRecipeOpCode(*opcodeText, bindingOpcode, parseError)) {
          return FailRecipeParse(result,
                                 sourceNameString,
                                 lineNumber,
                                 parseError);
        }

        DxilCallPattern bindingPattern =
          DxOpCall(bindingOpcode).Capture(*captureText).Build();
        bindingPattern.operandPatterns.push_back(
          ConstantIntOperand(0, static_cast<uint64_t>(bindingOpcode)).Build());

        const std::string *argText = FindRecipeAssignment(assignments, "arg");
        const std::string *valueText = FindRecipeAssignment(assignments, "value");
        if ((argText == nullptr) != (valueText == nullptr)) {
          return FailRecipeParse(result,
                                 sourceNameString,
                                 lineNumber,
                                 "bind_dxop requires arg= and value= together when constraining an operand");
        }

        if (argText != nullptr) {
          unsigned operandIndex = 0;
          if (!ParseRecipeUnsignedValue(*argText, operandIndex, parseError) ||
              operandIndex == 0) {
            if (parseError.empty())
              parseError = "bind_dxop arg= must be greater than zero";
            return FailRecipeParse(result,
                                   sourceNameString,
                                   lineNumber,
                                   parseError);
          }

          unsigned operandValue = 0;
          if (!ParseRecipeUnsignedValue(*valueText, operandValue, parseError)) {
            return FailRecipeParse(result,
                                   sourceNameString,
                                   lineNumber,
                                   parseError);
          }

          bindingPattern.operandPatterns.push_back(
              ConstantIntOperand(operandIndex, operandValue).Build());
        }

        pendingRewriteRule->rule.bindingPatterns.push_back(std::move(bindingPattern));
        continue;
      }

      if (command == "end") {
        if (pendingRewriteRule->rule.replacementCaptureName.empty() &&
            !pendingRewriteRule->rule.emittedCall.enabled &&
            pendingRewriteRule->rule.emittedSequence.replacementValueName.empty()) {
          return FailRecipeParse(result,
                                 sourceNameString,
                                 lineNumber,
                                 "rewrite_rule must provide with_capture=, emit=, or replace_with inside the block");
        }

        std::vector<DxilOperandPattern> operandPatterns;
        if (!BuildPendingOperandPatternTree(pendingRewriteRule->operandPatterns,
                    pendingRewriteRule->rootCaptureName,
                    operandPatterns,
                    parseError)) {
          return FailRecipeParse(result,
               sourceNameString,
               lineNumber,
               parseError);
        }

        std::vector<DxilOperandPattern> rootOperandPatterns;
        rootOperandPatterns.push_back(
            ConstantIntOperand(0,
                   static_cast<uint64_t>(pendingRewriteRule->dxilOpCode))
          .Build());
        rootOperandPatterns.insert(rootOperandPatterns.end(),
                 operandPatterns.begin(),
                 operandPatterns.end());

        pendingRewriteRule->rule.pattern =
            DxOpCall(pendingRewriteRule->dxilOpCode)
                .Capture(pendingRewriteRule->rootCaptureName)
          .Args(std::move(rootOperandPatterns))
                .Build();
        parsedRewriteRules[pendingRewriteRule->id] = pendingRewriteRule->rule;
        pendingRewriteRule.reset();
        continue;
      }

      return FailRecipeParse(result,
                             sourceNameString,
                             lineNumber,
                             "only operand, bind_dxop, emit_binop, emit_cast, emit_create_handle, emit_annotate_handle, emit_call, emit_extract, emit_operand, replace_with, or end is allowed inside rewrite_rule blocks");
    }

    if (pendingCBuffer != nullptr) {
      if (command == "field") {
        const std::string *name = FindRecipeAssignment(assignments, "name");
        const std::string *type = FindRecipeAssignment(assignments, "type");
        const std::string *widthText = FindRecipeAssignment(assignments, "width");
        const std::string *offsetText = FindRecipeAssignment(assignments, "offset");
        if (name == nullptr || type == nullptr || widthText == nullptr ||
            offsetText == nullptr) {
          return FailRecipeParse(result,
                                 sourceNameString,
                                 lineNumber,
                                 "field requires name=, type=, width=, and offset=");
        }

        CBufferFieldDesc field;
        field.name = *name;
        if (!ParseRecipeCompTypeKind(*type, field.compType, parseError) ||
            !ParseRecipeUnsignedValue(*widthText, field.vectorSize, parseError) ||
            !ParseRecipeUnsignedValue(*offsetText, field.offset, parseError)) {
          return FailRecipeParse(result, sourceNameString, lineNumber, parseError);
        }

        pendingCBuffer->schema.fields.push_back(std::move(field));
        continue;
      }

      if (command == "end") {
        if (pendingCBuffer->schema.fields.empty()) {
          return FailRecipeParse(result,
                                 sourceNameString,
                                 lineNumber,
                                 "cbuffer block must contain at least one field");
        }

        pendingCBuffer->desc.schema = &pendingCBuffer->schema;
        parsedCBuffers[pendingCBuffer->id] = pendingCBuffer->desc;
        result.recipe.AddStep(
            MakeAddCBufferStep(pendingCBuffer->id, pendingCBuffer->desc));
        requiresResourceRefresh = true;
        pendingCBuffer.reset();
        continue;
      }

      return FailRecipeParse(result,
                             sourceNameString,
                             lineNumber,
                             "only field or end is allowed inside add_cbuffer blocks");
    }

    if (command == "option") {
      if (assignments.size() != 1) {
        return FailRecipeParse(result,
                               sourceNameString,
                               lineNumber,
                               "option expects exactly one key=value assignment");
      }

      const auto &assignment = *assignments.begin();
      bool optionValue = false;
      if (!ParseRecipeBoolValue(assignment.second, optionValue, parseError)) {
        return FailRecipeParse(result, sourceNameString, lineNumber, parseError);
      }

      if (assignment.first == "restore_reflection") {
        result.patchOptions.restoreReflection = optionValue;
      } else if (assignment.first == "refresh_resources") {
        hasExplicitRefreshOption = true;
        result.patchOptions.refreshResources = optionValue;
      } else if (assignment.first == "verify_module") {
        result.patchOptions.verifyModule = optionValue;
      } else {
        return FailRecipeParse(result,
                               sourceNameString,
                               lineNumber,
                               "unknown option '" + assignment.first + "'");
      }
      continue;
    }

    if (command == "rewrite_rule") {
      const std::string *id = FindRecipeAssignment(assignments, "id");
      const std::string *opcodeText = FindRecipeAssignment(assignments, "opcode");
      const std::string *replaceText = FindRecipeAssignment(assignments, "replace");
      const std::string *withCaptureText =
          FindRecipeAssignment(assignments, "with_capture");
      const std::string *emitText = FindRecipeAssignment(assignments, "emit");
      if (id == nullptr || opcodeText == nullptr || replaceText == nullptr) {
        return FailRecipeParse(result,
                               sourceNameString,
                               lineNumber,
                               "rewrite_rule requires id=, opcode=, and replace=");
      }

      pendingRewriteRule = std::make_unique<PendingRewriteRuleBlock>();
      pendingRewriteRule->id = *id;
      pendingRewriteRule->rootCaptureName =
          FindRecipeAssignment(assignments, "capture") != nullptr
              ? *FindRecipeAssignment(assignments, "capture")
              : *replaceText;
      pendingRewriteRule->rule.name =
          FindRecipeAssignment(assignments, "name") != nullptr
              ? *FindRecipeAssignment(assignments, "name")
              : *id;
      pendingRewriteRule->rule.replaceCaptureName = *replaceText;
      if (withCaptureText != nullptr)
        pendingRewriteRule->rule.replacementCaptureName = *withCaptureText;

      if (emitText != nullptr) {
        pendingRewriteRule->rule.emittedCall.enabled = true;
        if (!ParseRecipeOpCode(*emitText,
                               pendingRewriteRule->rule.emittedCall.dxilOpCode,
                               parseError)) {
          return FailRecipeParse(result, sourceNameString, lineNumber, parseError);
        }

        const std::string *emitExtractText =
            FindRecipeAssignment(assignments, "emit_extract");
        if (emitExtractText != nullptr) {
          unsigned emitExtractIndex = 0;
          if (!ParseRecipeUnsignedValue(*emitExtractText,
                                        emitExtractIndex,
                                        parseError)) {
            return FailRecipeParse(result,
                                   sourceNameString,
                                   lineNumber,
                                   parseError);
          }
          pendingRewriteRule->rule.emittedCall.extractIndex =
              static_cast<int>(emitExtractIndex);
        }
      }

      const std::string *modeText = FindRecipeAssignment(assignments, "mode");
      if (modeText != nullptr &&
          !ParseRecipeRewriteMode(*modeText,
                                  pendingRewriteRule->rule.mode,
                                  parseError)) {
        return FailRecipeParse(result, sourceNameString, lineNumber, parseError);
      }

      const std::string *pruneDeadText =
          FindRecipeAssignment(assignments, "prune_dead");
      if (pruneDeadText != nullptr &&
          !ParseRecipeBoolValue(*pruneDeadText,
                                pendingRewriteRule->rule.pruneDeadInstructions,
                                parseError)) {
        return FailRecipeParse(result, sourceNameString, lineNumber, parseError);
      }

      const std::string *pruneCaptureText =
          FindRecipeAssignment(assignments, "prune_capture");
      if (pruneCaptureText != nullptr) {
        pendingRewriteRule->rule.pruneCaptureNames =
            SplitRecipeList(*pruneCaptureText, ',');
      }

      if (!ParseRecipeOpCode(*opcodeText,
                             pendingRewriteRule->dxilOpCode,
                             parseError)) {
        return FailRecipeParse(result, sourceNameString, lineNumber, parseError);
      }

      continue;
    }

    if (command == "apply_rule") {
      const std::string *ruleId = FindRecipeAssignment(assignments, "rule");
      if (ruleId == nullptr) {
        return FailRecipeParse(result,
                               sourceNameString,
                               lineNumber,
                               "apply_rule requires rule=");
      }

      auto ruleIt = parsedRewriteRules.find(*ruleId);
      if (ruleIt == parsedRewriteRules.end()) {
        return FailRecipeParse(result,
                               sourceNameString,
                               lineNumber,
                               "unknown rewrite rule '" + *ruleId + "'");
      }

      DxilRecipeRuleApplicationMode applicationMode =
          DxilRecipeRuleApplicationMode::Once;
      const std::string *modeText = FindRecipeAssignment(assignments, "mode");
      if (modeText != nullptr &&
          !ParseRecipeRuleApplicationMode(*modeText,
                                          applicationMode,
                                          parseError)) {
        return FailRecipeParse(result, sourceNameString, lineNumber, parseError);
      }

      bool required = true;
      const std::string *requiredText = FindRecipeAssignment(assignments, "required");
      if (requiredText != nullptr &&
          !ParseRecipeBoolValue(*requiredText, required, parseError)) {
        return FailRecipeParse(result, sourceNameString, lineNumber, parseError);
      }

      const std::string stepName = FindRecipeAssignment(assignments, "name") != nullptr
                                       ? *FindRecipeAssignment(assignments, "name")
                                       : ("apply_rule:" + *ruleId);
      result.recipe.AddStep(MakeApplyRewriteRulesStep(
          stepName, {ruleIt->second}, applicationMode, required));
      continue;
    }

    if (command == "add_texture" || command == "add_texture_uav") {
      const std::string *id = FindRecipeAssignment(assignments, "id");
      const std::string *name = FindRecipeAssignment(assignments, "name");
      const std::string *kindText = FindRecipeAssignment(assignments, "kind");
      const std::string *elementText = FindRecipeAssignment(assignments, "element");
      const std::string *widthText = FindRecipeAssignment(assignments, "width");
      if (id == nullptr || name == nullptr || kindText == nullptr ||
          elementText == nullptr || widthText == nullptr) {
        return FailRecipeParse(result,
                               sourceNameString,
                               lineNumber,
                               "add_texture requires id=, name=, kind=, element=, and width=");
      }

      TextureResourceDesc desc;
      desc.name = *name;
      if (!ParseRecipeResourceKind(*kindText, desc.kind, parseError) ||
          !ParseRecipeComponentType(*elementText, desc.elementKind, parseError) ||
          !ParseRecipeUnsignedValue(*widthText, desc.vectorWidth, parseError)) {
        return FailRecipeParse(result, sourceNameString, lineNumber, parseError);
      }

      if (command == "add_texture_uav") {
        desc.binding.AsUAV();
        desc.isReadWrite = true;
      } else {
        desc.binding.AsSRV();
        desc.isReadWrite = false;
      }

      if (!ParseRecipeBinding(assignments, desc.binding, parseError)) {
        return FailRecipeParse(result, sourceNameString, lineNumber, parseError);
      }

      if (command == "add_texture_uav")
        result.recipe.AddStep(MakeAddTextureUAVStep(*id, desc));
      else
        result.recipe.AddStep(MakeAddTextureStep(*id, desc));
      if (command == "add_texture_uav")
        parsedUavs[*id] = desc;
      else
        parsedTextures[*id] = desc;
      requiresResourceRefresh = true;
      continue;
    }

    if (command == "add_sampler") {
      const std::string *id = FindRecipeAssignment(assignments, "id");
      const std::string *name = FindRecipeAssignment(assignments, "name");
      if (id == nullptr || name == nullptr) {
        return FailRecipeParse(result,
                               sourceNameString,
                               lineNumber,
                               "add_sampler requires id= and name=");
      }

      SamplerDesc desc;
      desc.name = *name;
      if (!ParseRecipeBinding(assignments, desc.binding, parseError)) {
        return FailRecipeParse(result, sourceNameString, lineNumber, parseError);
      }

      result.recipe.AddStep(MakeAddSamplerStep(*id, desc));
  parsedSamplers[*id] = desc;
      requiresResourceRefresh = true;
      continue;
    }

    if (command == "add_cbuffer") {
      const std::string *id = FindRecipeAssignment(assignments, "id");
      const std::string *name = FindRecipeAssignment(assignments, "name");
      const std::string *typeName = FindRecipeAssignment(assignments, "type");
      const std::string *sizeText = FindRecipeAssignment(assignments, "size");
      if (id == nullptr || name == nullptr || typeName == nullptr ||
          sizeText == nullptr) {
        return FailRecipeParse(result,
                               sourceNameString,
                               lineNumber,
                               "add_cbuffer requires id=, name=, type=, and size=");
      }

      pendingCBuffer = std::make_unique<PendingCBufferBlock>();
      pendingCBuffer->id = *id;
      pendingCBuffer->desc.name = *name;
      pendingCBuffer->desc.binding.AsCBuffer();
      pendingCBuffer->schema.typeName = *typeName;
      if (!ParseRecipeUnsignedValue(*sizeText,
                                    pendingCBuffer->schema.sizeInBytes,
                                    parseError) ||
          !ParseRecipeBinding(assignments,
                              pendingCBuffer->desc.binding,
                              parseError)) {
        return FailRecipeParse(result, sourceNameString, lineNumber, parseError);
      }

      pendingCBuffer->desc.sizeInBytes = pendingCBuffer->schema.sizeInBytes;
      continue;
    }

    if (command == "expect_texture") {
      const std::string *id = FindRecipeAssignment(assignments, "id");
      if (id == nullptr) {
        return FailRecipeParse(result,
                               sourceNameString,
                               lineNumber,
                               "expect_texture requires id=");
      }
      result.recipe.AddStep(MakeExpectTextureStep(*id));
      continue;
    }

    if (command == "expect_texture_uav") {
      const std::string *id = FindRecipeAssignment(assignments, "id");
      if (id == nullptr) {
        return FailRecipeParse(result,
                               sourceNameString,
                               lineNumber,
                               "expect_texture_uav requires id=");
      }
      result.recipe.AddStep(MakeExpectTextureUAVStep(*id));
      continue;
    }

    if (command == "expect_cbuffer") {
      const std::string *id = FindRecipeAssignment(assignments, "id");
      if (id == nullptr) {
        return FailRecipeParse(result,
                               sourceNameString,
                               lineNumber,
                               "expect_cbuffer requires id=");
      }
      result.recipe.AddStep(MakeExpectCBufferStep(*id));
      continue;
    }

    if (command == "refresh_resources") {
      result.recipe.AddStep(MakeRefreshResourcesStep());
      continue;
    }

    if (command == "prune_dead_code") {
      result.recipe.AddStep(MakePruneDeadCodeStep());
      continue;
    }

    if (command == "verify_module") {
      result.recipe.AddStep(MakeVerifyModuleStep());
      continue;
    }

    return FailRecipeParse(result,
                           sourceNameString,
                           lineNumber,
                           "unknown command '" + command + "'");
  }

  if (pendingRewriteRule != nullptr) {
    result.error = sourceNameString +
                   ": recipe ended before closing rewrite_rule block with end";
    return false;
  }

  if (!hasExplicitRefreshOption)
    result.patchOptions.refreshResources = requiresResourceRefresh;

  if (pendingCBuffer != nullptr) {
    result.error = sourceNameString + ": recipe ended before closing add_cbuffer block with end";
    return false;
  }

  return true;
}

bool ParseDxilRecipeFile(const std::string &recipePath,
                         DxilRecipeParseResult &result) {
  std::ifstream file(recipePath);
  if (!file) {
    result = DxilRecipeParseResult();
    result.error = "failed to open recipe file '" + recipePath + "'";
    return false;
  }

  std::string recipeText((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
  return ParseDxilRecipeText(recipeText, result, recipePath);
}

// ============================================================
// Resource refresh
// ============================================================
void RefreshDxilAfterResourceMutation(hlsl::DxilModule &DM,
                                      bool traceEnabled) {
  TraceMessage(traceEnabled, "refresh: emit llvm.used");
  DM.EmitLLVMUsed();

  // Re-emit resource metadata using the same narrow path DXC passes use.
  TraceMessage(traceEnabled, "refresh: re-emit resources");
  DM.ReEmitDxilResources();

  // Recompute flags and validator expectations for the updated resource table.
  TraceMessage(traceEnabled, "refresh: collect shader flags");
  DM.CollectShaderFlagsForModule();
  TraceMessage(traceEnabled, "refresh: upgrade validator version");
  DM.UpgradeToMinValidatorVersion();
  TraceMessage(traceEnabled, "refresh: update validator metadata");
  DM.UpdateValidatorVersionMetadata();

  // llvm.used keeps newly injected globals alive during mutation, but it is
  // not needed once DXIL resource metadata has been rewritten and confuses
  // some downstream decompilers.
  TraceMessage(traceEnabled, "refresh: clear llvm.used");
  DM.ClearLLVMUsed();
}

// ============================================================
// Bitcode serialization
// ============================================================
std::vector<uint8_t> SerializeModuleToBitcode(Module &M) {
  std::string bitcodeBytes;
  llvm::raw_string_ostream os(bitcodeBytes);
  llvm::WriteBitcodeToFile(&M, os);
  os.flush();
  return std::vector<uint8_t>(bitcodeBytes.begin(), bitcodeBytes.end());
}

// ============================================================
// Memory stream helper
// ============================================================
static bool CreateMemoryStreamFromBytes(
    IMalloc *pMalloc,
    const std::vector<uint8_t> &bytes,
    CComPtr<hlsl::AbstractMemoryStream> &stream) {
  if (FAILED(hlsl::CreateMemoryStream(pMalloc, &stream)) || !stream) {
    std::cerr << "CreateMemoryStream failed.\n";
    return false;
  }

  if (!bytes.empty()) {
    ULONG written = 0;
    HRESULT hr = stream->Write(bytes.data(),
                               static_cast<ULONG>(bytes.size()),
                               &written);
    if (FAILED(hr) || written != bytes.size()) {
      std::cerr << "Failed to write bytes into memory stream.\n";
      return false;
    }
  }

  LARGE_INTEGER zero = {};
  if (FAILED(stream->Seek(zero, STREAM_SEEK_SET, nullptr))) {
    std::cerr << "Failed to rewind memory stream.\n";
    return false;
  }

  return true;
}

// ============================================================
// Final container serialization
// ============================================================
bool SerializePatchedContainer(hlsl::DxilModule &DM,
                               const std::vector<uint8_t> &moduleBitcode,
                               std::vector<uint8_t> &outputContainer) {
  CComPtr<IMalloc> pMalloc;
  if (FAILED(::CoGetMalloc(1, &pMalloc)) || !pMalloc) {
    std::cerr << "CoGetMalloc failed.\n";
    return false;
  }

  CComPtr<hlsl::AbstractMemoryStream> moduleBitcodeStream;
  if (!CreateMemoryStreamFromBytes(pMalloc, moduleBitcode, moduleBitcodeStream))
    return false;

  CComPtr<hlsl::AbstractMemoryStream> outputStream;
  if (FAILED(hlsl::CreateMemoryStream(pMalloc, &outputStream)) || !outputStream) {
    std::cerr << "Failed to create output stream.\n";
    return false;
  }

  hlsl::SerializeDxilFlags flags = hlsl::SerializeDxilFlags::None;

  hlsl::SerializeDxilContainerForModule(
      &DM,
      moduleBitcodeStream,
      /*DXCVersionInfo*/ nullptr,
      outputStream,
      /*DebugName*/ "",
      flags,
      /*ShaderHashOut*/ nullptr,
      /*ReflectionStreamOut*/ nullptr,
      /*RootSigStreamOut*/ nullptr,
      /*PrivateData*/ nullptr,
      /*PrivateDataSize*/ 0);

  if (outputStream->GetPtr() == nullptr || outputStream->GetPtrSize() == 0) {
    std::cerr << "DXIL container serialization produced no output.\n";
    return false;
  }

  outputContainer.assign(outputStream->GetPtr(),
                         outputStream->GetPtr() + outputStream->GetPtrSize());
  return true;
}

void RestoreOriginalResourceReflection(
    const std::vector<uint8_t> &inputBytes,
    hlsl::DxilModule &targetDxilModule,
    llvm::LLVMContext &reflectionContext) {
  DxilProgramBitcode reflectionBitcode;
  if (!ExtractProgramBitcodeFromContainerPart(
          inputBytes, hlsl::DFCC_ShaderStatistics, reflectionBitcode)) {
    return;
  }

  std::unique_ptr<Module> reflectionModule =
      ParseDxilBitcode(reflectionBitcode.ptr, reflectionBitcode.size,
                       reflectionContext);
  if (!reflectionModule)
    return;

  hlsl::DxilModule *reflectionDxilModule = nullptr;
  if (!LoadDxilState(*reflectionModule, reflectionDxilModule) ||
      reflectionDxilModule == nullptr) {
    return;
  }

  targetDxilModule.RestoreResourceReflection(*reflectionDxilModule);
}


