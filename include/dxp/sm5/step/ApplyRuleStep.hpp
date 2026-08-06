#pragma once
#include <bit>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include <array>

#include "value_types/indirect.h"

#include "dxp/Condition.hpp"
#include "dxp/sm5/Model.hpp"
#include "dxp/StepResults.hpp"

namespace dxp::sm5::step {

/// @brief Step that applies a single SM5 rewrite rule. All rule/pattern types are
/// nested here so the step namespace contains only the step structs themselves.
struct ApplyRuleStep {
  static constexpr std::string_view kind = "apply_rule";
  using Results = dxp::ApplyRuleResults;

  /// @brief Controls which match is rewritten when a rule matches more than once.
  enum class MatchKind : std::uint8_t {
    First,
    Last,
    MatchAll,
  };

  /// @brief Selects how replacement instructions are applied.
  enum class RewriteKind : std::uint8_t {
    None,
    Replace,
    Before,
    After,
    ReplaceRange,
  };

  /// @brief Encoding used for one index slot in a recipe operand pattern.
  enum class OperandIndexRepresentation : std::uint8_t {
    Immediate32,              ///< 32-bit immediate index
    Immediate64,              ///< 64-bit immediate index (two DWORDs)
    Relative,                 ///< Relative addressing via a sub-operand
    Immediate32PlusRelative,  ///< 32-bit immediate plus relative
    Immediate64PlusRelative,  ///< 64-bit immediate plus relative
  };

  /// @brief Typed-array context for a variable-backed immediate (immediates_u32 etc.).
  /// The target width/interpretation for resolving a runtime variable's value into
  /// immediate bytes at emit time. Only set on variable-backed index entries.
  ///
  /// Distinct from @c dxp::ComponentType (the shared scalar-type vocabulary, a full mirror of
  /// @c hlsl::DXIL::ComponentType): index immediates are 32/64-bit dwords, so this
  /// family only carries the types the emit path can actually encode.
  enum class IndexImmediateType : std::uint8_t {
    None = 0,
    U32 = 1,
    U64 = 2,
    I32 = 3,
    I64 = 4,
    F32 = 5,
    F64 = 6,
  };

  /// @brief Forward declaration - operand pattern (defined after index pattern).
  struct OperandPattern;

  /// @brief Describes one ordered index slot in an operand pattern.
  struct OperandIndexPattern {
    bool any = false;
    OperandIndexRepresentation representation = OperandIndexRepresentation::Immediate32;
    std::optional<uint32_t> immediate_lo;
    std::optional<uint32_t> immediate_hi;
    std::optional<xyz::indirect<OperandPattern>> relative_operand;  ///< Deep-copying, uniquely-owning relative operand (std::indirect-style).
    std::string capture;
    std::string match_capture;
    std::string immediate_lo_variable;
    std::string immediate_hi_variable;
    IndexImmediateType immediate_family = IndexImmediateType::None;
    // No user-declared constructors: implicit copy deep-copies via optional<indirect>, and the
    // struct stays an aggregate so it can be brace-initialized YAML-style from the C++ API.
  };

  /// @brief Describes one operand in a declarative recipe pattern or template.
  /// Index slots are specified EITHER as manual binary `indices` (immediate_lo/immediate_hi,
  /// exact bytes) OR as typed immediates arrays (`immediates_u32` etc. — literal or variable
  /// name entries). The typed arrays mirror the YAML surface and resolve at match/emit time.
  struct OperandPattern {
    bool any = false;
    std::optional<OperandType> type;
    /// Manual index construction — binary only: immediate_lo/immediate_hi exact bytes.
    std::vector<OperandIndexPattern> indices;
    /// Typed immediates shorthand — each entry is a literal of the array's type, or a
    /// std::string variable name resolved at match/emit time using the array's type.
    std::vector<std::variant<std::string, uint32_t>> immediates_u32;
    std::vector<std::variant<std::string, uint64_t>> immediates_u64;
    std::vector<std::variant<std::string, int32_t>> immediates_i32;
    std::vector<std::variant<std::string, int64_t>> immediates_i64;
    std::vector<std::variant<std::string, float>> immediates_f32;
    std::vector<std::variant<std::string, double>> immediates_f64;
    std::string from_handle;
    OperandIndexPattern element_index;
    std::string mask;
    std::string swizzle;
    std::string select;
    int32_t num_components = -1;
    std::optional<OperandModifier> modifier;
    std::string capture;
    std::string match_capture;
    std::optional<std::string> export_as;
    Components components;

    /// @brief Pure conversion (no validation): resolves this operand's index slot patterns.
    /// If manual `indices` are set they are returned as-is; otherwise the typed immediates
    /// arrays are expanded into index patterns (representation driven by the array type,
    /// literal entries materialized to bytes, variable entries kept for runtime resolution).
    [[nodiscard]] std::vector<OperandIndexPattern> IndexPatterns() const;
  };

  /// @brief Extended opcode match pattern — enum name or raw 32-bit token.
  using ExtendedOpcodePattern = std::variant<ExtendedOpcodeType, uint32_t>;

