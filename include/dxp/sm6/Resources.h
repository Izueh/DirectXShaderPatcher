#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "llvm/IR/Module.h"

#include "dxc/DXIL/DxilCompType.h"
#include "dxc/DXIL/DxilConstants.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilResource.h"
#include "dxc/DXIL/DxilResourceBinding.h"
#include "dxc/DXIL/DxilSampler.h"

/// @brief Sentinel bind point value that requests automatic binding selection.
static constexpr unsigned kDxilRecipeAutoBinding = static_cast<unsigned>(-1);

/// @brief Describes a DXIL resource binding range.
struct ResourceBindingDesc {
  static ResourceBindingDesc SRV(unsigned bindPoint = 0, unsigned space = 0) {
    return ResourceBindingDesc(hlsl::DXIL::ResourceClass::SRV, bindPoint,
                               space);
  }

  static ResourceBindingDesc UAV(unsigned bindPoint = 0, unsigned space = 0) {
    return ResourceBindingDesc(hlsl::DXIL::ResourceClass::UAV, bindPoint,
                               space);
  }

  static ResourceBindingDesc CBuffer(unsigned bindPoint = 0,
                                     unsigned space = 0) {
    return ResourceBindingDesc(hlsl::DXIL::ResourceClass::CBuffer, bindPoint,
                               space);
  }

  static ResourceBindingDesc Sampler(unsigned bindPoint = 0,
                                     unsigned space = 0) {
    return ResourceBindingDesc(hlsl::DXIL::ResourceClass::Sampler, bindPoint,
                               space);
  }

  static ResourceBindingDesc AutoSRV(unsigned space = 0) {
    return SRV(kDxilRecipeAutoBinding, space);
  }

  static ResourceBindingDesc AutoUAV(unsigned space = 0) {
    return UAV(kDxilRecipeAutoBinding, space);
  }

  static ResourceBindingDesc AutoCBuffer(unsigned space = 0) {
    return CBuffer(kDxilRecipeAutoBinding, space);
  }

  static ResourceBindingDesc AutoSampler(unsigned space = 0) {
    return Sampler(kDxilRecipeAutoBinding, space);
  }

  explicit ResourceBindingDesc(hlsl::DXIL::ResourceClass resourceClass =
                                   hlsl::DXIL::ResourceClass::Invalid,
                               unsigned bindPoint = 0, unsigned space = 0) {
    Set(bindPoint, space, resourceClass);
  }

  void Set(unsigned bindPoint, unsigned space,
           hlsl::DXIL::ResourceClass resourceClass) {
    dxilBinding.rangeLowerBound = bindPoint;
    dxilBinding.rangeUpperBound = bindPoint;
    dxilBinding.spaceID = space;
    dxilBinding.resourceClass = static_cast<uint8_t>(resourceClass);
    dxilBinding.Reserved1 = 0;
    dxilBinding.Reserved2 = 0;
    dxilBinding.Reserved3 = 0;
  }

  unsigned GetBindPoint() const { return dxilBinding.rangeLowerBound; }
  unsigned GetSpace() const { return dxilBinding.spaceID; }
  bool IsAutoBinding() const {
    return GetBindPoint() == kDxilRecipeAutoBinding;
  }
  hlsl::DXIL::ResourceClass GetResourceClass() const {
    return static_cast<hlsl::DXIL::ResourceClass>(dxilBinding.resourceClass);
  }

  ResourceBindingDesc &Register(unsigned bindPoint, unsigned space) {
    SetBindPoint(bindPoint);
    SetSpace(space);
    return *this;
  }

  ResourceBindingDesc &Auto(unsigned space = 0) {
    return Register(kDxilRecipeAutoBinding, space);
  }

  void SetBindPoint(unsigned bindPoint) {
    dxilBinding.rangeLowerBound = bindPoint;
    dxilBinding.rangeUpperBound = bindPoint;
  }

  void SetSpace(unsigned space) { dxilBinding.spaceID = space; }

  void SetResourceClass(hlsl::DXIL::ResourceClass resourceClass) {
    dxilBinding.resourceClass = static_cast<uint8_t>(resourceClass);
  }

  ResourceBindingDesc &AsSRV() {
    SetResourceClass(hlsl::DXIL::ResourceClass::SRV);
    return *this;
  }

