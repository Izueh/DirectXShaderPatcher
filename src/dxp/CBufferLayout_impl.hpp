#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include <glaze/glaze.hpp>

#include "dxp/ExportTypes.hpp"

namespace dxp::detail {

/// @brief YAML form of one constant-buffer field (shared by SM5 and SM6).
struct CBufferFieldData {
  std::string name;
  dxp::ComponentType type = dxp::ComponentType::F32;
  uint32_t width = 1;
  std::optional<uint32_t> offset;  ///< Unset: auto-packed after the previous field.
  /// Number or bool (broadcast to every component), or one number per component.
  /// Parsed generically for the same glaze limitation as meta (null = absent).
  glz::generic default_value;
  // Not std::optional: glaze 7.9 reads only the first key of a block-style YAML mapping into
  // an optional map/generic. A null generic means the key was absent.
  glz::generic meta;
};

/// @brief Cbuffer layouts declared so far while a recipe is loaded, by handle. Field
/// references in emits resolve against layouts declared by earlier steps.
using CBufferLayouts = std::unordered_map<std::string, dxp::CBufferLayout>;

/// @brief Converts a parsed `meta:` subtree into the public, glaze-free form.
dxp::MetaValue ToMetaValue(const glz::generic& value);
/// @brief As above, but a null value (an absent `meta:` key) maps to nullopt.
std::optional<dxp::MetaValue> ToOptionalMetaValue(const glz::generic& value);

/// @brief Resolves a field list into a layout using HLSL constant-buffer packing:
/// 4-byte components in 16-byte rows, and no field may straddle a row. Fields
/// without an offset are placed after the previous field; explicit offsets are
/// validated. The size defaults to the packed size rounded up to 16 bytes.
std::expected<dxp::CBufferLayout, std::string> BuildCBufferLayout(std::string_view handle, std::string type_name,
                                                                 uint32_t declared_size,
                                                                 const std::vector<CBufferFieldData>& fields);

}  // namespace dxp::detail

namespace glz {

template <>
struct meta<dxp::ComponentType> {
  using T = dxp::ComponentType;
  static constexpr auto keys = std::array{
      "Invalid", "I1", "I16", "U16", "I32", "U32", "I64", "U64",
      "F16", "F32", "F64", "SNormF16", "UNormF16", "SNormF32", "UNormF32",
      "SNormF64", "UNormF64", "PackedS8x32", "PackedU8x32", "U8", "I8",
      "F8_E4M3FN", "F8_E5M2"};
  static constexpr auto value = std::array{
      T::Invalid, T::I1, T::I16, T::U16, T::I32, T::U32, T::I64, T::U64,
      T::F16, T::F32, T::F64, T::SNormF16, T::UNormF16, T::SNormF32, T::UNormF32,
      T::SNormF64, T::UNormF64, T::PackedS8x32, T::PackedU8x32, T::U8, T::I8,
      T::F8_E4M3FN, T::F8_E5M2};
};

template <>
struct meta<dxp::detail::CBufferFieldData> {
  using T = dxp::detail::CBufferFieldData;
  static constexpr auto value = object("name", &T::name, "type", &T::type, "width", &T::width, "offset", &T::offset,
                                       "default", &T::default_value, "meta", &T::meta);
};

}  // namespace glz
