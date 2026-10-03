#pragma once
#include <cstdint>
#include <dxp/sm6/ResourceTypes.hpp>
#include <dxp/sm6/step/ApplyRuleStep.hpp>
#include <expected>
#include <glaze/glaze.hpp>
#include <span>
#include <string>
#include <unordered_map>
#include "dxp/Condition_impl.hpp"
#include "dxp/sm6/ExecutionContext.hpp"
#include "dxp/StepConcept.hpp"
#include "dxp/StepResults.hpp"
#include "dxp/ValidationContext.hpp"

namespace dxp::sm6::step {

/// @brief Repeat configuration — literal count with per-iteration typed params.
struct RepeatData {
  uint32_t times = 1;
  struct ParamArray {
    std::vector<uint32_t> u32;
    std::vector<int32_t> i32;
    std::vector<uint64_t> u64;
    std::vector<int64_t> i64;
    std::vector<float> f32;
    std::vector<double> f64;
  };
  std::unordered_map<std::string, ParamArray> params;
};

/// @brief Execute the ApplyRuleStep against the shader program.
/// @param step The step to execute.
/// @param ctx Execution context containing the shader program.
/// @return Results with match/applied counts, or error message.
std::expected<dxp::ApplyRuleResults, std::string> Execute(const ApplyRuleStep& step, ExecutionContext& ctx);

/// @brief Validate the ApplyRuleStep.
/// @param step The step to validate.
/// @param error Output error message on failure.
/// @param ctx Validation context.
/// @return void on success, error message on failure.
std::expected<void, std::string> Validate(const ApplyRuleStep& step, ValidationContext& ctx);
/// @brief Formats the step's result as a Trace log message.
std::string DescribeOutcome(const ApplyRuleStep& step, const dxp::ApplyRuleResults& results, const ExecutionContext& ctx);

struct MatchInstructionPatternData;

/// @brief Optional type guards for extractvalue aggregates/results (YAML form).
struct ValueTypePatternData {
  std::optional<ValueTypeKind> kind;
  /// Exact struct name (no leading %, case-sensitive). Optional = no constraint;
  /// explicitly-present-but-empty is rejected at Compile.
  std::optional<std::string> struct_name;
  std::optional<dxp::ComponentType> component_type;
};

/// @brief Exact LLVM extractvalue index path plus optional type guards (YAML form).
struct ExtractValuePatternData {
  std::vector<uint32_t> indices;
  std::optional<ValueTypePatternData> aggregate_type;
  std::optional<ValueTypePatternData> result_type;
};

/// @brief Captured integer index (YAML form of {capture: name}).
struct CaptureExtractIndexData {
  std::string capture;

  /// Comparison is required by glaze's `ordered` trait check on the index variant.
  auto operator<=>(const CaptureExtractIndexData&) const = default;
};

using EmitExtractIndexData = std::variant<uint32_t, CaptureExtractIndexData>;

/// @brief Emitted extractvalue specification (YAML form).
struct EmitExtractValueData {
  std::string aggregate;
  std::vector<EmitExtractIndexData> indices;
  std::optional<ValueTypePatternData> aggregate_type;
  std::optional<ValueTypePatternData> result_type;
};

/// @brief Operand pattern data — YAML-declarative form for match operands.
struct OperandPatternData {
  unsigned index = 0;
  std::optional<OperandKind> kind;
  std::string capture;
  std::string match_capture;
  std::unique_ptr<MatchInstructionPatternData> instruction;
  std::vector<std::variant<std::string, int64_t>> constant_int_values;
  std::vector<std::variant<std::string, double>> constant_float_values;
  std::optional<dxp::ComponentType> component_type;  ///< Optional: restrict constant matching to this type.
  std::optional<ResourceClass> resource_class;
  std::optional<ResourceKind> resource_kind;
  std::string resource_name;
  std::string resource_name_like;
  std::optional<int> register_index;
  std::optional<int> space;
  std::optional<std::string> export_as;

  [[nodiscard]] auto Compile() const -> std::expected<OperandPattern, std::string>;
};

/// @brief Emit operand pattern data — YAML-declarative form for emit operands.
struct EmitOperandPatternData {
  unsigned index = 0;
  std::optional<OperandKind> kind;
  std::string capture;
  std::string handle;
  std::unique_ptr<MatchInstructionPatternData> instruction;
  std::vector<std::variant<std::string, int64_t>> constant_int_values;
  std::vector<std::variant<std::string, double>> constant_float_values;
  std::optional<dxp::ComponentType> component_type;  ///< Optional: emit the constant with this type.

  [[nodiscard]] auto Compile() const -> std::expected<EmitOperand, std::string>;
};

/// @brief Match instruction pattern data — YAML-declarative form for matching DXIL instructions.
struct MatchInstructionPatternData {
  std::string opcode;
  /// Canonical extractvalue specification (v1+).
  std::optional<ExtractValuePatternData> extract;
  /// Legacy v1 spelling (single index); normalized to extract at Compile.
  std::optional<uint32_t> extract_index;
  std::string capture;
  std::string match_capture;
  std::vector<OperandPatternData> operands;