  ResourceBindingDesc &AsUAV() {
    SetResourceClass(hlsl::DXIL::ResourceClass::UAV);
    return *this;
  }

  ResourceBindingDesc &AsCBuffer() {
    SetResourceClass(hlsl::DXIL::ResourceClass::CBuffer);
    return *this;
  }

  ResourceBindingDesc &AsSampler() {
    SetResourceClass(hlsl::DXIL::ResourceClass::Sampler);
    return *this;
  }

  const hlsl::DxilResourceBinding &GetDxilBinding() const {
    return dxilBinding;
  }

  hlsl::DxilResourceBinding dxilBinding = {};
};

/// @brief Describes a constant buffer resource.
struct CBufferDesc {
  std::string name;
  ResourceBindingDesc binding =
      ResourceBindingDesc(hlsl::DXIL::ResourceClass::CBuffer);
  unsigned sizeInBytes = 0;
  const struct CBufferSchema *schema = nullptr;
};

/// @brief Describes one field within a constant buffer schema.
struct CBufferFieldDesc {
  std::string name;
  hlsl::CompType::Kind compType = hlsl::CompType::getU32().GetKind();
  unsigned vectorSize = 1;
  unsigned offset = 0;
};

/// @brief Describes the layout of a constant buffer payload.
struct CBufferSchema {
  std::string typeName;
  unsigned sizeInBytes = 0;
  std::vector<CBufferFieldDesc> fields;
};

/// @brief Builds a constant buffer schema from a standard-layout C++ type.
template <typename TStruct> class CBufferSchemaBuilder {
public:
  explicit CBufferSchemaBuilder(std::string typeName) {
    static_assert(std::is_standard_layout<TStruct>::value,
                  "CBuffer schema types must be standard-layout.");
    schema_.typeName = std::move(typeName);
    schema_.sizeInBytes = static_cast<unsigned>(sizeof(TStruct));
  }

  /// @brief Adds a 32-bit float field to the constant buffer schema.
  CBufferSchemaBuilder &Float(const std::string &name, unsigned offset) {
    return AddField(name, hlsl::CompType::getF32().GetKind(), 1, offset);
  }

  /// @brief Adds a 2-component float vector field to the schema.
  CBufferSchemaBuilder &Float2(const std::string &name, unsigned offset) {
    return AddField(name, hlsl::CompType::getF32().GetKind(), 2, offset);
  }

  /// @brief Adds a 3-component float vector field to the schema.
  CBufferSchemaBuilder &Float3(const std::string &name, unsigned offset) {
    return AddField(name, hlsl::CompType::getF32().GetKind(), 3, offset);
  }

  /// @brief Adds a 4-component float vector field to the schema.
  CBufferSchemaBuilder &Float4(const std::string &name, unsigned offset) {
    return AddField(name, hlsl::CompType::getF32().GetKind(), 4, offset);
  }

  /// @brief Adds a 32-bit unsigned integer field to the schema.
  CBufferSchemaBuilder &UInt(const std::string &name, unsigned offset) {
    return AddField(name, hlsl::CompType::getU32().GetKind(), 1, offset);
  }

  /// @brief Adds a 2-component unsigned int vector field to the schema.
  CBufferSchemaBuilder &UInt2(const std::string &name, unsigned offset) {
    return AddField(name, hlsl::CompType::getU32().GetKind(), 2, offset);
  }

  /// @brief Adds a 3-component unsigned int vector field to the schema.
  CBufferSchemaBuilder &UInt3(const std::string &name, unsigned offset) {
    return AddField(name, hlsl::CompType::getU32().GetKind(), 3, offset);
  }

  /// @brief Adds a 4-component unsigned int vector field to the schema.
  CBufferSchemaBuilder &UInt4(const std::string &name, unsigned offset) {
    return AddField(name, hlsl::CompType::getU32().GetKind(), 4, offset);
  }

  /// @brief Adds a 32-bit signed integer field to the schema.
  CBufferSchemaBuilder &Int(const std::string &name, unsigned offset) {
    return AddField(name, hlsl::CompType::getI32().GetKind(), 1, offset);
  }

  /// @brief Adds a 2-component signed int vector field to the schema.
  CBufferSchemaBuilder &Int2(const std::string &name, unsigned offset) {
    return AddField(name, hlsl::CompType::getI32().GetKind(), 2, offset);
  }

  /// @brief Adds a 3-component signed int vector field to the schema.
  CBufferSchemaBuilder &Int3(const std::string &name, unsigned offset) {
    return AddField(name, hlsl::CompType::getI32().GetKind(), 3, offset);
  }

  /// @brief Adds a 4-component signed int vector field to the schema.
  CBufferSchemaBuilder &Int4(const std::string &name, unsigned offset) {
    return AddField(name, hlsl::CompType::getI32().GetKind(), 4, offset);
  }

  CBufferSchema Build() { return std::move(schema_); }

private:
  CBufferSchemaBuilder &AddField(const std::string &name,
                                 hlsl::CompType::Kind compType,
                                 unsigned vectorSize, unsigned offset) {
    schema_.fields.push_back({name, compType, vectorSize, offset});
    return *this;
  }

  CBufferSchema schema_;
};

