#pragma once
#include <dxp/sm6/ResourceTypes.hpp>
#include <dxp/sm6/step/ApplyRuleStep.hpp>
#include <glaze/glaze.hpp>
#include "dxp/Condition_impl.hpp"
#include "dxp/sm6/ExecutionContext.hpp"
#include "dxp/StepConcept.hpp"
#include "dxp/StepResults.hpp"
#include "dxp/ValidationContext.hpp"

namespace dxp::sm6::step {

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
std::expected<void, std::string> Validate(const ApplyRuleStep& step, std::string& error, ValidationContext& ctx);
/// @brief Formats the step's result as a Trace log message.
std::string DescribeOutcome(const ApplyRuleStep& step, const dxp::ApplyRuleResults& results, const ExecutionContext& ctx);

struct MatchInstructionPatternData;

/// @brief Operand pattern data — YAML-declarative form for match operands.
struct OperandPatternData {
  unsigned index = 0;
  std::optional<OperandKind> kind;
  std::string capture;
  std::string match_capture;
  std::unique_ptr<MatchInstructionPatternData> instruction;
  std::vector<int64_t> constant_int_values;
  std::vector<double> constant_float_values;
  std::optional<dxp::ComponentType> component_type;  ///< Optional: restrict constant matching to this type.
  std::optional<ResourceClass> resource_class;
  std::optional<DxilResourceKind> resource_kind;
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
  std::vector<int64_t> constant_int_values;
  std::vector<double> constant_float_values;
  std::optional<dxp::ComponentType> component_type;  ///< Optional: emit the constant with this type.

  [[nodiscard]] auto Compile() const -> std::expected<EmitOperand, std::string>;
};

/// @brief Match instruction pattern data — YAML-declarative form for matching DXIL instructions.
struct MatchInstructionPatternData {
  std::string opcode;
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
  std::string aggregate;
  unsigned extract_index = 0;
  std::string capture;
  std::string replace_captured;

  [[nodiscard]] auto Compile() const -> std::expected<EmitPattern, std::string>;
};

/// @brief A single rewrite rule (YAML-declarative form).
struct RuleData {
  std::string name;
  std::vector<MatchInstructionPatternData> match;
  std::vector<EmitPatternData> emit;
  int32_t range_start_offset = 0;
  int32_t range_end_offset = -1;
  int32_t insert_index = -1;
  bool prune = false;

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
  RuleData rule;
  MatchKind match_mode = MatchKind::First;
  bool required = true;
  ::dxp::ConditionData condition;

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
  using T = dxp::sm6::step::RewriteKind;
  static constexpr auto keys = std::array{"none", "replace", "before", "after", "replace_range"};
  static constexpr auto value = std::array{dxp::sm6::step::RewriteKind::None, dxp::sm6::step::RewriteKind::Replace,
                                           dxp::sm6::step::RewriteKind::Before, dxp::sm6::step::RewriteKind::After,
                                           dxp::sm6::step::RewriteKind::ReplaceRange};
};

template <>
struct meta<dxp::sm6::step::MatchKind> {
  using T = dxp::sm6::step::MatchKind;
  static constexpr auto keys = std::array{"first", "last", "match_all"};
  static constexpr auto value = std::array{dxp::sm6::step::MatchKind::First, dxp::sm6::step::MatchKind::Last, dxp::sm6::step::MatchKind::MatchAll};
};

template <>
struct meta<dxp::sm6::step::OperandKind> {
  using T = dxp::sm6::step::OperandKind;
  static constexpr std::array keys = {"undefined", "call", "constant", "resource"};
  static constexpr std::array value = {T::Undefined, T::Call, T::Constant, T::Resource};
};

template <>
struct meta<dxp::sm6::step::ApplyRuleData> {
  static constexpr auto validate = [](const auto& self, std::string& error) {
    if (self.rule.name.empty()) {
      error = "apply_rule step '" + self.name + "': rule name must be specified";
    }
  };
};

}  // namespace glz
