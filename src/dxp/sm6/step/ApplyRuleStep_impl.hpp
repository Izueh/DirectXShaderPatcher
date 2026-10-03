#pragma once

#include "dxp/Condition_impl.hpp"
#include "dxp/sm6/ExecutionContext.hpp"
#include "dxp/sm6/step/ApplyRuleStep.hpp"
#include "dxp/sm6/step/common/Rule_impl.hpp"
#include "dxp/StepConcept.hpp"
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
std::expected<void, std::string> Validate(const ApplyRuleStep& step, ValidationContext& ctx);
/// @brief Formats the step's result as a Trace log message.
std::string DescribeOutcome(const ApplyRuleStep& step, const dxp::ApplyRuleResults& results, const ExecutionContext& ctx);

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
struct meta<dxp::sm6::step::ApplyRuleData> {
  static constexpr auto validate = [](const auto& self, std::string& error) {
    if (self.rule.name.empty()) {
      error = "apply_rule step '" + self.name + "': rule name must be specified";
    }
  };
};

}  // namespace glz
