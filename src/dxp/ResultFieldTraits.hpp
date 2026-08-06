#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <variant>

#include <dxp/StepResults.hpp>

namespace dxp {

/// @brief Trait: checks if a Results type has a given field name.
template <typename Results>
struct IsResultField;

/// @brief Trait: extracts a field value from a Results type.
template <typename Results>
struct ExtractFieldTrait;

/// @brief Builds a field-name validator for a recipe step's Results type.
/// Used to validate dot-notation field references (step.field) in conditions.
/// @param step Any recipe step (results type is used for field validation).
/// @return A function that checks if a field name is valid.
template <typename Step>
std::function<bool(std::string_view)> ValidateFields(const Step&) {
  using Results = typename Step::Results;
  Results dummy{};
  return [dummy](std::string_view field) {
    return dxp::ExtractFieldTrait<Results>::GetValue(dummy, field).has_value();
  };
}

template <typename Results>
struct IsResultField;

template <>
struct IsResultField<ApplyRuleResults> {
  static bool CheckValue(std::string_view field) {
    return field == "match_count" || field == "applied_count";
  }
};

template <>
struct IsResultField<AddResourceResults> {
  static bool CheckValue(std::string_view field) {
    return field == "textures_added" || field == "raw_resources_added" || field == "structured_resources_added" || field == "cbuffers_added" || field == "samplers_added" || field == "uavs_added" || field == "inputs_added" || field == "outputs_added" || field == "temps_added";
  }
};

template <>
struct IsResultField<CheckShaderVersionResults> {
  static bool CheckValue(std::string_view field) {
    return field == "major_version" || field == "minor_version";
  }
};

template <>
struct IsResultField<CheckOpcodeCountResults> {
  static bool CheckValue(std::string_view field) {
    return field == "opcode_counts" || field == "dxil_opcode_counts" || field == "llvm_opcode_counts" || field == "dcl_resource" || field == "dcl_sampler" || field == "dcl_cbuffer" || field == "dcl_temps" || field == "add" || field == "mov" || field == "mad" || field == "sample" || field == "ret" || field == "nop";
  }
};

template <>
struct IsResultField<CheckResourceCountResults> {
  static bool CheckValue(std::string_view field) {
    return field == "textures" || field == "samplers" || field == "cbuffers" || field == "uavs" || field == "thread_groups" || field == "total";
  }
};

/// @brief Trait: extracts a field value from a Results type.
template <typename Results>
struct ExtractFieldTrait;

template <>
struct ExtractFieldTrait<ApplyRuleResults> {
  static std::optional<PrimitiveValue> GetValue(const ApplyRuleResults& result, std::string_view field) {
    if (field == "match_count") return static_cast<int64_t>(result.match_count);
    if (field == "applied_count") return static_cast<int64_t>(result.applied_count);
    return std::nullopt;
  }
};

template <>
struct ExtractFieldTrait<AddResourceResults> {
  static std::optional<PrimitiveValue> GetValue(const AddResourceResults& result, std::string_view field) {
    if (field == "textures_added") return static_cast<int64_t>(result.textures_added);
    if (field == "raw_resources_added") return static_cast<int64_t>(result.raw_resources_added);
    if (field == "structured_resources_added") return static_cast<int64_t>(result.structured_resources_added);
    if (field == "cbuffers_added") return static_cast<int64_t>(result.cbuffers_added);
    if (field == "samplers_added") return static_cast<int64_t>(result.samplers_added);
    if (field == "uavs_added") return static_cast<int64_t>(result.uavs_added);
    if (field == "inputs_added") return static_cast<int64_t>(result.inputs_added);
    if (field == "outputs_added") return static_cast<int64_t>(result.outputs_added);
    if (field == "temps_added") return static_cast<int64_t>(result.temps_added);
    return std::nullopt;
  }
};

template <>
struct ExtractFieldTrait<CheckShaderVersionResults> {
  static std::optional<PrimitiveValue> GetValue(const CheckShaderVersionResults& result, std::string_view field) {
    if (field == "major_version") return static_cast<int64_t>(result.major_version);
    if (field == "minor_version") return static_cast<int64_t>(result.minor_version);
    return std::nullopt;
  }
};

// Note: the *_counts pseudo-fields below resolve to the map sizes (not the map
// contents). Dot-notation conditions should target per-opcode names (e.g.
// `count_ops.mov`) rather than `count_ops.opcode_counts`.
template <>
struct ExtractFieldTrait<CheckOpcodeCountResults> {
  static std::optional<PrimitiveValue> GetValue(const CheckOpcodeCountResults& result, std::string_view field) {
    if (field == "opcode_counts") return static_cast<int64_t>(result.opcode_counts.size());
    if (field == "dxil_opcode_counts") return static_cast<int64_t>(result.dxil_opcode_counts.size());
    if (field == "llvm_opcode_counts") return static_cast<int64_t>(result.llvm_opcode_counts.size());
    if (auto iter = result.opcode_counts.find(std::string(field)); iter != result.opcode_counts.end()) {
      return static_cast<int64_t>(iter->second);
    }
    if (auto iter = result.dxil_opcode_counts.find(std::string(field)); iter != result.dxil_opcode_counts.end()) {
      return static_cast<int64_t>(iter->second);
    }
    if (auto iter = result.llvm_opcode_counts.find(std::string(field)); iter != result.llvm_opcode_counts.end()) {
      return static_cast<int64_t>(iter->second);
    }
    return std::nullopt;
  }
};

template <>
struct ExtractFieldTrait<CheckResourceCountResults> {
  static std::optional<PrimitiveValue> GetValue(const CheckResourceCountResults& result, std::string_view field) {
    if (field == "textures") return static_cast<int64_t>(result.textures);
    if (field == "samplers") return static_cast<int64_t>(result.samplers);
    if (field == "cbuffers") return static_cast<int64_t>(result.cbuffers);
    if (field == "uavs") return static_cast<int64_t>(result.uavs);
    if (field == "thread_groups") return static_cast<int64_t>(result.thread_groups);
    if (field == "total") return static_cast<int64_t>(result.total);
    return std::nullopt;
  }
};

}  // namespace dxp
