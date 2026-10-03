#pragma once

#include "dxp/sm6/step/common/Rule.hpp"

namespace dxp::sm6::step {

/// @brief Step that applies a single DXIL rewrite rule.
struct ApplyRuleStep {
  static constexpr std::string_view kind = "apply_rule";
  using Results = dxp::ApplyRuleResults;
  std::string name;
  bool required = true;
  RewriteKind rewrite_mode = RewriteKind::Replace;
  std::optional<ConditionNode> condition;
  Rule rule;
  MatchKind match_mode = MatchKind::First;
  int32_t insert_index = -1;
  int32_t range_start_offset = 0;
  int32_t range_end_offset = -1;

  ApplyRuleStep(std::string name_val, bool required, RewriteKind rewrite_mode_val, std::optional<ConditionNode> condition_val, Rule rule_val, MatchKind match_kind = MatchKind::First)
      : name(std::move(name_val)), required(required), rewrite_mode(rewrite_mode_val), condition(std::move(condition_val)), rule(std::move(rule_val)), match_mode(match_kind) {
    // Order each emit pattern's operands by operand index so argument mapping is
    // deterministic regardless of how the step was constructed (YAML or programmatic
    // API). This is a one-time normalization at construction; Validate/Execute stay const.
    for (auto& emit_pattern : rule.emit_patterns) {
      std::ranges::sort(emit_pattern.operands,
                        [](const auto& a, const auto& b) { return a.operand_index < b.operand_index; });
    }
  }
};

}  // namespace dxp::sm6::step
