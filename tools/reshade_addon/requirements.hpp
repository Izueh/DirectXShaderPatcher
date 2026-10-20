#pragma once

// What a recipe expects to be bound, as the add-on understands it: the recommended
// `meta:` keys from docs/resource_declarations.md parsed into ResourceHints, plus
// display helpers for dxp::ResourceDeclaration. DXP never interprets `meta`; this is
// the add-on's reading of it. Header-only so tests can use it without the device code.

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "common.hpp"
#include "dxp/ExportTypes.hpp"

namespace dxp::addon {

/// `size:` hint, relative to `size_of` except for kFixed (`width`/`height`).
enum class SizeMode : uint8_t { kFull, kHalf, kQuarter, kEighth, kFixed };

/// `size_of:` hint: what kFull refers to.
enum class SizeReference : uint8_t { kRenderTarget, kBackbuffer };

/// Provisioning hints for one declaration. Every field is optional; invalid or unknown
/// values are skipped and described in `warnings` instead of failing the recipe.
struct ResourceHints {
  std::string description;
  std::string format_name;           // as written, when present
  std::optional<api::format> format; // set when format_name is a known format
  std::optional<SizeMode> size;
  std::optional<SizeReference> size_of;
  std::optional<uint32_t> width;
  std::optional<uint32_t> height;
  std::optional<std::array<float, 4>> clear;
  std::vector<std::string> warnings;
};

/// A recipe declaration plus the add-on's reading of its `meta`.
struct DeclaredResource {
  dxp::ResourceDeclaration declaration;
  ResourceHints hints;
};

namespace detail {

inline std::string LowerAscii(std::string_view text) {
  std::string out(text);
  std::ranges::transform(out, out.begin(), [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
  return out;
}

inline std::string DescribeMeta(const dxp::MetaValue& value) {
  if (const auto* text = value.AsString()) return std::format("'{}'", *text);
  if (const auto* number = value.AsNumber()) return std::format("{}", *number);
  if (const auto* flag = value.AsBool()) return *flag ? "true" : "false";
  if (value.AsArray() != nullptr) return "a list";
  if (value.AsObject() != nullptr) return "a map";
  return "null";
}

inline std::optional<uint32_t> PositiveInteger(const dxp::MetaValue& value) {
  const double* number = value.AsNumber();
  if (number == nullptr || *number < 1 || *number > 65536 || std::floor(*number) != *number) return std::nullopt;
  return static_cast<uint32_t>(*number);
}

}  // namespace detail

/// Maps a DXGI-style format name ("r16g16b16a16_float", optionally prefixed with
/// "DXGI_FORMAT_", any case) to a ReShade format.
inline std::optional<api::format> ParseFormatName(std::string_view name) {
  std::string lower = detail::LowerAscii(name);
  if (lower.starts_with("dxgi_format_")) lower.erase(0, 12);
#define DXP_FORMAT(f) {#f, api::format::f}
  static constexpr std::pair<std::string_view, api::format> kFormats[] = {
      DXP_FORMAT(r1_unorm), DXP_FORMAT(r8_typeless), DXP_FORMAT(r8_unorm), DXP_FORMAT(r8_uint), DXP_FORMAT(r8_snorm),
      DXP_FORMAT(r8_sint), DXP_FORMAT(a8_unorm), DXP_FORMAT(r8g8_typeless), DXP_FORMAT(r8g8_unorm), DXP_FORMAT(r8g8_uint),
      DXP_FORMAT(r8g8_snorm), DXP_FORMAT(r8g8_sint), DXP_FORMAT(r8g8b8a8_typeless), DXP_FORMAT(r8g8b8a8_unorm),
      DXP_FORMAT(r8g8b8a8_unorm_srgb), DXP_FORMAT(r8g8b8a8_uint), DXP_FORMAT(r8g8b8a8_snorm), DXP_FORMAT(r8g8b8a8_sint),
      DXP_FORMAT(b8g8r8a8_typeless), DXP_FORMAT(b8g8r8a8_unorm), DXP_FORMAT(b8g8r8a8_unorm_srgb), DXP_FORMAT(b8g8r8x8_typeless),
      DXP_FORMAT(b8g8r8x8_unorm), DXP_FORMAT(b8g8r8x8_unorm_srgb), DXP_FORMAT(r10g10b10a2_typeless), DXP_FORMAT(r10g10b10a2_unorm),
      DXP_FORMAT(r10g10b10a2_uint), DXP_FORMAT(r10g10b10a2_xr_bias), DXP_FORMAT(r16_typeless), DXP_FORMAT(r16_float),
      DXP_FORMAT(r16_unorm), DXP_FORMAT(r16_uint), DXP_FORMAT(r16_snorm), DXP_FORMAT(r16_sint), DXP_FORMAT(r16g16_typeless),
      DXP_FORMAT(r16g16_float), DXP_FORMAT(r16g16_unorm), DXP_FORMAT(r16g16_uint), DXP_FORMAT(r16g16_snorm), DXP_FORMAT(r16g16_sint),
      DXP_FORMAT(r16g16b16a16_typeless), DXP_FORMAT(r16g16b16a16_float), DXP_FORMAT(r16g16b16a16_unorm),
      DXP_FORMAT(r16g16b16a16_uint), DXP_FORMAT(r16g16b16a16_snorm), DXP_FORMAT(r16g16b16a16_sint), DXP_FORMAT(r32_typeless),
      DXP_FORMAT(r32_float), DXP_FORMAT(r32_uint), DXP_FORMAT(r32_sint), DXP_FORMAT(r32g32_typeless), DXP_FORMAT(r32g32_float),
      DXP_FORMAT(r32g32_uint), DXP_FORMAT(r32g32_sint), DXP_FORMAT(r32g32b32_typeless), DXP_FORMAT(r32g32b32_float),
      DXP_FORMAT(r32g32b32_uint), DXP_FORMAT(r32g32b32_sint), DXP_FORMAT(r32g32b32a32_typeless), DXP_FORMAT(r32g32b32a32_float),
      DXP_FORMAT(r32g32b32a32_uint), DXP_FORMAT(r32g32b32a32_sint), DXP_FORMAT(r9g9b9e5), DXP_FORMAT(r11g11b10_float),
      DXP_FORMAT(b5g6r5_unorm), DXP_FORMAT(b5g5r5a1_unorm), DXP_FORMAT(b5g5r5x1_unorm), DXP_FORMAT(b4g4r4a4_unorm),
      DXP_FORMAT(a4b4g4r4_unorm), DXP_FORMAT(s8_uint), DXP_FORMAT(d16_unorm), DXP_FORMAT(d16_unorm_s8_uint),
      DXP_FORMAT(d24_unorm_x8_uint), DXP_FORMAT(d24_unorm_s8_uint), DXP_FORMAT(d32_float), DXP_FORMAT(d32_float_s8_uint),
      DXP_FORMAT(r24_g8_typeless), DXP_FORMAT(r24_unorm_x8_uint), DXP_FORMAT(x24_unorm_g8_uint), DXP_FORMAT(r32_g8_typeless),
      DXP_FORMAT(r32_float_x8_uint), DXP_FORMAT(x32_float_g8_uint), DXP_FORMAT(bc1_typeless), DXP_FORMAT(bc1_unorm),
      DXP_FORMAT(bc1_unorm_srgb), DXP_FORMAT(bc2_typeless), DXP_FORMAT(bc2_unorm), DXP_FORMAT(bc2_unorm_srgb),
      DXP_FORMAT(bc3_typeless), DXP_FORMAT(bc3_unorm), DXP_FORMAT(bc3_unorm_srgb), DXP_FORMAT(bc4_typeless), DXP_FORMAT(bc4_unorm),
      DXP_FORMAT(bc4_snorm), DXP_FORMAT(bc5_typeless), DXP_FORMAT(bc5_unorm), DXP_FORMAT(bc5_snorm), DXP_FORMAT(bc6h_typeless),
      DXP_FORMAT(bc6h_ufloat), DXP_FORMAT(bc6h_sfloat), DXP_FORMAT(bc7_typeless), DXP_FORMAT(bc7_unorm), DXP_FORMAT(bc7_unorm_srgb),
  };
#undef DXP_FORMAT
  for (const auto& [format_name, format] : kFormats) {
    if (format_name == lower) return format;
  }
  return std::nullopt;
}

/// Reads the recommended keys from a declaration's `meta`. Unknown scalar keys are
/// reported as likely typos; nested maps are left alone since host-specific keys
/// belong under a namespace (`meta: { reshade: { ... } }`).
inline ResourceHints ParseResourceHints(const std::optional<dxp::MetaValue>& meta) {
  ResourceHints hints;
  if (!meta) return hints;
  const auto* object = meta->AsObject();
  if (object == nullptr) {
    hints.warnings.push_back(std::format("meta should be a map, got {}", detail::DescribeMeta(*meta)));
    return hints;
  }

  const auto invalid = [&](std::string_view key, const dxp::MetaValue& value, std::string_view expected) {
    hints.warnings.push_back(std::format("meta.{}: {} is not {}, ignored", key, detail::DescribeMeta(value), expected));
  };

  for (const auto& [key, value] : *object) {
    if (key == "description") {
      if (const auto* text = value.AsString()) hints.description = *text;
      else invalid(key, value, "a string");
    } else if (key == "format") {
      const auto* text = value.AsString();
      if (text == nullptr) {
        invalid(key, value, "a format name");
        continue;
      }
      hints.format_name = *text;
      hints.format = ParseFormatName(*text);
      if (!hints.format) hints.warnings.push_back(std::format("meta.format: unknown format '{}'", *text));
    } else if (key == "size") {
      constexpr std::pair<std::string_view, SizeMode> kModes[] = {
          {"full", SizeMode::kFull}, {"half", SizeMode::kHalf}, {"quarter", SizeMode::kQuarter},
          {"eighth", SizeMode::kEighth}, {"fixed", SizeMode::kFixed}};
      const auto* text = value.AsString();
      const auto it = text ? std::ranges::find(kModes, detail::LowerAscii(*text), &std::pair<std::string_view, SizeMode>::first)
                           : std::ranges::end(kModes);
      if (it != std::ranges::end(kModes)) hints.size = it->second;
      else invalid(key, value, "full, half, quarter, eighth or fixed");
    } else if (key == "size_of") {
      const auto* text = value.AsString();
      const std::string lower = text ? detail::LowerAscii(*text) : std::string();
      if (lower == "render_target") hints.size_of = SizeReference::kRenderTarget;
      else if (lower == "backbuffer") hints.size_of = SizeReference::kBackbuffer;
      else invalid(key, value, "render_target or backbuffer");
    } else if (key == "width" || key == "height") {
      if (const auto pixels = detail::PositiveInteger(value)) (key == "width" ? hints.width : hints.height) = pixels;
      else invalid(key, value, "a pixel count");
    } else if (key == "clear") {
      std::array<float, 4> color{};
      bool ok = false;
      if (const auto* number = value.AsNumber()) {
        color.fill(static_cast<float>(*number));
        ok = true;
      } else if (const auto* list = value.AsArray(); list && !list->empty() && list->size() <= 4) {
        ok = std::ranges::all_of(*list, [](const dxp::MetaValue& v) { return v.AsNumber() != nullptr; });
        for (size_t i = 0; ok && i < list->size(); ++i) color[i] = static_cast<float>(*(*list)[i].AsNumber());
      }
      if (ok) hints.clear = color;
      else invalid(key, value, "a number or a list of 1-4 numbers");
    } else if (value.AsObject() == nullptr) {
      hints.warnings.push_back(std::format("meta.{}: unknown key, ignored", key));
    }
  }

  if (hints.size == SizeMode::kFixed && (!hints.width || !hints.height)) {
    hints.warnings.push_back("meta.size is fixed but width/height are missing");
  } else if (hints.size != SizeMode::kFixed && (hints.width || hints.height)) {
    hints.warnings.push_back("meta.width/height only apply with size: fixed");
  }
  if (hints.size_of && hints.size == SizeMode::kFixed) {
    hints.warnings.push_back("meta.size_of does not apply with size: fixed");
  }
  return hints;
}

inline std::vector<DeclaredResource> DescribeDeclarations(std::vector<dxp::ResourceDeclaration> declarations) {
  std::vector<DeclaredResource> out;
  out.reserve(declarations.size());
  for (auto& declaration : declarations) {
    ResourceHints hints = ParseResourceHints(declaration.meta);
    out.push_back({std::move(declaration), std::move(hints)});
  }
  return out;
}

inline const char* SizeModeName(SizeMode mode) {
  constexpr const char* kNames[] = {"full", "half", "quarter", "eighth", "fixed"};
  return kNames[static_cast<size_t>(mode)];
}

inline const char* BindingClassName(dxp::BindingClass binding_class) {
  switch (binding_class) {
    case dxp::BindingClass::Texture:            return "Texture";
    case dxp::BindingClass::TextureUav:
    case dxp::BindingClass::Uav:                return "UAV";
    case dxp::BindingClass::RawResource:        return "Raw buffer";
    case dxp::BindingClass::StructuredResource: return "Structured buffer";
    case dxp::BindingClass::CBuffer:            return "Constant buffer";
    case dxp::BindingClass::Sampler:            return "Sampler";
    default:                                    return "Other";
  }
}

inline const char* DimensionName(dxp::ResourceDimension dimension) {
  constexpr const char* kNames[] = {"Unknown", "Buffer", "Texture1D", "Texture2D", "Texture2DMS", "Texture3D", "TextureCube",
                                    "Texture1DArray", "Texture2DArray", "Texture2DMSArray", "TextureCubeArray", "RawBuffer",
                                    "StructuredBuffer"};
  const auto index = static_cast<size_t>(dimension);
  return index < std::size(kNames) ? kNames[index] : "Unknown";
}

inline const char* ComponentTypeName(dxp::ComponentType type) {
  constexpr const char* kNames[] = {"Invalid", "I1", "I16", "U16", "I32", "U32", "I64", "U64", "F16", "F32", "F64",
                                    "SNormF16", "UNormF16", "SNormF32", "UNormF32", "SNormF64", "UNormF64",
                                    "PackedS8x32", "PackedU8x32", "U8", "I8", "F8_E4M3FN", "F8_E5M2"};
  const auto index = static_cast<size_t>(type);
  return index < std::size(kNames) ? kNames[index] : "?";
}

inline const char* ViewTypeName(api::resource_view_type type) {
  constexpr const char* kNames[] = {"unknown", "buffer", "1D", "1D array", "2D", "2D array", "2D MS", "2D MS array",
                                    "3D", "cube", "cube array", "acceleration structure"};
  const auto index = static_cast<size_t>(type);
  return index < std::size(kNames) ? kNames[index] : "unknown";
}

/// Shape of the declaration, e.g. "Texture3D, F32x4" or "stride 16" or "32 bytes, 3 fields".
inline std::string DescribeType(const dxp::ResourceTypeInfo& type) {
  std::string out;
  const auto add = [&](const std::string& part) { out += (out.empty() ? "" : ", ") + part; };
  if (type.dimension) add(DimensionName(*type.dimension));
  if (type.element_type) {
    add(type.vector_width && *type.vector_width > 1
            ? std::format("{}x{}", ComponentTypeName(*type.element_type), *type.vector_width)
            : ComponentTypeName(*type.element_type));
  }
  if (type.structure_stride) add(std::format("stride {}", *type.structure_stride));
  if (type.cbuffer) add(std::format("{} bytes, {} field(s)", type.cbuffer->size_in_bytes, type.cbuffer->fields.size()));
  return out;
}

/// Register and space, e.g. "t0, space100", or "auto" when the register is assigned at
/// execution (SM5 always reports space 0, which needs no mention then).
inline std::string DescribeSlot(const dxp::ResourceDeclaration& declaration) {
  char prefix = 't';
  switch (declaration.binding_class) {
    case dxp::BindingClass::Uav:
    case dxp::BindingClass::TextureUav: prefix = 'u'; break;
    case dxp::BindingClass::Sampler:    prefix = 's'; break;
    case dxp::BindingClass::CBuffer:    prefix = 'b'; break;
    default:                            break;
  }
  if (!declaration.register_index) {
    return declaration.space.value_or(0) != 0 ? std::format("auto, space{}", *declaration.space) : "auto";
  }
  const std::string reg = std::format("{}{}", prefix, *declaration.register_index);
  return declaration.space ? std::format("{}, space{}", reg, *declaration.space) : std::format("{}, space auto", reg);
}

/// The view a DDS file of this view type gets matches the declared shader dimension.
/// Unset or unknown declared dimensions match anything.
inline bool DimensionMatches(std::optional<dxp::ResourceDimension> declared, api::resource_view_type file) {
  using D = dxp::ResourceDimension;
  using V = api::resource_view_type;
  if (!declared || *declared == D::Unknown) return true;
  switch (*declared) {
    case D::Texture1D:        return file == V::texture_1d;
    case D::Texture1DArray:   return file == V::texture_1d_array;
    case D::Texture2D:        return file == V::texture_2d;
    case D::Texture2DArray:   return file == V::texture_2d_array;
    case D::Texture2DMS:      return file == V::texture_2d_multisample;
    case D::Texture2DMSArray: return file == V::texture_2d_multisample_array;
    case D::Texture3D:        return file == V::texture_3d;
    case D::TextureCube:      return file == V::texture_cube;
    case D::TextureCubeArray: return file == V::texture_cube_array;
    default:                  return file == V::buffer;
  }
}

}  // namespace dxp::addon