/// @brief Describes a texture SRV or UAV resource.
struct TextureResourceDesc {
  std::string name;
  ResourceBindingDesc binding =
      ResourceBindingDesc(hlsl::DXIL::ResourceClass::SRV);
  hlsl::DXIL::ResourceKind kind = hlsl::DXIL::ResourceKind::Texture2D;
  hlsl::DXIL::ComponentType elementKind = hlsl::DXIL::ComponentType::F32;
  unsigned vectorWidth = 4;
  bool isReadWrite = false;
};

/// @brief Builds a constant buffer description.
class CBufferDescBuilder {
public:
  explicit CBufferDescBuilder(std::string name) {
    desc_.name = std::move(name);
  }

  /// @brief Sets the resource binding for this constant buffer.
  CBufferDescBuilder &Binding(const ResourceBindingDesc &binding) {
    desc_.binding = binding;
    return *this;
  }

  /// @brief Sets the constant buffer size in bytes.
  CBufferDescBuilder &SizeInBytes(unsigned size) {
    desc_.sizeInBytes = size;
    return *this;
  }

  /// @brief Sets the schema pointer for this constant buffer.
  CBufferDescBuilder &Schema(const CBufferSchema *schema) {
    desc_.schema = schema;
    return *this;
  }

  CBufferDesc Build() const { return desc_; }

private:
  CBufferDesc desc_;
};

/// @brief Builds a texture resource description.
class TextureResourceBuilder {
public:
  explicit TextureResourceBuilder(std::string name) {
    desc_.name = std::move(name);
  }

  /// @brief Configures this texture as a shader resource view (read-only).
  TextureResourceBuilder &SRV() {
    desc_.binding.AsSRV();
    desc_.isReadWrite = false;
    return *this;
  }

  /// @brief Configures this texture as an unordered access view (read-write).
  TextureResourceBuilder &UAV() {
    desc_.binding.AsUAV();
    desc_.isReadWrite = true;
    return *this;
  }

  /// @brief Sets the texture dimension to Texture2D.
  TextureResourceBuilder &Texture2D() {
    desc_.kind = hlsl::DXIL::ResourceKind::Texture2D;
    return *this;
  }

  /// @brief Sets the texture dimension to Texture2DArray.
  TextureResourceBuilder &Texture2DArray() {
    desc_.kind = hlsl::DXIL::ResourceKind::Texture2DArray;
    return *this;
  }

  /// @brief Configures as a read-write Texture2D UAV.
  TextureResourceBuilder &RWTexture2D() { return UAV().Texture2D(); }

  /// @brief Configures as a read-write Texture2DArray UAV.
  TextureResourceBuilder &RWTexture2DArray() { return UAV().Texture2DArray(); }

  /// @brief Sets the element component type to 32-bit float.
  TextureResourceBuilder &Float(unsigned vectorWidth = 1) {
    return Element(hlsl::DXIL::ComponentType::F32, vectorWidth);
  }

  /// @brief Sets the element to a 2-component float vector.
  TextureResourceBuilder &Float2() { return Float(2); }
  /// @brief Sets the element to a 3-component float vector.
  TextureResourceBuilder &Float3() { return Float(3); }
  /// @brief Sets the element to a 4-component float vector.
  TextureResourceBuilder &Float4() { return Float(4); }

