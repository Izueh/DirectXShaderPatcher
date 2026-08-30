#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <expected>
#include <functional>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "dxp/Condition.hpp"
#include "dxp/sm6/ResourceTypes.hpp"
#include "dxp/StepResults.hpp"
#include "value_types/indirect.h"

namespace dxp::sm6::step {

/// @brief Controls which DXIL match is rewritten when a rule matches more than once.
enum class MatchKind : std::uint8_t {
  First,
  Last,
  MatchAll,
};

/// @brief Identifies the kind of operand (used for both matching and emit).
enum class OperandKind : std::uint8_t {
  Undefined,  ///< LLVM undef type (emit only).
  Call,       ///< Call/instruction operand.
  Constant,   ///< Constant value operand.
  Resource    ///< Resource handle operand.
};

struct Rule;
struct InstructionPattern;
struct EmitOperand;

/// @brief Operand pattern — delegates instruction logic to InstructionPattern.
struct OperandPattern {
  uint32_t operand_index = 0;
  std::optional<OperandKind> kind;
  std::string capture_name;
  std::string match_capture;
  std::optional<xyz::indirect<InstructionPattern>> instruction;  ///< Deep-copying, uniquely-owning nested instruction (std::indirect-style).
  std::vector<int64_t> constant_int_values;
  std::vector<double> constant_float_values;
  std::optional<dxp::ComponentType> component_type;  ///< Optional: restrict constant matching to this type.
  std::optional<ResourceClass> resource_class;
  std::optional<ResourceKind> resource_kind;
  std::optional<std::string> resource_name;
  std::optional<std::string> resource_name_like_pattern;
  std::optional<int> resource_register_index;
  std::optional<int> resource_space;
  std::optional<std::string> export_as;

  // No user-declared constructors: implicit copy deep-copies via optional<indirect>, and
  // the struct stays an aggregate so it can be brace-initialized YAML-style from the C++ API.
};

struct InstructionPattern {
  uint32_t operand_index = 0;
  std::optional<std::string> callee_name;
  std::optional<std::string> opcode;
  std::string capture_name;
  std::string match_capture;
  std::vector<OperandPattern> operand_patterns;
};

/// @brief Selects how a DXIL rewrite is applied.
/// Member order matches dxp::sm5::step::ApplyRuleStep::RewriteKind so both backends share the same
/// underlying values; YAML keys are unchanged.
enum class RewriteKind : std::uint8_t {
  None,
  Replace,
  Before,
  After,
  ReplaceRange,
};

/// @brief Describes one operand used by emitted rewrite code.
struct EmitOperand {
  uint32_t operand_index = 0;
  OperandKind kind = OperandKind::Call;

  std::optional<std::string> capture;
  std::vector<int64_t> constant_int_values;
  std::vector<double> constant_float_values;
  std::optional<dxp::ComponentType> component_type;  ///< Optional: emit the constant with this type (default i32 / f32).
  std::string handle;
  std::optional<xyz::indirect<InstructionPattern>> instruction;  ///< Deep-copying, uniquely-owning nested instruction (std::indirect-style).

  // No user-declared constructors: the struct stays an aggregate so it can be
  // brace-initialized YAML-style from the C++ API.
};

/// @brief Emit instruction pattern — for generating IR.
struct EmitPattern {
  uint32_t operand_index = 0;
  std::optional<std::string> callee_name;
  std::optional<std::string> opcode;
  std::optional<std::string> cast_opcode;
  std::string capture_name;
  std::vector<EmitOperand> operands;
  std::optional<dxp::ComponentType> result_component_type;
  uint32_t extract_index = 0;
  std::string aggregate;
  std::string name;
  std::string capture;
  std::string replace_captured;
};

/// @brief Describes one DXIL rewrite rule.
struct Rule {
  std::vector<InstructionPattern> match_patterns;
  std::vector<EmitPattern> emit_patterns;
  bool prune_dead_instructions = true;
};

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
