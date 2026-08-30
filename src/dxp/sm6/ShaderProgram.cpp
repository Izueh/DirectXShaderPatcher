#include "dxp/sm6/ShaderProgram.hpp"
#include "dxp/ExportTypes.hpp"
#include "dxp/sm6/EnumMirrors.hpp"
#include "dxp/sm6/ResourceTypes.hpp"
// clang-format off
// WinIncludes.h must precede dxcapi.h — dxcapi.h uses COM types but doesn't
// include its own Windows dependencies. DXC's own code does the same.
#include <dxc/Support/WinIncludes.h>
#include <dxc/dxcapi.h>
#include <dxc/Support/Global.h>
// clang-format on
#include <atlcomcli.h>
#include <combaseapi.h>
#include <dxc/DXIL/DxilCompType.h>
#include <dxc/DXIL/DxilConstants.h>
#include <dxc/DXIL/DxilResourceBase.h>
#include <dxc/DXIL/DxilResourceBinding.h>
#include <dxc/DXIL/DxilResourceProperties.h>
#include <intsafe.h>
#include <llvm/IR/Argument.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Use.h>
#include <llvm/IR/ValueHandle.h>
#include <llvm/Support/Casting.h>
#include <objidlbase.h>
#include <winnt.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <memory>
#include <ranges>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "dxc/DXIL/DxilCBuffer.h"
#include "dxc/DXIL/DxilInterpolationMode.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilResource.h"
#include "dxc/DXIL/DxilSampler.h"
#include "dxc/DXIL/DxilSemantic.h"
#include "dxc/DXIL/DxilSignatureElement.h"
#include "dxc/DXIL/DxilTypeSystem.h"
#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/DxilContainer/DxilContainerAssembler.h"
#include "dxc/DxilContainer/DxilContainerReader.h"
#include "dxc/DxilValidation/DxilValidation.h"
#include "dxc/Support/FileIOHelper.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/MSFileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Local.h"

