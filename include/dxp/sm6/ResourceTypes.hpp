#pragma once

#include <cstdint>
#include <dxp/ExportTypes.hpp>
#include <optional>
#include <string>
#include <vector>

namespace dxp::sm6 {

/// @brief Resource class. Mirrors @c hlsl::DXIL::ResourceClass.
enum class ResourceClass : uint8_t {
  SRV = 0,
  UAV,
  CBuffer,
  Sampler,
  Invalid,
};

/// @brief Resource kind. Mirrors @c hlsl::DXIL::ResourceKind.
enum class DxilResourceKind : uint8_t {
  Invalid = 0,
  Texture1D,
  Texture2D,
  Texture2DMS,
  Texture3D,
  TextureCube,
  Texture1DArray,
  Texture2DArray,
  Texture2DMSArray,
  TextureCubeArray,
  TypedBuffer,
  RawBuffer,
  StructuredBuffer,
  CBuffer,
  Sampler,
  TBuffer,
  RTAccelerationStructure,
  FeedbackTexture2D,
  FeedbackTexture2DArray,
  NumEntries,
};

/// @brief Interpolation mode. Mirrors @c hlsl::DXIL::InterpolationMode.
enum class InterpolationMode : uint8_t {
  Undefined = 0,
  Constant = 1,
  Linear = 2,
  LinearCentroid = 3,
  LinearNoperspective = 4,
  LinearNoperspectiveCentroid = 5,
  LinearSample = 6,
  LinearNoperspectiveSample = 7,
  Invalid = 8,
};

/// @brief Resource binding descriptor. User-facing mirror of @c hlsl::DxilResourceBinding.
struct ResourceBindingDesc {
  ResourceClass resource_class = ResourceClass::SRV;
  std::optional<uint32_t> register_index = std::nullopt;  ///< Register index. nullopt = auto.
  std::optional<uint32_t> space = std::nullopt;           ///< Space. nullopt = auto.
};

/// @brief Constant buffer resource descriptor. User-facing mirror of @c hlsl::DxilCBuffer.
struct CBufferDesc {
  std::string name;                                                                             ///< Cbuffer name.
  ResourceBindingDesc binding = ResourceBindingDesc{.resource_class = ResourceClass::CBuffer};  ///< Binding info.
  uint32_t size_in_bytes = 0;                                                                   ///< Size in bytes.
  const struct CBufferSchema* schema = nullptr;                                                 ///< Optional schema.
};

/// @brief Constant buffer schema field descriptor. Mirrors a field within @c hlsl::DxilCBuffer schema.
struct CBufferFieldDesc {
  std::string name;                                        ///< Field name.
  dxp::ComponentType comp_type = dxp::ComponentType::U32;  ///< Component type.
  uint32_t vector_size = 1;                                ///< Vector size.
  uint32_t offset = 0;                                     ///< Byte offset.
};

/// @brief Constant buffer schema descriptor. Mirrors @c hlsl::DxilCBuffer schema.
struct CBufferSchema {
  std::string type_name;                 ///< Type name.
  uint32_t size_in_bytes = 0;            ///< Total size in bytes.
  std::vector<CBufferFieldDesc> fields;  ///< Field list.
};

/// @brief Texture SRV/UAV resource descriptor. User-facing mirror of @c hlsl::DxilResource (texture variant).
struct TextureResourceDesc {
  std::string name;                                                                         ///< Texture name.
  ResourceBindingDesc binding = ResourceBindingDesc{.resource_class = ResourceClass::SRV};  ///< Binding info.
  DxilResourceKind kind = DxilResourceKind::Texture2D;                                      ///< Texture kind.
  dxp::ComponentType element_kind = dxp::ComponentType::F32;                                ///< Element type.
  uint32_t vector_width = 4;                                                                ///< Vector width.
  bool is_read_write = false;                                                               ///< Whether this is a UAV.
};

/// @brief Sampler resource descriptor. User-facing mirror of @c hlsl::DxilSampler.
struct SamplerDesc {
  std::string name;                                                                             ///< Sampler name.
  ResourceBindingDesc binding = ResourceBindingDesc{.resource_class = ResourceClass::Sampler};  ///< Binding info.
};

}  // namespace dxp::sm6