  [[nodiscard]] auto Compile() const -> std::expected<InstructionPattern, std::string>;
};

/// @brief Emit instruction pattern data — YAML-declarative form for emitting DXIL instructions.
struct EmitPatternData {
  std::string opcode;
  std::string name;
  std::vector<EmitOperandPatternData> operands;
  std::optional<ComponentType> result_component_type;
  std::optional<std::string> cast_opcode;
  /// Legacy v1 aggregate name; normalized to extract at Compile.
  std::string aggregate;
  /// Legacy v1 single index (default 0); normalized to extract at Compile.
  std::optional<uint32_t> extract_index;
  /// Canonical extractvalue specification (v1+).
  std::optional<EmitExtractValueData> extract;
  std::string capture;
  std::string replace_captured;
  std::string template_name;           ///< Template instantiation (mutually exclusive with opcode/capture).
  std::unique_ptr<RepeatData> repeat;  ///< Per-iteration repeat configuration.

  [[nodiscard]] auto Compile() const -> std::expected<EmitPattern, std::string>;
};

/// @brief A single rewrite rule (YAML-declarative form).
struct RuleData {
  std::string name;
  std::vector<MatchInstructionPatternData> match;
  std::vector<EmitPatternData> emit;
  /// Deprecated no-op: per-rule dead-code pruning was removed (DCE runs at
  /// serialization). Parsed for v1 recipe compatibility; ignored at Compile.
  /// Default kept at `true` per prior SM6 documentation for schema-level
  /// compatibility (generated schema shows no default change).
  bool prune = true;

  /**
   * @brief Compile this YAML data into a Rule.
   * @return Compiled rule or error message.
   */
  [[nodiscard]] auto Compile() const -> std::expected<Rule, std::string>;
};

/// @brief Step that applies a single inline rule.
struct ApplyRuleData {
  std::string name;
  RewriteKind rewrite_mode = RewriteKind::Replace;
  MatchKind match_mode = MatchKind::First;
  bool required = true;
  ::dxp::ConditionData condition;
  int32_t insert_index = -1;
  int32_t range_start_offset = 0;
  int32_t range_end_offset = -1;
  RuleData rule;

  /**
   * @brief Compile this YAML data into an ApplyRuleStep.
   * @return Compiled step or error message.
   */
  auto Compile() const -> std::expected<ApplyRuleStep, std::string>;
};

}  // namespace dxp::sm6::step

namespace glz {

template <>
struct meta<dxp::sm6::step::RewriteKind> {
  static constexpr auto value = enumerate("none", dxp::sm6::step::RewriteKind::None, "replace", dxp::sm6::step::RewriteKind::Replace, "before", dxp::sm6::step::RewriteKind::Before, "after", dxp::sm6::step::RewriteKind::After, "replace_range", dxp::sm6::step::RewriteKind::ReplaceRange);
};

template <>
struct meta<dxp::sm6::step::MatchKind> {
  static constexpr auto value = enumerate("first", dxp::sm6::step::MatchKind::First, "last", dxp::sm6::step::MatchKind::Last, "match_all", dxp::sm6::step::MatchKind::MatchAll);
};

template <>
struct meta<dxp::sm6::step::OperandKind> {
  static constexpr auto value = enumerate("undefined", dxp::sm6::step::OperandKind::Undefined, "call", dxp::sm6::step::OperandKind::Call, "constant", dxp::sm6::step::OperandKind::Constant, "resource", dxp::sm6::step::OperandKind::Resource);
};

template <>
struct meta<dxp::sm6::step::ValueTypeKind> {
  static constexpr auto value = enumerate("scalar", dxp::sm6::step::ValueTypeKind::Scalar, "vector", dxp::sm6::step::ValueTypeKind::Vector, "array", dxp::sm6::step::ValueTypeKind::Array, "struct", dxp::sm6::step::ValueTypeKind::Struct);
};

template <>
struct meta<dxp::sm6::step::ApplyRuleData> {
  static constexpr auto validate = [](const auto& self, std::string& error) {
    if (self.rule.name.empty()) {
      error = "apply_rule step '" + self.name + "': rule name must be specified";
    }
  };
};

template <>
struct meta<dxp::sm6::step::EmitPatternData> {
  using T = dxp::sm6::step::EmitPatternData;
  static constexpr auto value = object("opcode", &T::opcode, "name", &T::name,
                                       "operands", &T::operands, "result_component_type", &T::result_component_type,
                                       "cast_opcode", &T::cast_opcode, "aggregate", &T::aggregate,
                                       "extract_index", &T::extract_index, "extract", &T::extract,
                                       "capture", &T::capture,
                                       "replace_captured", &T::replace_captured, "template", &T::template_name,
                                       "repeat", &T::repeat);
};

}  // namespace glz