namespace {

struct DxilProgramBitcode {
  const uint8_t* ptr = nullptr;
  uint32_t size = 0;
};

auto GetTextureElementTypeName(hlsl::DXIL::ComponentType kind) -> const char* {
  using hlsl::DXIL::ComponentType;
  switch (kind) {
    case ComponentType::F32:
      return "float";
    case ComponentType::U32:
      return "uint";
    case ComponentType::I32:
      return "int";
    default:
      return "invalid";
  }
}

auto GetTextureElementScalarType(llvm::LLVMContext& ctx, hlsl::DXIL::ComponentType kind) -> llvm::Type* {
  using hlsl::DXIL::ComponentType;
  switch (kind) {
    case ComponentType::F32:
      return llvm::Type::getFloatTy(ctx);
    case ComponentType::U32:
    case ComponentType::I32:
      return llvm::Type::getInt32Ty(ctx);
    default:
      return nullptr;
  }
}

auto GetTextureCompType(hlsl::DXIL::ComponentType kind) -> hlsl::CompType {
  return {kind};
}

auto ValidateTextureResourceDesc(const dxp::sm6::TextureResourceDesc& desc, std::string& error_message) -> bool {
  if (desc.name.empty()) {
    error_message = "Texture resource name must not be empty.";
    return false;
  }
  switch (static_cast<hlsl::DXIL::ResourceClass>(desc.binding.resource_class)) {
    case hlsl::DXIL::ResourceClass::SRV:
      if (desc.is_read_write) {
        error_message = "Texture SRV descriptors must not be marked read-write.";
        return false;
      }
      break;
    case hlsl::DXIL::ResourceClass::UAV:
      if (!desc.is_read_write) {
        error_message = "Texture UAV descriptors must be marked read-write.";
        return false;
      }
      break;
    default:
      error_message = "Texture resources currently support SRV and UAV binding classes only.";
      return false;
  }
  if (desc.vector_width < 1 || desc.vector_width > 4) {
    error_message = "Texture resource vector width must be between 1 and 4.";
    return false;
  }
  switch (static_cast<hlsl::DXIL::ResourceKind>(static_cast<unsigned>(desc.kind))) {
    case hlsl::DXIL::ResourceKind::Texture1D:
    case hlsl::DXIL::ResourceKind::Texture2D:
    case hlsl::DXIL::ResourceKind::Texture2DMS:
    case hlsl::DXIL::ResourceKind::Texture3D:
    case hlsl::DXIL::ResourceKind::TextureCube:
    case hlsl::DXIL::ResourceKind::Texture1DArray:
    case hlsl::DXIL::ResourceKind::Texture2DArray:
    case hlsl::DXIL::ResourceKind::Texture2DMSArray:
    case hlsl::DXIL::ResourceKind::TextureCubeArray:
    case hlsl::DXIL::ResourceKind::TypedBuffer:
    case hlsl::DXIL::ResourceKind::RawBuffer:
    case hlsl::DXIL::ResourceKind::StructuredBuffer:
      break;
    default:
      error_message = "unsupported resource kind (only texture and buffer SRV/UAV kinds are supported).";
      return false;
  }
  switch (static_cast<hlsl::DXIL::ComponentType>(static_cast<uint8_t>(desc.element_kind))) {
    case hlsl::DXIL::ComponentType::F32:
    case hlsl::DXIL::ComponentType::U32:
    case hlsl::DXIL::ComponentType::I32:
      break;
    default:
      error_message = "only F32, U32, and I32 element types are supported.";
      return false;
  }
  return true;
}

auto GetTextureElementTypeDisplayName(const dxp::sm6::TextureResourceDesc& desc) -> std::string {
  std::string kScalarName = GetTextureElementTypeName(static_cast<hlsl::DXIL::ComponentType>(static_cast<uint8_t>(desc.element_kind)));
  if (desc.vector_width == 1) {
    return kScalarName;
  }
  std::string result = "vector<";
  result += kScalarName;
  result += ", ";
  result += std::to_string(desc.vector_width);
  result += ">";
  return result;
}

auto GetTextureTypeName(const dxp::sm6::TextureResourceDesc& desc) -> std::string {
  using hlsl::DXIL::ResourceKind;
  const std::string el_type_name = GetTextureElementTypeDisplayName(desc);
  const bool rw = desc.is_read_write;
  const std::string& el = el_type_name;
  switch (static_cast<ResourceKind>(static_cast<unsigned>(desc.kind))) {
    case ResourceKind::Texture1D:
      return rw ? "class.RWTexture1D<" + el + " >" : "class.Texture1D<" + el + " >";
    case ResourceKind::Texture2D:
      return rw ? "class.RWTexture2D<" + el + " >" : "class.Texture2D<" + el + " >";
    case ResourceKind::Texture2DMS:
      return rw ? "class.RWTexture2DMS<" + el + " >" : "class.Texture2DMS<" + el + " >";
    case ResourceKind::Texture3D:
      return rw ? "class.RWTexture3D<" + el + " >" : "class.Texture3D<" + el + " >";
    case ResourceKind::TextureCube:
      return "class.TextureCube<" + el + " >";
    case ResourceKind::Texture1DArray:
      return rw ? "class.RWTexture1DArray<" + el + " >" : "class.Texture1DArray<" + el + " >";
    case ResourceKind::Texture2DArray:
      return rw ? "class.RWTexture2DArray<" + el + " >" : "class.Texture2DArray<" + el + " >";
    case ResourceKind::Texture2DMSArray:
      return rw ? "class.RWTexture2DMSArray<" + el + " >" : "class.Texture2DMSArray<" + el + " >";
    case ResourceKind::TextureCubeArray:
      return "class.TextureCubeArray<" + el + " >";
    case ResourceKind::TypedBuffer:
      return rw ? "class.RWBuffer<" + el + " >" : "class.Buffer<" + el + " >";
    case ResourceKind::RawBuffer:
      return rw ? "class.RWByteAddressBuffer" : "class.ByteAddressBuffer";
    case ResourceKind::StructuredBuffer:
      return rw ? "class.RWStructuredBuffer<" + el + " >" : "class.StructuredBuffer<" + el + " >";
    default:
      return "invalid";
  }
}

auto GetResourceElementStride(const dxp::sm6::TextureResourceDesc& desc) -> unsigned {
  switch (static_cast<hlsl::DXIL::ComponentType>(static_cast<uint8_t>(desc.element_kind))) {
    case hlsl::DXIL::ComponentType::F32:
    case hlsl::DXIL::ComponentType::U32:
    case hlsl::DXIL::ComponentType::I32:
      break;
    default:
      return 4;
  }
  return 4 * desc.vector_width;
}

auto GetTextureMipsTypeName(const dxp::sm6::TextureResourceDesc& desc) -> std::string {
  if (desc.is_read_write) {
    return {};
  }
  return GetTextureTypeName(desc) + "::mips_type";
}

auto GetTextureElementType(llvm::LLVMContext& ctx, const dxp::sm6::TextureResourceDesc& desc) -> llvm::Type* {
  if (static_cast<hlsl::DXIL::ResourceKind>(static_cast<unsigned>(desc.kind)) == hlsl::DXIL::ResourceKind::RawBuffer) {
    // ByteAddressBuffer's symbol type carries the raw byte payload.
    return llvm::ArrayType::get(llvm::Type::getInt8Ty(ctx), 4);
  }
  auto* scalar_type = GetTextureElementScalarType(ctx, static_cast<hlsl::DXIL::ComponentType>(static_cast<uint8_t>(desc.element_kind)));
  if (scalar_type == nullptr) {
    return nullptr;
  }
  if (desc.vector_width == 1) {
    return scalar_type;
  }
  return llvm::VectorType::get(scalar_type, desc.vector_width);
}

auto GetCBufferFieldVectorSize(const dxp::sm6::CBufferFieldDesc& field) -> unsigned {
  return field.vector_size;
}

auto GetCBufferFieldCompKind(const dxp::sm6::CBufferFieldDesc& field) -> hlsl::CompType::Kind {
  return static_cast<hlsl::CompType::Kind>(static_cast<uint8_t>(field.comp_type));
}

auto GetCBufferFieldScalarSize(hlsl::CompType::Kind comp_type) -> unsigned {
  switch (comp_type) {
    case hlsl::DXIL::ComponentType::F32:
    case hlsl::DXIL::ComponentType::U32:
    case hlsl::DXIL::ComponentType::I32:
      return 4;
    default:
      return 0;
  }
}

auto GetCBufferFieldSize(const dxp::sm6::CBufferFieldDesc& field) -> unsigned {
  return GetCBufferFieldScalarSize(static_cast<hlsl::CompType::Kind>(static_cast<uint8_t>(field.comp_type))) * GetCBufferFieldVectorSize(field);
}

auto GetCBufferFieldType(llvm::LLVMContext& ctx, const dxp::sm6::CBufferFieldDesc& field) -> llvm::Type* {
  auto comp = static_cast<hlsl::DXIL::ComponentType>(static_cast<uint8_t>(field.comp_type));
  llvm::Type* scalar_type = nullptr;
  switch (comp) {
    case hlsl::DXIL::ComponentType::F32:
      scalar_type = llvm::Type::getFloatTy(ctx);
      break;
    case hlsl::DXIL::ComponentType::U32:
    case hlsl::DXIL::ComponentType::I32:
      scalar_type = llvm::Type::getInt32Ty(ctx);
      break;
    default:
      return nullptr;
  }
  const unsigned vec_size = GetCBufferFieldVectorSize(field);
  return vec_size == 1 ? scalar_type : llvm::VectorType::get(scalar_type, vec_size);
}

auto ValidateCBufferSchema(const dxp::sm6::CBufferSchema& schema, std::string& error_message) -> bool {
  if (schema.type_name.empty()) {
    error_message = "CBuffer schema type name must not be empty.";
    return false;
  }
  if (schema.size_in_bytes == 0) {
    error_message = "CBuffer schema size must be non-zero.";
    return false;
  }
  unsigned prev_offset = 0;
  bool have_prev = false;
  for (const auto& field : schema.fields) {
    if (field.name.empty()) {
      error_message = "CBuffer schema field name must not be empty.";
      return false;
    }
    switch (static_cast<hlsl::DXIL::ComponentType>(static_cast<uint8_t>(field.comp_type))) {
      case hlsl::DXIL::ComponentType::F32:
      case hlsl::DXIL::ComponentType::U32:
      case hlsl::DXIL::ComponentType::I32:
        break;
      default:
        error_message = "CBuffer schema currently supports F32, U32, and I32 field types only.";
        return false;
    }
    if (field.vector_size < 1 || field.vector_size > 4) {
      error_message = "CBuffer schema field vector size must be between 1 and 4.";
      return false;
    }
    const unsigned field_size = GetCBufferFieldSize(field);
    if (field.offset + field_size > schema.size_in_bytes) {
      error_message = "CBuffer schema field exceeds declared struct size: " + field.name;
      return false;
    }
    if (have_prev && field.offset < prev_offset) {
      error_message = "CBuffer schema fields must be declared in ascending offset order.";
      return false;
    }
    prev_offset = field.offset;
    have_prev = true;
  }
  return true;
}

void BuildCBufferSchemaLayout(const dxp::sm6::CBufferSchema& schema, llvm::LLVMContext& ctx,
                              std::vector<llvm::Type*>& element_types, std::vector<int>& field_mapping) {
  element_types.clear();
  field_mapping.clear();
  unsigned cur = 0;
  // Padding is emitted as individual i32 scalars � one field_mapping entry each.
  // Scalars only need 4-byte alignment, whereas arrays in cbuffers must start at
  // 16-byte boundaries (Sm.CBufferArrayOffsetAlignment).
  const auto push_padding = [&](unsigned padding_size) {
    for (unsigned p = 0; p < padding_size / 4; ++p) {
      element_types.push_back(llvm::Type::getInt32Ty(ctx));
      field_mapping.push_back(-1);
    }
    for (unsigned p = 0; p < padding_size % 4; ++p) {
      element_types.push_back(llvm::Type::getInt8Ty(ctx));
      field_mapping.push_back(-1);
    }
  };
  for (size_t fi = 0; fi < schema.fields.size(); ++fi) {
    const auto& field = schema.fields[fi];
    if (field.offset > cur) {
      push_padding(field.offset - cur);
      cur = field.offset;
    }
    element_types.push_back(GetCBufferFieldType(ctx, field));
    field_mapping.push_back(static_cast<int>(fi));
    cur += GetCBufferFieldSize(field);
  }
  if (schema.size_in_bytes > cur) {
    push_padding(schema.size_in_bytes - cur);
  }
}

auto CanUseUnpackedCBufferLayout(const llvm::DataLayout& data_layout, const dxp::sm6::CBufferSchema& schema,
                                 const std::vector<llvm::Type*>& types, const std::vector<int>& mapping) -> bool {
  if (types.empty()) {
    return false;
  }
  auto* probe = llvm::StructType::get(types.front()->getContext(), types, false);
  const auto* struct_layout = data_layout.getStructLayout(probe);
  if (struct_layout->getSizeInBytes() != schema.size_in_bytes) {
    return false;
  }
  for (size_t ei = 0; ei < mapping.size(); ++ei) {
    if (mapping[ei] < 0) {
      continue;
    }
    if (struct_layout->getElementOffset(ei) != schema.fields[static_cast<size_t>(mapping[ei])].offset) {
      return false;
    }
  }
  return true;
}

template <typename T>
auto HasRegisterIndexConflict(const std::vector<std::unique_ptr<T>>& resources, unsigned space, unsigned register_index) -> bool {
  return std::ranges::any_of(resources, [space, register_index](const auto& resource) {
    return resource->GetSpaceID() == space && resource->GetLowerBound() == register_index;
  });
}

auto HasGlobalNameConflict(const llvm::Module& module, const std::string& name) -> bool {
  return module.getNamedValue(name) != nullptr;
}

void KeepGlobalAlive(hlsl::DxilModule& dxil_module, llvm::GlobalVariable* global_var) {
  auto& used = dxil_module.GetLLVMUsed();
  if (std::ranges::find(used, global_var) == used.end()) {
    used.push_back(global_var);
  }
}

auto GetOrCreateCBufferType(llvm::Module& mod, const dxp::sm6::CBufferDesc& desc) -> llvm::StructType* {
  auto& ctx = mod.getContext();
  auto* struct_type = mod.getTypeByName(desc.name + "_t");
  if (struct_type == nullptr) {
    struct_type = llvm::StructType::create(ctx, desc.name + "_t");
  }
  if (struct_type->isOpaque()) {
    const unsigned dw_count = std::max(1U, (desc.size_in_bytes + 3U) / 4U);
    auto* arr = llvm::ArrayType::get(llvm::Type::getInt32Ty(ctx), dw_count);
    llvm::Type* const kEls[] = {arr};
    struct_type->setBody(kEls, false);
  }
  return struct_type;
}

auto GetOrCreateCBufferSchemaType(llvm::Module& mod, const dxp::sm6::CBufferSchema& schema) -> llvm::StructType* {
  auto& ctx = mod.getContext();
  auto* struct_type = mod.getTypeByName(schema.type_name);
  if (struct_type == nullptr) {
    struct_type = llvm::StructType::create(ctx, schema.type_name);
  }
  if (struct_type->isOpaque()) {
    std::vector<llvm::Type*> element_types;
    std::vector<int> field_mapping;
    BuildCBufferSchemaLayout(schema, ctx, element_types, field_mapping);
    const bool packed = !CanUseUnpackedCBufferLayout(mod.getDataLayout(), schema, element_types, field_mapping);
    struct_type->setBody(element_types, packed);
  }
  return struct_type;
}

auto GetOrCreateMarkerStructType(llvm::Module& mod, const std::string& name) -> llvm::StructType* {
  auto& ctx = mod.getContext();
  auto* struct_type = mod.getTypeByName(name);
  if (struct_type == nullptr) {
    struct_type = llvm::StructType::create(ctx, name);
  }
  if (struct_type->isOpaque()) {
    llvm::Type* const kEls[] = {llvm::Type::getInt32Ty(ctx)};
    struct_type->setBody(kEls, false);
  }
  return struct_type;
}

auto GetOrCreateTextureType(llvm::Module& mod, const dxp::sm6::TextureResourceDesc& tex_desc) -> llvm::StructType* {
  auto& ctx = mod.getContext();
  const std::string type_name = GetTextureTypeName(tex_desc);
  auto* elem_type = GetTextureElementType(ctx, tex_desc);
  if (elem_type == nullptr) {
    return nullptr;
  }
  auto* struct_type = mod.getTypeByName(type_name);
  if (struct_type == nullptr) {
    struct_type = llvm::StructType::create(ctx, type_name);
  }
  if (struct_type->isOpaque()) {
    if (tex_desc.is_read_write) {
      llvm::Type* const kEls[] = {elem_type};
      struct_type->setBody(kEls, false);
    } else {
      const std::string mips_name = GetTextureMipsTypeName(tex_desc);
      auto* mips_type = mod.getTypeByName(mips_name);
      if (mips_type == nullptr) {
        mips_type = llvm::StructType::create(ctx, mips_name);
      }
      if (mips_type->isOpaque()) {
        llvm::Type* const kMels[] = {llvm::Type::getInt32Ty(ctx)};
        mips_type->setBody(kMels, false);
      }
      llvm::Type* const kEls[] = {elem_type, mips_type};
      struct_type->setBody(kEls, false);
    }
  }
  return struct_type;
}

auto GetOrCreateResolvedCBufferType(llvm::Module& mod, const dxp::sm6::CBufferDesc& cb_desc) -> llvm::StructType* {
  return (cb_desc.schema != nullptr) ? GetOrCreateCBufferSchemaType(mod, *cb_desc.schema) : GetOrCreateCBufferType(mod, cb_desc);
}

/// @brief Generate a unique global name by appending numeric suffixes.
std::string MakeUniqueGlobalName(llvm::Module& mod, const std::string& baseName) {
  if (!HasGlobalNameConflict(mod, baseName)) {
    return baseName;
  }
  for (unsigned suffix = 1; suffix != 0; ++suffix) {
    const std::string candidate = baseName + std::to_string(suffix);
    if (!HasGlobalNameConflict(mod, candidate)) {
      return candidate;
    }
  }
  return baseName;
}

/// @brief Create a named GlobalVariable for a cbuffer resource.
llvm::GlobalVariable* CreateCBufferSymbol(llvm::Module& mod, dxp::sm6::CBufferDesc& desc, llvm::StructType*& out_type) {
  desc.name = MakeUniqueGlobalName(mod, desc.name);
  out_type = GetOrCreateResolvedCBufferType(mod, desc);
  return new llvm::GlobalVariable(mod, out_type, false, llvm::GlobalValue::ExternalLinkage, nullptr, desc.name);
}

/// @brief Create a named GlobalVariable for a texture resource.
llvm::GlobalVariable* CreateTextureSymbol(llvm::Module& mod, dxp::sm6::TextureResourceDesc& desc) {
  desc.name = MakeUniqueGlobalName(mod, desc.name);
  auto* struct_type = GetOrCreateTextureType(mod, desc);
  if (struct_type == nullptr) return nullptr;
  return new llvm::GlobalVariable(mod, struct_type, false, llvm::GlobalValue::ExternalLinkage, nullptr, desc.name);
}

/// @brief Create or retrieve a named GlobalVariable for a sampler resource.
llvm::GlobalVariable* EnsureSamplerGlobal(llvm::Module& mod, dxp::sm6::SamplerDesc& desc) {
  desc.name = MakeUniqueGlobalName(mod, desc.name);
  auto* struct_type = GetOrCreateMarkerStructType(mod, desc.name + "_Sampler_t");
  auto* global_var = mod.getGlobalVariable(desc.name);
  if (global_var == nullptr) {
    global_var = new llvm::GlobalVariable(mod, struct_type, false, llvm::GlobalValue::ExternalLinkage, nullptr, desc.name);
  }
  return global_var;
}

void MaybeAnnotateCBufferType(hlsl::DxilModule& dxm, llvm::StructType* struct_type, unsigned size_in_bytes) {
  if ((struct_type == nullptr) || struct_type->getNumElements() != 1) {
    return;
  }
  auto& type_sys = dxm.GetTypeSystem();
  auto* struct_annot = type_sys.GetStructAnnotation(struct_type);
  if (struct_annot == nullptr) {
    struct_annot = type_sys.AddStructAnnotation(struct_type);
  }
  struct_annot->SetCBufferSize(size_in_bytes);
  auto& field_annot = struct_annot->GetFieldAnnotation(0);
  field_annot.SetFieldName("Data");
  field_annot.SetCBufferOffset(0);
  field_annot.SetCompType(hlsl::CompType::getU32().GetKind());
  field_annot.SetCBVarUsed(true);
  type_sys.FinishStructAnnotation(*struct_annot);
}

void MaybeAnnotateCBufferType(hlsl::DxilModule& dxm, llvm::StructType* struct_type, const dxp::sm6::CBufferSchema& schema) {
  if (struct_type == nullptr) {
    return;
  }
  std::vector<llvm::Type*> element_types;
  std::vector<int> field_mapping;
  BuildCBufferSchemaLayout(schema, struct_type->getContext(), element_types, field_mapping);
  auto& type_sys = dxm.GetTypeSystem();
  auto* struct_annot = type_sys.GetStructAnnotation(struct_type);
  if (struct_annot == nullptr) {
    struct_annot = type_sys.AddStructAnnotation(struct_type);
  }
  struct_annot->SetCBufferSize(schema.size_in_bytes);
  // Track running offsets so padding elements (fields with mapping -1) still get
  // a valid offset/comp type � the DXIL validator rejects uninitialized ones.
  unsigned cur_offset = 0;
  for (size_t fi = 0; fi < field_mapping.size(); ++fi) {
    auto& field_annot = struct_annot->GetFieldAnnotation(static_cast<unsigned>(fi));
    const int smi = field_mapping[fi];
    if (smi < 0) {
      field_annot.SetFieldName("<padding>");
      field_annot.SetCBufferOffset(cur_offset);
      field_annot.SetCompType(hlsl::CompType::Kind::U32);
      field_annot.SetCBVarUsed(false);
      cur_offset += static_cast<unsigned>(element_types[fi]->getPrimitiveSizeInBits() / 8);
      continue;
    }
    const auto& field_desc = schema.fields[static_cast<size_t>(smi)];
    field_annot.SetFieldName(field_desc.name);
    field_annot.SetCBufferOffset(field_desc.offset);
    field_annot.SetCompType(GetCBufferFieldCompKind(field_desc));
    field_annot.SetCBVarUsed(true);
    const unsigned vec_size = GetCBufferFieldVectorSize(field_desc);
    cur_offset = field_desc.offset + GetCBufferFieldSize(field_desc);
    if (vec_size > 1) {
      field_annot.SetVectorSize(vec_size);
    }
  }
  type_sys.FinishStructAnnotation(*struct_annot);
}

auto FourCCToString(uint32_t four_cc) -> std::string {
  constexpr uint32_t kByteMask = 0xffU;
  constexpr uint32_t kByteShift1 = 8;
  constexpr uint32_t kByteShift2 = 16;
  constexpr uint32_t kByteShift3 = 24;
  std::string text(4, '\0');
  text[0] = static_cast<char>(four_cc & kByteMask);
  text[1] = static_cast<char>((four_cc >> kByteShift1) & kByteMask);
  text[2] = static_cast<char>((four_cc >> kByteShift2) & kByteMask);
  text[3] = static_cast<char>((four_cc >> kByteShift3) & kByteMask);
  for (char& chr : text) {
    if (std::isprint(static_cast<unsigned char>(chr)) == 0) {
      chr = '?';
    }
  }
  return text;
}

auto DigestToHex(std::span<const uint8_t> digest) -> std::string {
  static constexpr std::array<char, 16> kHexDigits = {'0', '1', '2', '3', '4', '5', '6', '7',
                                                      '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
  std::string hex;
  hex.reserve(digest.size() * 2);
  for (const uint8_t kByte : digest) {
    hex.push_back(kHexDigits.at((kByte >> 4U) & 0xfU));
    hex.push_back(kHexDigits.at(kByte & 0xfU));
  }
  return hex;
}

}  // anonymous namespace

/// @brief Per-thread LLVMContext reused across ShaderProgram loads on the same
/// thread. An llvm::LLVMContext is a per-thread state container and is not
/// thread-safe — it must be confined to one thread — and DXC's own compilation
/// model reuses one context per thread.
///
/// Modules parsed into this context are owned by their ShaderProgram and are
/// destroyed at the end of each recipe Execute call, so the context (destroyed
/// lazily at thread exit) always outlives its modules.
llvm::LLVMContext& ThreadLocalContext() {
  thread_local llvm::LLVMContext context;
  return context;
}

namespace dxp::sm6 {

const std::string& DxcRuntime::Ensure() {
  // The DXC-built LLVM routes all fd I/O (raw_fd_ostream, MemoryBuffer, the
  // bitcode writer) through a per-thread MSFileSystem. Without one installed,
  // writes silently fail with EBADF. The thread malloc likewise must stay
  // installed for the thread's lifetime: DXC-owned allocations created under
  // it are freed under it, so per-operation setup/teardown would corrupt the
  // heap across allocator boundaries.
  static thread_local std::string setup_error = []() -> std::string {
    if (llvm::sys::fs::SetupPerThreadFileSystem()) {
      return "failed to set up per-thread file system";
    }
    llvm::sys::fs::MSFileSystem* msf_ptr = nullptr;
    if (FAILED(CreateMSFileSystemForDisk(&msf_ptr)) || msf_ptr == nullptr) {
      llvm::sys::fs::CleanupPerThreadFileSystem();
      return "failed to create disk file system";
    }
    if (llvm::sys::fs::SetCurrentThreadFileSystem(msf_ptr)) {
      delete msf_ptr;
      llvm::sys::fs::CleanupPerThreadFileSystem();
      return "failed to install per-thread file system";
    }
    if (FAILED(DxcInitThreadMalloc())) {
      return "failed to initialize DXC thread allocator";
    }
    DxcSetThreadMallocToDefault();
    return {};
  }();
  return setup_error;
}

ShaderProgram::~ShaderProgram() {
  if (module && module->HasDxilModule()) {
    module->pfnRemoveGlobal = nullptr;
    module->pfnResetDxilModule = nullptr;
    module->SetDxilModule(nullptr);
  }
  dxil_module = nullptr;
  module.reset();
}

ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept
    : input_bytes(std::move(other.input_bytes)),
      module(std::move(other.module)),
      dxil_module(other.dxil_module) {
  other.dxil_module = nullptr;
}

auto ShaderProgram::operator=(ShaderProgram&& other) noexcept -> ShaderProgram& {
  if (this != &other) {
    input_bytes = std::move(other.input_bytes);
    module = std::move(other.module);
    dxil_module = other.dxil_module;
    other.dxil_module = nullptr;
  }
  return *this;
}

auto ShaderProgram::ExtractBitcodeFromContainer(hlsl::DxilFourCC part_kind, DxilProgramBitcode& out) const -> bool {
  hlsl::DxilContainerReader reader;
  if (DXC_FAILED(reader.Load(input_bytes.data(), static_cast<uint32_t>(input_bytes.size())))) {
    return false;
  }

  uint32_t part_index = hlsl::DXIL_CONTAINER_BLOB_NOT_FOUND;
  if (DXC_FAILED(reader.FindFirstPartKind(part_kind, &part_index)) || part_index == hlsl::DXIL_CONTAINER_BLOB_NOT_FOUND) {
    return false;
  }

  const void* part_data = nullptr;
  uint32_t part_size = 0;
  if (DXC_FAILED(reader.GetPartContent(part_index, &part_data, &part_size)) || (part_data == nullptr) || part_size < sizeof(hlsl::DxilProgramHeader)) {
    return false;
  }

  const auto* prog_hdr = reinterpret_cast<const hlsl::DxilProgramHeader*>(part_data);
  if (!hlsl::IsValidDxilProgramHeader(prog_hdr, part_size)) {
    return false;
  }

  out.ptr = reinterpret_cast<const uint8_t*>(hlsl::GetDxilBitcodeData(prog_hdr));
  out.size = hlsl::GetDxilBitcodeSize(prog_hdr);
  return true;
}

auto ShaderProgram::ExtractDxilBitcode(DxilProgramBitcode& out) const -> bool {
  return ExtractBitcodeFromContainer(hlsl::DFCC_DXIL, out);
}

auto ShaderProgram::ParseBitcode(const DxilProgramBitcode& bitcode, llvm::LLVMContext& parse_context)
    -> std::expected<void, std::string> {
  // Create a MemoryBuffer like DXC's ValidateLoadModule does
  auto pBitcodeBuf = llvm::MemoryBuffer::getMemBuffer(
      llvm::StringRef(reinterpret_cast<const char*>(bitcode.ptr), bitcode.size), "", false);

  // Use TrackBitstream=true like DXC does — this is required for proper
  // DxilModule initialization and metadata loading
  auto mod_or_err = llvm::parseBitcodeFile(
      pBitcodeBuf.get()->getMemBufferRef(), parse_context, nullptr, true /*TrackBitstream*/);
  if (!mod_or_err) {
    return std::unexpected("failed to parse DXIL bitcode: " + mod_or_err.getError().message());
  }
  module = std::move(mod_or_err.get());
  return {};
}

auto ShaderProgram::LoadState() -> bool {
  if (!module) {
    return false;
  }
  hlsl::DxilModule& dxm = module->GetOrCreateDxilModule();
  dxil_module = &dxm;
  if (dxm.HasMetadataErrors()) {
    warnings.emplace_back("DXIL metadata load reported non-fatal errors");
  }
  return true;
}

void ShaderProgram::RestoreReflection() {
  DxilProgramBitcode ref_bitcode;
  if (!ExtractBitcodeFromContainer(hlsl::DFCC_ShaderStatistics, ref_bitcode)) {
    return;
  }

  // The reflection module is parsed into the module's own context (which is the
  // thread-local context for the default path, or the caller-provided external
  // context) and released at the end of this function — RestoreResourceReflection
  // deep-copies the reflection data, so the temporary module is not retained.
  auto ref_mod = llvm::parseBitcodeFile(
      llvm::MemoryBufferRef(llvm::StringRef(reinterpret_cast<const char*>(ref_bitcode.ptr), ref_bitcode.size),
                            "dxil-reflection"),
      module->getContext());
  if (!ref_mod) {
    return;
  }

  hlsl::DxilModule* ref_dxm = nullptr;
  hlsl::DxilModule& rdm = (*ref_mod)->GetOrCreateDxilModule();
  ref_dxm = &rdm;
  if (ref_dxm == nullptr) {
    return;
  }

  dxil_module->RestoreResourceReflection(*ref_dxm);
}

auto ShaderProgram::FromBytes(std::span<const uint8_t> bytes, ShaderProgram& out, bool restore_reflection)
    -> std::expected<void, std::string> {
  return FromBytes(bytes, out, ThreadLocalContext(), restore_reflection);
}

auto ShaderProgram::FromBytes(std::span<const uint8_t> bytes, ShaderProgram& out, llvm::LLVMContext& external_context,
                              bool restore_reflection) -> std::expected<void, std::string> {
  out = ShaderProgram{};
  out.input_bytes.assign(bytes.begin(), bytes.end());

  DxilProgramBitcode dxil_bitcode;
  if (!out.ExtractBitcodeFromContainer(hlsl::DFCC_DXIL, dxil_bitcode)) {
    return std::unexpected("failed to extract DXIL program bitcode");
  }

  if (auto parse_result = out.ParseBitcode(dxil_bitcode, external_context); !parse_result) {
    return parse_result;
  }

  if (!out.LoadState() || out.dxil_module == nullptr) {
    return std::unexpected("failed to load DxilModule state");
  }

  if (restore_reflection) {
    out.RestoreReflection();
  }

  return {};
}

auto ShaderProgram::Reload() -> std::expected<void, std::string> {
  DxilProgramBitcode dxil_bitcode;
  if (!ExtractBitcodeFromContainer(hlsl::DFCC_DXIL, dxil_bitcode)) {
    return std::unexpected("failed to extract DXIL bitcode from stored container");
  }
  // Re-parse into the context the module was originally created in (thread-local
  // for the default path, or the caller-provided external context).
  llvm::LLVMContext& parse_context = module ? module->getContext() : ThreadLocalContext();
  if (auto parse_result = ParseBitcode(dxil_bitcode, parse_context); !parse_result) {
    return parse_result;
  }
  if (!LoadState() || dxil_module == nullptr) {
    return std::unexpected("failed to load DxilModule state");
  }
  return {};
}

auto ShaderProgram::AddCBuffer(const CBufferDesc& desc) -> std::expected<void, std::string> {
  if (!module || (dxil_module == nullptr)) {
    return std::unexpected("add_resource: cbuffer: no loaded shader program");
  }
  const unsigned kSizeInBytes = (desc.schema != nullptr) ? desc.schema->size_in_bytes : desc.size_in_bytes;
  if (desc.name.empty() || kSizeInBytes == 0) {
    return std::unexpected("add_resource: refusing to add cbuffer with empty name or zero size");
  }
  if (desc.schema != nullptr) {
    std::string schema_error;
    if (!ValidateCBufferSchema(*desc.schema, schema_error)) {
      return std::unexpected("add_resource: cbuffer schema: " + schema_error);
    }
    if (desc.size_in_bytes != 0 && desc.size_in_bytes != desc.schema->size_in_bytes) {
      return std::unexpected("add_resource: cbuffer schema size does not match buffer size");
    }
  }

  CBufferDesc resolved = desc;
  const unsigned cbuf_space = resolved.binding.space.value_or(0);
  unsigned cbuf_reg = resolved.binding.register_index.value_or(0);
  if (!resolved.binding.register_index.has_value()) {
    cbuf_reg = FindNextAvailable(dxil_module->GetCBuffers(), cbuf_space, 0);
  }

  if (HasRegisterIndexConflict(dxil_module->GetCBuffers(), cbuf_space, cbuf_reg)) {
    return std::unexpected("add_resource: cbuffer binding already exists: space=" + std::to_string(cbuf_space) + " register=" + std::to_string(cbuf_reg));
  }

  llvm::StructType* cbuffer_type = nullptr;
  auto* sym = CreateCBufferSymbol(*module, resolved, cbuffer_type);
  if (resolved.schema != nullptr) {
    MaybeAnnotateCBufferType(*dxil_module, cbuffer_type, *resolved.schema);
  } else {
    MaybeAnnotateCBufferType(*dxil_module, cbuffer_type, kSizeInBytes);
  }

  auto cbuffer = std::make_unique<hlsl::DxilCBuffer>();
  cbuffer->SetGlobalSymbol(sym);
  cbuffer->SetGlobalName(resolved.name);
  cbuffer->SetHLSLType(sym->getType());
  cbuffer->SetSpaceID(cbuf_space);
  cbuffer->SetLowerBound(cbuf_reg);
  cbuffer->SetRangeSize(1);
  cbuffer->SetID(static_cast<unsigned>(dxil_module->GetCBuffers().size()));
  cbuffer->SetSize(kSizeInBytes);
  dxil_module->AddCBuffer(std::move(cbuffer));
  return {};
}

auto ShaderProgram::AddTextureSRV(const TextureResourceDesc& desc) -> std::expected<void, std::string> {
  if (!module || (dxil_module == nullptr)) {
    return std::unexpected("add_resource: texture: no loaded shader program");
  }
  if (desc.name.empty()) {
    return std::unexpected("add_resource: refusing to add texture SRV with empty name");
  }
  std::string validation_error;
  if (!ValidateTextureResourceDesc(desc, validation_error)) {
    return std::unexpected("add_resource: texture validation: " + validation_error);
  }

  TextureResourceDesc resolved = desc;
  const unsigned srv_space = resolved.binding.space.value_or(0);
  unsigned srv_reg = resolved.binding.register_index.value_or(0);
  if (!resolved.binding.register_index.has_value()) {
    srv_reg = FindNextAvailable(dxil_module->GetSRVs(), srv_space, 0);
  }

  if (HasRegisterIndexConflict(dxil_module->GetSRVs(), srv_space, srv_reg)) {
    return std::unexpected("add_resource: texture SRV binding already exists: space=" + std::to_string(srv_space) + " register=" + std::to_string(srv_reg));
  }

  auto* sym = CreateTextureSymbol(*module, resolved);
  if (sym == nullptr) {
    return std::unexpected("add_resource: failed to create texture symbol for " + resolved.name);
  }

  auto tex = std::make_unique<hlsl::DxilResource>();
  tex->SetGlobalSymbol(sym);
  tex->SetGlobalName(resolved.name);
  tex->SetHLSLType(sym->getType());
  tex->SetSpaceID(srv_space);
  tex->SetLowerBound(srv_reg);
  tex->SetRangeSize(1);
  tex->SetID(static_cast<unsigned>(dxil_module->GetSRVs().size()));
  tex->SetKind(static_cast<hlsl::DXIL::ResourceKind>(static_cast<unsigned>(resolved.kind)));
  if (resolved.kind == ResourceKind::StructuredBuffer) {
    tex->SetElementStride(GetResourceElementStride(resolved));
  }
  tex->SetCompType(GetTextureCompType(static_cast<hlsl::DXIL::ComponentType>(static_cast<uint8_t>(resolved.element_kind))));
  tex->SetSampleCount(0);
  tex->SetRW(false);
  dxil_module->AddSRV(std::move(tex));
  return {};
}

auto ShaderProgram::AddTexture2DSRV(const TextureResourceDesc& desc) -> std::expected<void, std::string> {
  return AddTextureSRV(desc);
}

auto ShaderProgram::AddTextureUAV(const TextureResourceDesc& desc) -> std::expected<void, std::string> {
  if (!module || (dxil_module == nullptr)) {
    return std::unexpected("add_resource: uav: no loaded shader program");
  }
  if (desc.name.empty()) {
    return std::unexpected("add_resource: refusing to add texture UAV with empty name");
  }
  std::string validation_error;
  if (!ValidateTextureResourceDesc(desc, validation_error)) {
    return std::unexpected("add_resource: texture validation: " + validation_error);
  }

  TextureResourceDesc resolved = desc;
  const unsigned uav_space = resolved.binding.space.value_or(0);
  unsigned uav_reg = resolved.binding.register_index.value_or(0);
  if (!resolved.binding.register_index.has_value()) {
    uav_reg = FindNextAvailable(dxil_module->GetUAVs(), uav_space, 0);
  }

  if (HasRegisterIndexConflict(dxil_module->GetUAVs(), uav_space, uav_reg)) {
    return std::unexpected("add_resource: texture UAV binding already exists: space=" + std::to_string(uav_space) + " register=" + std::to_string(uav_reg));
  }

  auto* sym = CreateTextureSymbol(*module, resolved);
  if (sym == nullptr) {
    return std::unexpected("add_resource: failed to create texture symbol for " + resolved.name);
  }

  auto tex = std::make_unique<hlsl::DxilResource>();
  tex->SetGlobalSymbol(sym);
  tex->SetGlobalName(resolved.name);
  tex->SetHLSLType(sym->getType());
  tex->SetSpaceID(uav_space);
  tex->SetLowerBound(uav_reg);
  tex->SetRangeSize(1);
  tex->SetID(static_cast<unsigned>(dxil_module->GetUAVs().size()));
  tex->SetKind(static_cast<hlsl::DXIL::ResourceKind>(static_cast<unsigned>(resolved.kind)));
  if (resolved.kind == ResourceKind::StructuredBuffer) {
    tex->SetElementStride(GetResourceElementStride(resolved));
  }
  tex->SetCompType(GetTextureCompType(static_cast<hlsl::DXIL::ComponentType>(static_cast<uint8_t>(resolved.element_kind))));
  tex->SetSampleCount(0);
  tex->SetRW(true);
  dxil_module->AddUAV(std::move(tex));
  return {};
}

auto ShaderProgram::AddSampler(const SamplerDesc& desc) -> std::expected<void, std::string> {
  if (!module || (dxil_module == nullptr)) {
    return std::unexpected("add_resource: sampler: no loaded shader program");
  }
  if (desc.name.empty()) {
    return std::unexpected("add_resource: refusing to add sampler with empty name");
  }

  SamplerDesc resolved = desc;
  const unsigned samp_space = resolved.binding.space.value_or(0);
  unsigned samp_reg = resolved.binding.register_index.value_or(0);
  if (!resolved.binding.register_index.has_value()) {
    samp_reg = FindNextAvailable(dxil_module->GetSamplers(), samp_space, 0);
  }

  if (HasRegisterIndexConflict(dxil_module->GetSamplers(), samp_space, samp_reg)) {
    return std::unexpected("add_resource: sampler binding already exists: space=" + std::to_string(samp_space) + " register=" + std::to_string(samp_reg));
  }

  auto* global_var = EnsureSamplerGlobal(*module, resolved);
  KeepGlobalAlive(*dxil_module, global_var);

  auto sampler = std::make_unique<hlsl::DxilSampler>();
  sampler->SetGlobalSymbol(global_var);
  sampler->SetGlobalName(resolved.name);
  sampler->SetHLSLType(global_var->getType());
  sampler->SetSpaceID(samp_space);
  sampler->SetLowerBound(samp_reg);
  sampler->SetRangeSize(1);
  sampler->SetID(static_cast<unsigned>(dxil_module->GetSamplers().size()));
  sampler->SetKind(hlsl::DXIL::ResourceKind::Sampler);
  sampler->SetSamplerKind(hlsl::DXIL::SamplerKind::Default);
  dxil_module->AddSampler(std::move(sampler));
  return {};
}

void ShaderProgram::PruneInstruction(llvm::Instruction* instruction) {
  if (instruction == nullptr) return;
  std::unordered_set<llvm::Instruction*> visited;
  std::vector<llvm::WeakTrackingVH> post_order;
  std::vector<llvm::Instruction*> worklist;
  worklist.push_back(instruction);
  while (!worklist.empty()) {
    llvm::Instruction* current = worklist.back();
    worklist.pop_back();
    if (!visited.insert(current).second) continue;
    post_order.emplace_back(current);
    for (const llvm::Use& operand_use : current->operands()) {
      auto* operand_instruction = llvm::dyn_cast<llvm::Instruction>(operand_use.get());
      if (operand_instruction == nullptr || !operand_instruction->use_empty()) continue;
      worklist.push_back(operand_instruction);
    }
  }
  for (auto& it : std::ranges::reverse_view(post_order)) {
    auto* candidate = llvm::dyn_cast_or_null<llvm::Instruction>(static_cast<llvm::Value*>(it));
    if (candidate == nullptr || !candidate->use_empty()) continue;
    bool prunable = false;
    if (const auto* call = llvm::dyn_cast<llvm::CallInst>(candidate)) {
      const llvm::Function* callee = call->getCalledFunction();
      if (callee != nullptr && (call->doesNotAccessMemory() || call->onlyReadsMemory())) {
        prunable = true;
      } else if (callee != nullptr) {
        const llvm::StringRef callee_name = callee->getName();
        if (callee_name == "dx.op.annotateHandle" || callee_name == "dx.op.createHandleFromBinding") {
          prunable = true;
        }
      }
    } else {
      prunable = !candidate->mayHaveSideEffects();
    }
    if (!prunable) continue;
    if (llvm::isInstructionTriviallyDead(candidate)) {
      llvm::RecursivelyDeleteTriviallyDeadInstructions(candidate);
    } else {
      candidate->eraseFromParent();
    }
  }
}

void ShaderProgram::PruneDeadCode() const {
  auto* entryFunc = GetEntryFunction();
  if (entryFunc == nullptr) return;
  bool changed = true;
  while (changed) {
    changed = false;
    std::vector<llvm::WeakTrackingVH> candidates;
    for (llvm::BasicBlock& basic_block : *entryFunc) {
      for (llvm::Instruction& instruction : basic_block) {
        if (!instruction.use_empty()) continue;
        if (instruction.isTerminator()) continue;
        candidates.emplace_back(&instruction);
      }
    }
    for (const llvm::WeakTrackingVH& candidate_handle : candidates) {
      auto* candidate = llvm::dyn_cast_or_null<llvm::Instruction>(static_cast<llvm::Value*>(candidate_handle));
      if (candidate == nullptr || !candidate->use_empty()) continue;
      if (llvm::isInstructionTriviallyDead(candidate)) {
        llvm::RecursivelyDeleteTriviallyDeadInstructions(candidate);
        changed = true;
      } else {
        const llvm::WeakTrackingVH kPruneProbe(candidate);
        ShaderProgram::PruneInstruction(candidate);
        if (static_cast<llvm::Value*>(kPruneProbe) == nullptr) changed = true;
      }
    }
  }
}

auto ShaderProgram::SerializeBitcode() -> std::vector<uint8_t> {
  PruneDeadCode();

  std::string bitcode_bytes;
  llvm::raw_string_ostream ostream(bitcode_bytes);
  llvm::WriteBitcodeToFile(module.get(), ostream);
  ostream.flush();
  return {bitcode_bytes.begin(), bitcode_bytes.end()};
}

auto ShaderProgram::SerializeContainer(std::span<const uint8_t> bitcode, std::vector<uint8_t>& output_container)
    -> std::expected<void, std::string> {
  CComPtr<IMalloc> malloc_interface;
  if (DXC_FAILED(::CoGetMalloc(1, &malloc_interface)) || !malloc_interface) {
    return std::unexpected("serialize: CoGetMalloc failed");
  }

  CComPtr<hlsl::AbstractMemoryStream> bitcode_stream;
  if (DXC_FAILED(hlsl::CreateMemoryStream(malloc_interface, &bitcode_stream)) || !bitcode_stream) {
    return std::unexpected("serialize: failed to create bitcode stream");
  }

  if (!bitcode.empty()) {
    ULONG written = 0;
    if (DXC_FAILED(bitcode_stream->Write(bitcode.data(), static_cast<ULONG>(bitcode.size()), &written)) || written != bitcode.size()) {
      return std::unexpected("serialize: failed to write bitcode stream");
    }
  }

  CComPtr<hlsl::AbstractMemoryStream> output_stream;
  if (DXC_FAILED(hlsl::CreateMemoryStream(malloc_interface, &output_stream)) || !output_stream) {
    return std::unexpected("serialize: failed to create output stream");
  }

  // StripRootSignature prevents SerializeDxilContainerForModule from re-serializing
  // the module mid-write (the passed stream is at its end position, matching DXC).
  constexpr auto kFlags = hlsl::SerializeDxilFlags::StripRootSignature;
  hlsl::SerializeDxilContainerForModule(dxil_module, bitcode_stream, nullptr, output_stream, "", kFlags, nullptr, nullptr,
                                        nullptr, nullptr, 0);

  if ((output_stream->GetPtr() == nullptr) || output_stream->GetPtrSize() == 0) {
    return std::unexpected("serialize: container serialization produced no output");
  }

  output_container.assign(output_stream->GetPtr(), output_stream->GetPtr() + output_stream->GetPtrSize());
  return {};
}

auto ShaderProgram::Serialize() -> std::expected<std::vector<uint8_t>, std::string> {
  if (dxil_module != nullptr) {
    if (auto* op = dxil_module->GetOP()) op->RefreshCache();
    dxil_module->EmitLLVMUsed();
    dxil_module->CollectShaderFlagsForModule();
    dxil_module->ReEmitDxilResources();
    dxil_module->UpgradeToMinValidatorVersion();
    dxil_module->UpdateValidatorVersionMetadata();
    dxil_module->ClearLLVMUsed();
    if (auto* op = dxil_module->GetOP()) op->RefreshCache();
  }
  if (auto verify_result = Verify(); !verify_result) {
    return std::unexpected("serialize: " + verify_result.error());
  }
  auto bitcode = SerializeBitcode();
  std::vector<uint8_t> container;
  if (auto container_result = SerializeContainer(bitcode, container); !container_result) {
    return std::unexpected(std::move(container_result.error()));
  }
  // Unconditional full DXIL validation of the produced container. A patch that
  // corrupts the shader must fail the operation here, not at runtime on the GPU.
  llvm::LLVMContext validation_context;
  std::string diagnostics;
  llvm::raw_string_ostream diag_stream(diagnostics);
  if (FAILED(hlsl::ValidateDxilContainer(container.data(), static_cast<uint32_t>(container.size()),
                                         diag_stream))) {
    diag_stream.flush();
    return std::unexpected("serialize: DXIL validation failed: " + diagnostics);
  }
  return container;
}

auto ShaderProgram::BuildContainerReport(std::span<const uint8_t> container_bytes,
                                         dxp::PatchContainerReport& report) -> std::expected<void, std::string> {
  if (container_bytes.size() < sizeof(hlsl::DxilContainerHeader)) {
    return std::unexpected("container too small to be a valid DXIL container");
  }

  const hlsl::DxilContainerHeader* header = hlsl::IsDxilContainerLike(container_bytes.data(), container_bytes.size());
  if (header == nullptr || !hlsl::IsValidDxilContainer(header, container_bytes.size())) {
    return std::unexpected("failed to validate serialized DXIL container");
  }

  report = dxp::PatchContainerReport{};
  report.format = "DXIL";
  report.total_size_in_bytes = header->ContainerSizeInBytes;
  report.hash_hex = DigestToHex(std::span<const uint8_t, hlsl::DxilContainerHashSize>(header->Hash.Digest));
  report.chunks.reserve(header->PartCount);

  uint32_t part_offset = sizeof(hlsl::DxilContainerHeader);
  for (uint32_t index = 0; index < header->PartCount; ++index) {
    const hlsl::DxilPartHeader* part = hlsl::GetDxilContainerPart(header, index);
    dxp::PatchChunkReport chunk;
    chunk.id = FourCCToString(part->PartFourCC);
    chunk.four_cc = part->PartFourCC;
    chunk.offset_in_container = part_offset;
    chunk.size_in_bytes = part->PartSize;
    part_offset += sizeof(hlsl::DxilPartHeader) + part->PartSize;
    report.chunks.push_back(std::move(chunk));
  }

  return {};
}

auto ShaderProgram::GetOpcodeCounts() const -> std::pair<std::unordered_map<std::string, int32_t>, std::unordered_map<std::string, int32_t>> {
  std::unordered_map<std::string, int32_t> dxil_counts;
  std::unordered_map<std::string, int32_t> llvm_counts;

  auto* entry = GetEntryFunction();
  if (entry == nullptr) return {std::move(dxil_counts), std::move(llvm_counts)};

  for (auto& bb : *entry) {
    for (auto& instr : bb) {
      if (hlsl::OP::IsDxilOpFuncCallInst(&instr)) {
        auto dxil_op = hlsl::OP::GetDxilOpFuncCallInst(&instr);
        const char* name = hlsl::OP::GetOpCodeName(dxil_op);
        if (name != nullptr) {
          dxil_counts[name]++;
        }
      }
      const char* llvm_name = llvm::Instruction::getOpcodeName(instr.getOpcode());
      if (llvm_name != nullptr) {
        llvm_counts[llvm_name]++;
      }
    }
  }

  return {std::move(dxil_counts), std::move(llvm_counts)};
}

auto ShaderProgram::FindNextAvailableTexture(unsigned space, unsigned preferred) -> unsigned {
  return FindNextAvailable(dxil_module->GetSRVs(), space, preferred);
}

auto ShaderProgram::FindNextAvailableUAV(unsigned space, unsigned preferred) -> unsigned {
  return FindNextAvailable(dxil_module->GetUAVs(), space, preferred);
}

auto ShaderProgram::FindNextAvailableCBuffer(unsigned space, unsigned preferred) -> unsigned {
  return FindNextAvailable(dxil_module->GetCBuffers(), space, preferred);
}

auto ShaderProgram::FindNextAvailableSampler(unsigned space, unsigned preferred) -> unsigned {
  return FindNextAvailable(dxil_module->GetSamplers(), space, preferred);
}

auto ShaderProgram::FindNextAvailableInput() -> unsigned {
  std::unordered_set<unsigned> occupied;
  for (const auto& elem : dxil_module->GetInputSignature().GetElements()) {
    if (elem->IsAllocated()) {
      occupied.insert(elem->GetID());
    }
  }
  unsigned register_index = 0;
  while (occupied.contains(register_index)) ++register_index;
  return register_index;
}

auto ShaderProgram::FindNextAvailableOutput() -> unsigned {
  std::unordered_set<unsigned> occupied;
  for (const auto& elem : dxil_module->GetOutputSignature().GetElements()) {
    if (elem->IsAllocated()) {
      occupied.insert(elem->GetID());
    }
  }
  unsigned register_index = 0;
  while (occupied.contains(register_index)) ++register_index;
  return register_index;
}

hlsl::DxilResourceBinding ShaderProgram::ToDxilBinding(const ResourceBindingDesc& desc) {
  hlsl::DxilResourceBinding b{};
  b.resourceClass = static_cast<uint8_t>(desc.resource_class);
  b.rangeLowerBound = desc.register_index.value_or(0);
  b.rangeUpperBound = desc.register_index.value_or(0);
  b.spaceID = desc.space.value_or(0);
  return b;
}

auto ShaderProgram::CreateResourceHandle(const hlsl::DxilResourceBase& resource,
                                         const hlsl::DxilResourceBinding& binding) -> llvm::Value* {
  if (!module || (dxil_module == nullptr)) return nullptr;

  auto* mod = module.get();
  auto* dxil = dxil_module;
  auto* entry = dxil->GetEntryFunction();
  if (entry == nullptr) {
    return nullptr;
  }

  hlsl::OP dxil_op(mod->getContext(), mod);
  dxil_op.InitWithMinPrecision(dxil->GetUseMinPrecision());

  llvm::Constant* resource_binding_constant =
      hlsl::resource_helper::getAsConstant(binding, dxil_op.GetResourceBindingType(),
                                           *dxil->GetShaderModel());
  if (resource_binding_constant == nullptr) {
    return nullptr;
  }

  llvm::Constant* resource_props_constant =
      hlsl::resource_helper::getAsConstant(hlsl::resource_helper::loadPropsFromResourceBase(&resource),
                                           dxil_op.GetResourcePropertiesType(), *dxil->GetShaderModel());
  if (resource_props_constant == nullptr) {
    return nullptr;
  }

  llvm::Function* create_handle_function =
      mod->getFunction("dx.op.createHandleFromBinding");
  if (create_handle_function == nullptr) {
    create_handle_function = dxil_op.GetOpFunc(hlsl::OP::OpCode::CreateHandleFromBinding,
                                               llvm::Type::getVoidTy(dxil_op.GetCtx()));
  }
  if (create_handle_function == nullptr) {
    return nullptr;
  }

  auto get_arg = [create_handle_function](unsigned index) -> const llvm::Argument* {
    if (!create_handle_function || index >= create_handle_function->arg_size()) return nullptr;
    auto it = create_handle_function->arg_begin();
    std::advance(it, index);
    return &*it;
  };

  const llvm::Argument* opcode_argument = get_arg(0);
  const llvm::Argument* index_argument = get_arg(2);
  const llvm::Argument* non_uniform_argument = get_arg(3);
  if ((opcode_argument == nullptr) || (index_argument == nullptr) || (non_uniform_argument == nullptr)) {
    return nullptr;
  }

  // Insert at the entry block's first insertion point (after allocas), never at
  // the end — appending after the block terminator produces invalid IR.
  llvm::IRBuilder<> builder(&*entry->getEntryBlock().getFirstInsertionPt());
  llvm::Value* create_handle = builder.CreateCall(
      create_handle_function,
      {llvm::ConstantInt::get(opcode_argument->getType(),
                              static_cast<uint64_t>(hlsl::OP::OpCode::CreateHandleFromBinding)),
       resource_binding_constant,
       llvm::ConstantInt::get(index_argument->getType(), binding.rangeLowerBound),
       llvm::ConstantInt::get(non_uniform_argument->getType(), 0U)});

  llvm::Function* annotate_handle_function =
      mod->getFunction("dx.op.annotateHandle");
  if (annotate_handle_function == nullptr) {
    annotate_handle_function = dxil_op.GetOpFunc(hlsl::OP::OpCode::AnnotateHandle,
                                                 llvm::Type::getVoidTy(dxil_op.GetCtx()));
  }
  if (annotate_handle_function == nullptr) {
    return nullptr;
  }

  auto get_annotate_arg = [annotate_handle_function](unsigned index) -> const llvm::Argument* {
    if (!annotate_handle_function || index >= annotate_handle_function->arg_size()) return nullptr;
    auto it = annotate_handle_function->arg_begin();
    std::advance(it, index);
    return &*it;
  };

  const llvm::Argument* annotate_opcode_arg = get_annotate_arg(0);
  const llvm::Argument* annotate_handle_arg = get_annotate_arg(1);
  const llvm::Argument* annotate_props_arg = get_annotate_arg(2);
  if ((annotate_opcode_arg == nullptr) || (annotate_handle_arg == nullptr) || (annotate_props_arg == nullptr)) {
    return nullptr;
  }

  return builder.CreateCall(
      annotate_handle_function,
      {llvm::ConstantInt::get(annotate_opcode_arg->getType(),
                              static_cast<uint64_t>(hlsl::OP::OpCode::AnnotateHandle)),
       create_handle, resource_props_constant});
}

const hlsl::DxilResourceBase* FindResourceByRegisterIndex(hlsl::DxilModule& dxil_module,
                                                          hlsl::DXIL::ResourceClass resource_class,
                                                          unsigned register_index, unsigned space) {
  auto matches = [register_index, space](const auto& resource) {
    return resource->GetLowerBound() == register_index && resource->GetSpaceID() == space;
  };
  switch (resource_class) {
    case hlsl::DXIL::ResourceClass::SRV:
      for (const auto& resource : dxil_module.GetSRVs()) {
        if (matches(resource)) return resource.get();
      }
      break;
    case hlsl::DXIL::ResourceClass::UAV:
      for (const auto& resource : dxil_module.GetUAVs()) {
        if (matches(resource)) return resource.get();
      }
      break;
    case hlsl::DXIL::ResourceClass::CBuffer:
      for (const auto& resource : dxil_module.GetCBuffers()) {
        if (matches(resource)) return resource.get();
      }
      break;
    case hlsl::DXIL::ResourceClass::Sampler:
      for (const auto& resource : dxil_module.GetSamplers()) {
        if (matches(resource)) return resource.get();
      }
      break;
    default:
      break;
  }
  return nullptr;
}

auto ShaderProgram::AddInputSignature(const std::string& semantic_name, hlsl::CompType::Kind comp_type,
                                      unsigned vector_size, unsigned register_index,
                                      hlsl::InterpolationMode interp_mode) -> bool {
  if (dxil_module == nullptr) return false;

  auto element = dxil_module->GetInputSignature().CreateElement();
  if (!element) return false;

  hlsl::CompType elem_type;
  switch (comp_type) {
    case hlsl::CompType::Kind::F32:
      elem_type = hlsl::CompType::getF32();
      break;
    case hlsl::CompType::Kind::U32:
      elem_type = hlsl::CompType::getU32();
      break;
    case hlsl::CompType::Kind::I32:
      elem_type = hlsl::CompType::getI32();
      break;
    default:
      elem_type = hlsl::CompType::getF32();
      break;
  }

  element->Initialize(llvm::StringRef(semantic_name), elem_type, interp_mode, 1, vector_size,
                      hlsl::Semantic::kUndefinedRow, hlsl::Semantic::kUndefinedCol, register_index);
  element->SetKind(static_cast<hlsl::Semantic::Kind>(hlsl::DXIL::SemanticKind::Arbitrary));
  element->SetUsageMask(0xff);

  return dxil_module->GetInputSignature().AppendElement(std::move(element)) != UINT_MAX;
}

auto ShaderProgram::AddOutputSignature(const std::string& semantic_name, hlsl::CompType::Kind comp_type,
                                       unsigned vector_size, unsigned register_index) -> bool {
  if (dxil_module == nullptr) return false;

  auto element = dxil_module->GetOutputSignature().CreateElement();
  if (!element) return false;

  hlsl::CompType elem_type;
  switch (comp_type) {
    case hlsl::CompType::Kind::F32:
      elem_type = hlsl::CompType::getF32();
      break;
    case hlsl::CompType::Kind::U32:
      elem_type = hlsl::CompType::getU32();
      break;
    case hlsl::CompType::Kind::I32:
      elem_type = hlsl::CompType::getI32();
      break;
    default:
      elem_type = hlsl::CompType::getF32();
      break;
  }

  element->Initialize(llvm::StringRef(semantic_name), elem_type,
                      hlsl::InterpolationMode(hlsl::DXIL::InterpolationMode::Undefined), 1, vector_size,
                      hlsl::Semantic::kUndefinedRow, hlsl::Semantic::kUndefinedCol, register_index);
  element->SetKind(static_cast<hlsl::Semantic::Kind>(hlsl::DXIL::SemanticKind::Arbitrary));
  element->SetUsageMask(0xff);

  return dxil_module->GetOutputSignature().AppendElement(std::move(element)) != UINT_MAX;
}

auto ShaderProgram::Verify() const -> std::expected<void, std::string> {
  if (!module) {
    return std::unexpected("verify: no loaded module");
  }
  std::string errors;
  llvm::raw_string_ostream ostream(errors);
  if (llvm::verifyModule(*module, &ostream)) {
    ostream.flush();
    return std::unexpected("DXIL module verification failed: " + errors);
  }
  return {};
}

auto ShaderProgram::GetEntryFunction() const -> llvm::Function* {
  return (dxil_module != nullptr) ? dxil_module->GetEntryFunction() : nullptr;
}

template <typename T>
auto ShaderProgram::FindNextAvailable(const std::vector<std::unique_ptr<T>>& resources, unsigned space,
                                      unsigned preferred) -> unsigned {
  unsigned bound_pos = preferred;
  while (true) {
    bool conflict = false;
    for (const auto& res : resources) {
      if (res->GetSpaceID() == space && res->GetLowerBound() == bound_pos) {
        conflict = true;
        break;
      }
    }
    if (!conflict) {
      return bound_pos;
    }
    ++bound_pos;
  }
}

template unsigned ShaderProgram::FindNextAvailable<hlsl::DxilResource>(
    const std::vector<std::unique_ptr<hlsl::DxilResource>>&, unsigned, unsigned);
template unsigned ShaderProgram::FindNextAvailable<hlsl::DxilCBuffer>(
    const std::vector<std::unique_ptr<hlsl::DxilCBuffer>>&, unsigned, unsigned);
template unsigned ShaderProgram::FindNextAvailable<hlsl::DxilSampler>(
    const std::vector<std::unique_ptr<hlsl::DxilSampler>>&, unsigned, unsigned);

}  // namespace dxp::sm6