  /// @brief Sets the element component type to 32-bit unsigned int.
  TextureResourceBuilder &UInt(unsigned vectorWidth = 1) {
    return Element(hlsl::DXIL::ComponentType::U32, vectorWidth);
  }

  /// @brief Sets the element to a 2-component unsigned int vector.
  TextureResourceBuilder &UInt2() { return UInt(2); }
  /// @brief Sets the element to a 3-component unsigned int vector.
  TextureResourceBuilder &UInt3() { return UInt(3); }
  /// @brief Sets the element to a 4-component unsigned int vector.
  TextureResourceBuilder &UInt4() { return UInt(4); }

  /// @brief Sets the element component type to 32-bit signed int.
  TextureResourceBuilder &Int(unsigned vectorWidth = 1) {
    return Element(hlsl::DXIL::ComponentType::I32, vectorWidth);
  }

  /// @brief Sets the element to a 2-component signed int vector.
  TextureResourceBuilder &Int2() { return Int(2); }
  /// @brief Sets the element to a 3-component signed int vector.
  TextureResourceBuilder &Int3() { return Int(3); }
  /// @brief Sets the element to a 4-component signed int vector.
  TextureResourceBuilder &Int4() { return Int(4); }

  /// @brief Sets the register bind point and space for this resource.
  TextureResourceBuilder &Register(unsigned bindPoint, unsigned space = 0) {
    desc_.binding.Register(bindPoint, space);
    return *this;
  }

  /// @brief Sets the register space for this resource.
  TextureResourceBuilder &Space(unsigned space) {
    desc_.binding.SetSpace(space);
    return *this;
  }

  /// @brief Requests automatic bind-point assignment to the next available slot.
  TextureResourceBuilder &AutoBinding(unsigned space = 0) {
    desc_.binding.Auto(space);
    return *this;
  }

  /// @brief Sets the element component type and vector width.
  TextureResourceBuilder &Element(hlsl::DXIL::ComponentType elementKind,
                                  unsigned vectorWidth) {
    desc_.elementKind = elementKind;
    desc_.vectorWidth = vectorWidth;
    return *this;
  }

  TextureResourceDesc Build() { return desc_; }

private:
  TextureResourceDesc desc_;
};

/// @brief Describes a sampler resource.
struct SamplerDesc {
  std::string name;
  ResourceBindingDesc binding =
      ResourceBindingDesc(hlsl::DXIL::ResourceClass::Sampler);
};

/// @brief Generates a unique global name within a module.
std::string MakeUniqueGlobalName(const llvm::Module &module,
                                 const std::string &baseName);

/// @brief Finds the next available binding slot for a resource collection.
template <typename TResource>
unsigned FindNextAvailableBinding(
    const std::vector<std::unique_ptr<TResource>> &resources, unsigned space,
    unsigned preferredBindPoint) {
  unsigned bindPoint = preferredBindPoint;
  while (true) {
    bool conflict = false;
    for (const auto &resource : resources) {
      if (resource->GetSpaceID() == space &&
          resource->GetLowerBound() == bindPoint) {
        conflict = true;
        break;
      }
    }

    if (!conflict)
      return bindPoint;

    ++bindPoint;
  }
}

/// @brief Adds a constant buffer resource to a DXIL module.
bool AddCBuffer(llvm::Module &module, hlsl::DxilModule &dxilModule,
                const CBufferDesc &desc);

/// @brief Adds a texture SRV resource to a DXIL module.
bool AddTextureSRV(llvm::Module &module, hlsl::DxilModule &dxilModule,
                   const TextureResourceDesc &desc);

/// @brief Adds a texture UAV resource to a DXIL module.
bool AddTextureUAV(llvm::Module &module, hlsl::DxilModule &dxilModule,
                   const TextureResourceDesc &desc);

/// @brief Adds a Texture2D SRV resource to a DXIL module.
bool AddTexture2DSRV(llvm::Module &module, hlsl::DxilModule &dxilModule,
                     const TextureResourceDesc &desc);

/// @brief Adds a sampler resource to a DXIL module.
bool AddSampler(llvm::Module &module, hlsl::DxilModule &dxilModule,
                const SamplerDesc &desc);