  /// @brief Describes one instruction pattern for rule matching.
  struct InstructionPattern {
    std::optional<Opcode> opcode;
    std::string capture;
    std::optional<bool> saturate;
    std::optional<InterpolationMode> interpolation_mode;
    int32_t test_boolean = -1;
    std::vector<OperandPattern> operands;
    std::optional<std::vector<ExtendedOpcodePattern>> extended_opcodes;
  };

  /// @brief Describes one instruction emitted by a rewrite rule.
  struct EmitPattern {
    std::optional<Opcode> opcode;
    std::optional<bool> saturate;
    std::optional<InterpolationMode> interpolation_mode;
    int32_t test_boolean = -1;
    std::vector<OperandPattern> operands;
    std::string capture;
  };

  /// @brief Describes one SM5 rewrite rule.
  struct Rule {
    std::vector<InstructionPattern> match_patterns;
    std::vector<EmitPattern> emit_patterns;
    int32_t range_start_offset = 0;
    int32_t range_end_offset = -1;
    int32_t insert_relative_index = -1;
  };

  std::string name;
  bool required = true;
  RewriteKind rewrite_mode = RewriteKind::Replace;
  std::optional<ConditionNode> condition;
  Rule rule;
  MatchKind match_mode = MatchKind::First;

  ApplyRuleStep(std::string name_val, bool required, RewriteKind rewrite_mode_val, std::optional<ConditionNode> condition_val, Rule rule_val, MatchKind match_kind = MatchKind::First)
      : name(std::move(name_val)), required(required), rewrite_mode(rewrite_mode_val), condition(std::move(condition_val)), rule(std::move(rule_val)), match_mode(match_kind) {}
};

inline std::vector<ApplyRuleStep::OperandIndexPattern> ApplyRuleStep::OperandPattern::IndexPatterns() const {
  if (!indices.empty()) {
    return indices;
  }
  std::vector<OperandIndexPattern> result;
  auto process = [&result](const auto& array, ApplyRuleStep::IndexImmediateType family,
                           ApplyRuleStep::OperandIndexRepresentation representation) {
    for (const auto& val : array) {
      OperandIndexPattern pattern;
      pattern.representation = representation;
      std::visit(
          [&pattern, family](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::string>) {
              pattern.immediate_lo_variable = v;
              pattern.immediate_family = family;
            } else if constexpr (std::is_same_v<T, float>) {
              pattern.immediate_lo = std::bit_cast<uint32_t>(v);
            } else if constexpr (std::is_same_v<T, double>) {
              const auto bits = std::bit_cast<uint64_t>(v);
              pattern.immediate_lo = static_cast<uint32_t>(bits);
              pattern.immediate_hi = static_cast<uint32_t>(bits >> 32U);
            } else {
              using U = std::make_unsigned_t<T>;
              const auto value = std::bit_cast<U>(v);
              pattern.immediate_lo = static_cast<uint32_t>(value);
              if constexpr (sizeof(U) > 4U) {
                pattern.immediate_hi = static_cast<uint32_t>(value >> 32U);
              }
            }
          },
          val);
      result.push_back(std::move(pattern));
    }
  };
  process(immediates_u32, ApplyRuleStep::IndexImmediateType::U32, ApplyRuleStep::OperandIndexRepresentation::Immediate32);
  process(immediates_u64, ApplyRuleStep::IndexImmediateType::U64, ApplyRuleStep::OperandIndexRepresentation::Immediate64);
  process(immediates_i32, ApplyRuleStep::IndexImmediateType::I32, ApplyRuleStep::OperandIndexRepresentation::Immediate32);
  process(immediates_i64, ApplyRuleStep::IndexImmediateType::I64, ApplyRuleStep::OperandIndexRepresentation::Immediate64);
  process(immediates_f32, ApplyRuleStep::IndexImmediateType::F32, ApplyRuleStep::OperandIndexRepresentation::Immediate32);
  process(immediates_f64, ApplyRuleStep::IndexImmediateType::F64, ApplyRuleStep::OperandIndexRepresentation::Immediate64);
  return result;
}

/// @brief The recipe index-representation vocabulary maps 1:1 onto the bytecode
/// representation. Kept as separate enums (YAML surface vs DXBC token format);
/// this assert guarantees they never drift.
static_assert(static_cast<uint8_t>(::dxp::sm5::Operand::IndexRepresentation::Immediate32) == static_cast<uint8_t>(ApplyRuleStep::OperandIndexRepresentation::Immediate32));
static_assert(static_cast<uint8_t>(::dxp::sm5::Operand::IndexRepresentation::Immediate64) == static_cast<uint8_t>(ApplyRuleStep::OperandIndexRepresentation::Immediate64));
static_assert(static_cast<uint8_t>(::dxp::sm5::Operand::IndexRepresentation::Relative) == static_cast<uint8_t>(ApplyRuleStep::OperandIndexRepresentation::Relative));
static_assert(static_cast<uint8_t>(::dxp::sm5::Operand::IndexRepresentation::Immediate32PlusRelative) == static_cast<uint8_t>(ApplyRuleStep::OperandIndexRepresentation::Immediate32PlusRelative));
static_assert(static_cast<uint8_t>(::dxp::sm5::Operand::IndexRepresentation::Immediate64PlusRelative) == static_cast<uint8_t>(ApplyRuleStep::OperandIndexRepresentation::Immediate64PlusRelative));

}  // namespace dxp::sm5::step
