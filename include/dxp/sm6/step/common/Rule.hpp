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
#include <variant>
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

/// @brief LLVM type kind for extractvalue type guards.
enum class ValueTypeKind : std::uint8_t {
  Scalar,
  Vector,
  Array,
  Struct,
};

/// @brief Optional type guards for extractvalue aggregates/results (v1 minimal set).
struct ValueTypePattern {
  std::optional<ValueTypeKind> kind;
  /// Exact, case-sensitive LLVM identified-struct name, without the leading '%'.
  std::optional<std::string> struct_name;
  std::optional<dxp::ComponentType> component_type;
};

/// @brief Exact LLVM extractvalue index path plus optional type guards.
struct ExtractValuePattern {
  std::vector<uint32_t> indices;
  std::optional<ValueTypePattern> aggregate_type;
  std::optional<ValueTypePattern> result_type;
};

/// @brief Captured integer index for emitted extractvalue paths.
struct CaptureExtractIndex {
  std::string capture;
};

using EmitExtractIndex = std::variant<uint32_t, CaptureExtractIndex>;

/// @brief Emitted extractvalue specification.
struct EmitExtractValue {
  std::string aggregate;
  std::vector<EmitExtractIndex> indices;
  std::optional<ValueTypePattern> aggregate_type;
  std::optional<ValueTypePattern> result_type;
};

/// @brief Operand pattern — delegates instruction logic to InstructionPattern.
struct OperandPattern {
  uint32_t operand_index = 0;
  std::optional<OperandKind> kind;
  std::string capture_name;
  std::string match_capture;
  std::optional<xyz::indirect<InstructionPattern>> instruction;  ///< Deep-copying, uniquely-owning nested instruction (std::indirect-style).
  /// @brief Integer literals or env names; one entry broadcasts to the operand width.
  std::vector<std::variant<std::string, int64_t>> constant_int_values;
  /// @brief Floating-point literals or env names, converted at operand precision.
  std::vector<std::variant<std::string, double>> constant_float_values;
  /// @brief Constant type guard; unsigned types select unsigned literal comparisons.
  std::optional<dxp::ComponentType> component_type;
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
  /// Optional exact LLVM extractvalue index path with optional type guards.
  /// Only valid with opcode 'extractvalue'; aggregate binding uses operand 0.
  std::optional<ExtractValuePattern> extract;
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
  /// @brief Integer literals or env names; one entry broadcasts to the operand width.
  std::vector<std::variant<std::string, int64_t>> constant_int_values;
  /// @brief Floating-point literals or env names, converted at operand precision.
  std::vector<std::variant<std::string, double>> constant_float_values;
  /// @brief Explicit operand type; required for constant and undefined cast sources.
  std::optional<dxp::ComponentType> component_type;
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
  /// @brief Scalar result/overload type; supports I1, 8/16/32/64-bit integers and F16/F32/F64.
  std::optional<dxp::ComponentType> result_component_type;
  /// Optional extractvalue emission (mutually exclusive with opcode/capture/operands).
  std::optional<EmitExtractValue> extract;
  std::string name;
  std::string capture;
  std::string replace_captured;
  /// @brief Calls a function declared by an earlier declare_function step.
  std::string function_name;
};

/// @brief Describes one DXIL rewrite rule.
///
/// @c name carries the YAML `rule.name` through to execution so the step can publish
/// `state[<rule name>]` alongside `state[<step name>]`, matching the SM5 backend.
struct Rule {
  std::vector<InstructionPattern> match_patterns;
  std::vector<EmitPattern> emit_patterns;
  std::string name;
};

}  // namespace dxp::sm6::step
