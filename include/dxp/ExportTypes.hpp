#pragma once

#include <bitset>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace dxp {

/// @brief Variant of primitive literal value types.
/// Absence/unset is expressed by map-absence or std::optional, never by a variant
/// alternative — an "empty" value is a step-level std::optional<ConditionNode>.
using PrimitiveValue = std::variant<bool, int32_t, uint32_t, int64_t, uint64_t, double>;

/// @brief One serialized container chunk.
struct PatchChunkReport {
  std::string id;
  uint32_t four_cc = 0;
  uint32_t offset_in_container = 0;
  uint32_t size_in_bytes = 0;
};

/// @brief Final serialized output container.
struct PatchContainerReport {
  std::string format;
  uint32_t total_size_in_bytes = 0;
  std::string hash_hex;
  std::vector<PatchChunkReport> chunks;
};

/// @brief Scalar component type vocabulary — the single shared mirror of
/// @c hlsl::DXIL::ComponentType (value-identical; verified by static_assert in
/// src/dxp/sm6/EnumMirrors.hpp). Used both for resource/signature descriptors
/// (SM6) and for typed immediate exports on both backends.
///
/// Values marked "not currently emitted" are not produced by the immediate
/// capture engines yet; they exist for API completeness and future use.
enum class ComponentType : std::uint8_t {
  Invalid = 0,
  I1 = 1,  ///< Not currently emitted.
  I16 = 2,
  U16 = 3,
  I32 = 4,
  U32 = 5,
  I64 = 6,
  U64 = 7,
  F16 = 8,
  F32 = 9,
  F64 = 10,
  SNormF16 = 11,     ///< Not currently emitted.
  UNormF16 = 12,     ///< Not currently emitted.
  SNormF32 = 13,     ///< Not currently emitted.
  UNormF32 = 14,     ///< Not currently emitted.
  SNormF64 = 15,     ///< Not currently emitted.
  UNormF64 = 16,     ///< Not currently emitted.
  PackedS8x32 = 17,  ///< Not currently emitted.
  PackedU8x32 = 18,  ///< Not currently emitted.
  U8 = 19,
  I8 = 20,
  F8_E4M3FN = 21,  ///< Not currently emitted. @c hlsl::DXIL::ComponentType::F8_E4M3
  F8_E5M2 = 22,    ///< Not currently emitted.
  LastEntry = 23,  ///< Sentinel; mirrors @c hlsl::DXIL::ComponentType::LastEntry.
};

/// @brief Unified resource kind for typed exports.
/// A deliberate cross-backend union (sm5 operand families + sm6 resource kinds),
/// NOT a mirror of any single D3D/DXC enum — do not cast to/from token enums.
enum class ResourceKind : std::uint8_t {
  Unknown,
  Input,
  Output,
  Texture,
  TextureUav,
  RawResource,
  StructuredResource,
  CBuffer,
  Sampler,
  Uav,
};

/// @brief Resource binding produced by patching.
struct ResourceBinding {
  std::string handle;
  ResourceKind resource_kind = ResourceKind::Unknown;
  uint32_t register_index = 0;
  uint32_t space = 0;
};

/// @brief Resources found during pattern matching.
struct ResourceUsage {
  std::string handle;
  std::string handle_name;
  ResourceKind type = ResourceKind::Unknown;
  std::string register_name;
  uint32_t register_index = 0;
  uint32_t space = 0;
  uint32_t member_offset = 0;
  uint32_t member_size = 0;
  std::bitset<4> accessed_components;
};

/// @brief Immediate values found during pattern matching.
struct ImmediateValue {
  std::string name;
  /// Right-aligned, zero-extended bits; @p type defines the meaningful width
  /// (I1 → 0/1, I8 → low byte, I16 → low 16 bits, F16 → half bits in the low
  /// word, F32 → 32-bit float bits, F64 → full 64 bits).
  std::vector<uint64_t> raw_values;
  ComponentType type = ComponentType::F32;
};

}  // namespace dxp
