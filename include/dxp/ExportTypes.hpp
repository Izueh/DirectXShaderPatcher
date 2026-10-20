#pragma once

#include <array>
#include <bitset>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <dxp/sm5/Model.hpp>

namespace dxp {

/// @brief Interpolation mode for input-signature declarations. Shared vocabulary:
/// alias of @c dxp::sm5::model::InterpolationMode.
using InterpolationMode = sm5::model::InterpolationMode;

/// @brief Resource shape declared for a texture/UAV register. Shared vocabulary:
/// alias of @c dxp::sm5::model::ResourceDimension.
using ResourceDimension = sm5::model::ResourceDimension;

/// @brief Data type a resource read returns. Shared vocabulary:
/// alias of @c dxp::sm5::model::ResourceReturnType.
using ResourceReturnType = sm5::model::ResourceReturnType;

/// @brief Semantic name of SIV/SGV signature declarations. Shared vocabulary:
/// alias of @c dxp::sm5::model::SignatureSemantic.
using SignatureSemantic = sm5::model::SignatureSemantic;

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

/// @brief Scalar component type vocabulary for resource/signature descriptors
/// and typed immediate exports. Mirrors @c hlsl::DXIL::ComponentType.
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

/// @brief Register family (binding slot) a handle belongs to — t/s/b/u registers,
/// temp registers, or signature slots. For the resource's shape, see
/// @c dxp::sm6::ResourceKind or @c dxp::sm5::model::ResourceDimension.
enum class BindingClass : std::uint8_t {
  Unknown,
  Temp,
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

/// @brief Opaque recipe metadata: the YAML subtree under a declaration's `meta:` key.
///
/// DXP parses it and reports it verbatim but never interprets it, so consumers can
/// attach their own hints (provisioning, UI, documentation) without DXP depending on
/// any of them. Numbers are stored as double; object keys keep their YAML order.
struct MetaValue {
  using Array = std::vector<MetaValue>;
  using Object = std::vector<std::pair<std::string, MetaValue>>;

  std::variant<std::monostate, bool, double, std::string, Array, Object> value;

  /// @brief Member lookup on an object value; nullptr when absent or not an object.
  [[nodiscard]] const MetaValue* Find(std::string_view key) const {
    if (const auto* object = std::get_if<Object>(&value)) {
      for (const auto& [k, v] : *object) {
        if (k == key) return &v;
      }
    }
    return nullptr;
  }
  [[nodiscard]] const std::string* AsString() const { return std::get_if<std::string>(&value); }
  [[nodiscard]] const double* AsNumber() const { return std::get_if<double>(&value); }
  [[nodiscard]] const bool* AsBool() const { return std::get_if<bool>(&value); }
  [[nodiscard]] const Array* AsArray() const { return std::get_if<Array>(&value); }
  [[nodiscard]] const Object* AsObject() const { return std::get_if<Object>(&value); }
};

/// @brief One field of a recipe-declared constant buffer.
///
/// Fields are a recipe-side layout: shader bytecode only addresses 16-byte rows and
/// components, so the layout exists to resolve `{ name, field }` references at load
/// time and to tell consumers how to fill the buffer.
struct CBufferField {
  std::string name;
  ComponentType type = ComponentType::F32;  ///< F32, U32 or I32.
  uint32_t width = 1;                       ///< Components, 1-4.
  uint32_t offset = 0;                      ///< Byte offset (explicit or auto-packed).
  std::vector<double> default_value;        ///< Empty = zero; otherwise one entry per component.
  std::optional<MetaValue> meta;
};

/// @brief Layout of a recipe-declared constant buffer.
struct CBufferLayout {
  std::string type_name;
  uint32_t size_in_bytes = 0;
  std::vector<CBufferField> fields;

  [[nodiscard]] const CBufferField* FindField(std::string_view name) const {
    for (const auto& field : fields) {
      if (field.name == name) return &field;
    }
    return nullptr;
  }
};

/// @brief Shader-facing type of a declared or added resource. Fields that a shader
/// model does not express are left unset (e.g. SM5 textures carry no element type).
struct ResourceTypeInfo {
  std::optional<ResourceDimension> dimension;  ///< Textures, UAVs and buffers.
  std::optional<ComponentType> element_type;   ///< Element component type (SM6 textures/UAVs).
  std::optional<uint32_t> vector_width;        ///< Element component count (SM6 textures/UAVs).
  std::optional<uint32_t> structure_stride;    ///< Structured buffers.
  std::optional<CBufferLayout> cbuffer;        ///< Constant buffers declared with fields.
};

/// @brief Resource binding produced by patching.
struct ResourceBinding {
  std::string handle;
  BindingClass binding_class = BindingClass::Unknown;
  uint32_t register_index = 0;
  uint32_t space = 0;
  ResourceTypeInfo type;          ///< What the patched shader expects to find bound here.
  std::optional<MetaValue> meta;  ///< The declaration's `meta:` subtree, verbatim.
};

/// @brief A resource declared by a recipe's add_resource steps, known right after
/// parsing (see Recipe::GetDeclaredResources). Execution may skip it: see
/// @c conditional, and RecipeReport::new_bindings for what a run actually added.
struct ResourceDeclaration {
  std::string step;  ///< add_resource step name.
  std::string handle;
  BindingClass binding_class = BindingClass::Unknown;
  std::optional<uint32_t> register_index;  ///< Unset: assigned at execution.
  std::optional<uint32_t> space;           ///< Unset: assigned at execution (SM5 is always space 0).
  ResourceTypeInfo type;
  bool conditional = false;  ///< The step has a condition or `required: false`.
  std::optional<MetaValue> meta;
};

/// @brief Resources found during pattern matching.
struct ResourceUsage {
  std::string handle;
  BindingClass binding_class = BindingClass::Unknown;
  uint32_t register_index = 0;
  uint32_t space = 0;
  std::bitset<4> accessed_components;

  /// @brief Declaration payload resolved from the shader's dcl_* instructions;
  /// unset when the operand has no resolvable declaration.
  std::optional<ResourceDimension> dimension;
  std::optional<ResourceReturnType> return_type;
  std::optional<uint32_t> structure_stride;
  std::optional<SignatureSemantic> semantic;
  std::optional<InterpolationMode> interpolation;
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
