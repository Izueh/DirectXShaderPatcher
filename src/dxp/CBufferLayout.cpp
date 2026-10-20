#include "dxp/CBufferLayout_impl.hpp"

#include <algorithm>
#include <format>
#include <type_traits>

namespace dxp::detail {

namespace {

constexpr uint32_t kRowBytes = 16;
constexpr uint32_t kComponentBytes = 4;

uint32_t AlignUp(uint32_t value, uint32_t alignment) {
  return (value + alignment - 1) / alignment * alignment;
}

}  // namespace

dxp::MetaValue ToMetaValue(const glz::generic& value) {
  dxp::MetaValue out;
  std::visit(
      [&](const auto& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, bool>) {
          out.value = v;
        } else if constexpr (std::is_arithmetic_v<T>) {
          out.value = static_cast<double>(v);
        } else if constexpr (std::is_same_v<T, std::string>) {
          out.value = v;
        } else if constexpr (std::is_same_v<T, glz::generic::array_t>) {
          dxp::MetaValue::Array array;
          array.reserve(v.size());
          for (const auto& element : v) array.push_back(ToMetaValue(element));
          out.value = std::move(array);
        } else if constexpr (std::is_same_v<T, glz::generic::object_t>) {
          dxp::MetaValue::Object object;
          for (const auto& [key, element] : v) object.emplace_back(std::string(key), ToMetaValue(element));
          out.value = std::move(object);
        }
        // null stays monostate
      },
      value.data);
  return out;
}

std::optional<dxp::MetaValue> ToOptionalMetaValue(const glz::generic& value) {
  if (value.is_null()) return std::nullopt;
  return ToMetaValue(value);
}

std::expected<dxp::CBufferLayout, std::string> BuildCBufferLayout(std::string_view handle, std::string type_name,
                                                                 uint32_t declared_size,
                                                                 const std::vector<CBufferFieldData>& fields) {
  const auto fail = [&](const std::string& message) {
    return std::unexpected(std::format("cbuffer '{}': {}", handle, message));
  };

  dxp::CBufferLayout layout;
  layout.type_name = type_name.empty() ? std::string(handle) : std::move(type_name);

  uint32_t cursor = 0;
  for (const CBufferFieldData& data : fields) {
    dxp::CBufferField field;
    field.name = data.name;
    field.type = data.type;
    field.width = data.width == 0 ? 1 : data.width;
    field.meta = ToOptionalMetaValue(data.meta);

    if (field.name.empty()) return fail("field name must not be empty");
    if (layout.FindField(field.name) != nullptr) return fail(std::format("duplicate field '{}'", field.name));
    if (field.type != dxp::ComponentType::F32 && field.type != dxp::ComponentType::U32 && field.type != dxp::ComponentType::I32) {
      return fail(std::format("field '{}': type must be F32, U32 or I32", field.name));
    }
    if (field.width > 4) return fail(std::format("field '{}': width must be 1-4", field.name));

    const uint32_t bytes = field.width * kComponentBytes;
    if (data.offset) {
      field.offset = *data.offset;
      if (field.offset % kComponentBytes != 0) return fail(std::format("field '{}': offset must be a multiple of 4", field.name));
      if (field.offset < cursor) {
        return fail(std::format("field '{}': offset {} overlaps the previous field or is out of ascending order", field.name, field.offset));
      }
    } else {
      field.offset = cursor;
      // A field never straddles a 16-byte row: start a new row when it does not fit.
      if (field.offset % kRowBytes + bytes > kRowBytes) field.offset = AlignUp(field.offset, kRowBytes);
    }
    if (field.offset % kRowBytes + bytes > kRowBytes) {
      return fail(std::format("field '{}' at offset {} straddles a 16-byte row", field.name, field.offset));
    }
    cursor = field.offset + bytes;

    if (!data.default_value.is_null()) {
      const dxp::MetaValue value = ToMetaValue(data.default_value);
      if (const bool* b = value.AsBool()) {
        field.default_value.assign(field.width, *b ? 1.0 : 0.0);
      } else if (const double* number = value.AsNumber()) {
        field.default_value.assign(field.width, *number);
      } else if (const auto* array = value.AsArray()) {
        for (const auto& element : *array) {
          if (element.AsNumber() == nullptr) return fail(std::format("field '{}': default entries must be numbers", field.name));
          field.default_value.push_back(*element.AsNumber());
        }
      } else {
        return fail(std::format("field '{}': default must be a number, a bool or a list of numbers", field.name));
      }
      if (field.default_value.size() != field.width) {
        return fail(std::format("field '{}': default has {} value(s) for width {}", field.name, field.default_value.size(), field.width));
      }
    }
    layout.fields.push_back(std::move(field));
  }

  const uint32_t packed_size = std::max(kRowBytes, AlignUp(cursor, kRowBytes));
  if (declared_size != 0 && declared_size < cursor) {
    return fail(std::format("size {} is smaller than its fields ({} bytes)", declared_size, cursor));
  }
  layout.size_in_bytes = declared_size != 0 ? declared_size : packed_size;
  return layout;
}

}  // namespace dxp::detail
