#pragma once
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "dxp/Condition.hpp"
#include "dxp/sm5/step/common/Rule.hpp"
#include "dxp/StepResults.hpp"

namespace dxp::sm5::step {

/// @brief Step that applies a single SM5 rewrite rule. All rule/pattern types are
/// in Rule.hpp so the step namespace contains only the step struct itself.
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
  std::optional<MatchBlob> match_blob;  ///< XOR with rule.match_patterns and scope.
  std::optional<EmitBlob> emit_blob;    ///< Only valid with match_blob.
  std::string scope;                    ///< Name of a stored blob this step's rule runs against (XOR with match/match_blob).

  ApplyRuleStep(std::string name_val, bool required, RewriteKind rewrite_mode_val, std::optional<ConditionNode> condition_val, Rule rule_val, MatchKind match_kind = MatchKind::First)
      : name(std::move(name_val)), required(required), rewrite_mode(rewrite_mode_val), condition(std::move(condition_val)), rule(std::move(rule_val)), match_mode(match_kind) {}
};

}  // namespace dxp::sm5::step
