#pragma once
#include <algorithm>
#include <bit>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

#include <array>

#include "value_types/indirect.h"

#include "dxp/sm5/Model.hpp"

namespace dxp::sm5::step {

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
enum class IndexImmediateType : std::uint8_t {
  None = 0,
  U32 = 1,
  U64 = 2,
  I32 = 3,
  I64 = 4,
  F32 = 5,
  F64 = 6,
};

/// @brief Alias for IndexImmediateType — used in legacy code paths.
using ImmediateFamily = IndexImmediateType;

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
  BeforeLastReturn,  ///< Anchor = last ret/retc in the program; match patterns optional (usable as a guard). Designed for blob insertion.
};

// Forward declarations
struct OperandPattern;
struct InstructionPattern;
struct EmitPattern;

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
  std::optional<model::OperandType> type;
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
  struct Handle {
    std::string name;
    std::optional<std::variant<std::string, uint32_t>> element_index;
  };
  std::optional<Handle> handle;

  /// @brief Match-only constraint: the operand's register must resolve to a
  /// declaration matching all specified fields. Missing declaration = no match.
  struct DeclConstraint {
    std::optional<model::ResourceDimension> dimension;
    std::array<std::optional<model::ResourceReturnType>, 4> return_type;
    std::optional<uint32_t> structure_stride;
    std::optional<model::SamplerMode> mode;
    std::optional<model::CbufferAccessPattern> access_pattern;
    std::optional<model::SignatureSemantic> semantic;
    std::optional<model::InterpolationMode> interpolation;

    [[nodiscard]] bool AnyFieldSet() const {
      return dimension.has_value() || structure_stride.has_value() || mode.has_value()
             || access_pattern.has_value() || semantic.has_value() || interpolation.has_value()
             || std::any_of(return_type.begin(), return_type.end(), [](const auto& t) { return t.has_value(); });
    }
  };
  std::optional<DeclConstraint> decl;
  std::string mask;
  std::string swizzle;
  std::string select;
  int32_t num_components = -1;
  std::optional<model::OperandModifier> modifier;
  std::string capture;
  std::string match_capture;
  std::optional<std::string> export_as;
  /// @brief Explicit component spec (presence = the user specified `components:`).
  /// Capture operands inherit their component mode from the captured operand at
  /// runtime; temp-handle operands must specify it explicitly (Validate).
  std::optional<model::Components> components;

  /// @brief Pure conversion (no validation): resolves this operand's index slot patterns.
  /// If manual `indices` are set they are returned as-is; otherwise the typed immediates
  /// arrays are expanded into index patterns (representation driven by the array type,
  /// literal entries materialized to bytes, variable entries kept for runtime resolution).
  [[nodiscard]] std::vector<OperandIndexPattern> IndexPatterns() const;
};

inline std::vector<OperandIndexPattern> OperandPattern::IndexPatterns() const {
  if (!indices.empty()) {
    return indices;
  }
  std::vector<OperandIndexPattern> result;
  auto process = [&result](const auto& array, IndexImmediateType family,
                           OperandIndexRepresentation representation) {
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
  process(immediates_u32, IndexImmediateType::U32, OperandIndexRepresentation::Immediate32);
  process(immediates_u64, IndexImmediateType::U64, OperandIndexRepresentation::Immediate64);
  process(immediates_i32, IndexImmediateType::I32, OperandIndexRepresentation::Immediate32);
  process(immediates_i64, IndexImmediateType::I64, OperandIndexRepresentation::Immediate64);
  process(immediates_f32, IndexImmediateType::F32, OperandIndexRepresentation::Immediate32);
  process(immediates_f64, IndexImmediateType::F64, OperandIndexRepresentation::Immediate64);
  return result;
}

/// @brief Extended-opcode match entry.
///   - Any: position wildcard (matches any token at this position);
///   - Raw: exact 32-bit token match;
///   - Type: type-bits match, optionally refined by structured payload
///     expectations (sample_controls offsets, resource dimension/stride,
///     per-component return types) compared against the decoded token.
struct ExtendedOpcodePattern {
  enum class Kind : std::uint8_t {
    Any = 0,
    Raw = 1,
    Type = 2,
  };

  Kind kind = Kind::Any;
  model::ExtendedOpcodeType type = model::ExtendedOpcodeType::Empty;  ///< Kind::Type.
  uint32_t raw = 0;                                                   ///< Kind::Raw.
  std::optional<model::SampleControlsPayload> sample_controls;        ///< Type-kind payload expectations.
  std::optional<model::ResourceDimPayload> resource_dim;
  std::optional<model::ResourceReturnTypePayload> resource_return_type;
};

/// @brief Describes one instruction pattern for rule matching.
struct InstructionPattern {
  std::optional<model::Opcode> opcode;
  std::string capture;
  std::optional<bool> saturate;
  std::optional<model::InterpolationMode> interpolation_mode;
  int32_t test_boolean = -1;
  std::vector<OperandPattern> operands;
  std::optional<std::vector<ExtendedOpcodePattern>> extended_opcodes;
  std::optional<model::ResourceDimension> dimension;
  std::array<std::optional<model::ResourceReturnType>, 4> return_type;
  uint32_t structure_stride = 0;
  std::optional<model::CbufferAccessPattern> access_pattern;
  std::optional<model::SamplerMode> mode;
  uint32_t uav_flags = 0;
};

/// @brief Extended-opcode emit entry.
///   - Raw: exact token value (bits 30:00; the engine assigns the chaining
///     bit 31 from the final chain position);
///   - Type: typed token with structured payload. A type entry emits exactly
///     the given payload verbatim; members of the canonical resource pair
///     (ResourceDim + ResourceReturnType) that the entry chain omits are
///     synthesized from the resource declaration (or fixed metadata for
///     ld_raw/ld_structured) at emit time, in canonical order.
struct EmitExtendedOpcode {
  enum class Kind : std::uint8_t {
    Type = 0,
    Raw = 1,
  };

  Kind kind = Kind::Type;
  model::ExtendedOpcodeType type = model::ExtendedOpcodeType::Empty;  ///< Kind::Type.
  uint32_t raw = 0;                                                   ///< Kind::Raw.
  std::optional<model::SampleControlsPayload> sample_controls;        ///< Kind::Type payloads.
  std::optional<model::ResourceDimPayload> resource_dim;
  std::optional<model::ResourceReturnTypePayload> resource_return_type;
};

/// @brief Repeat configuration for template/emit repetition.
struct RepeatConfig {
  uint32_t times = 1;
  struct ParamValue {
    std::vector<uint32_t> u32;
    std::vector<int32_t> i32;
    std::vector<uint64_t> u64;
    std::vector<int64_t> i64;
    std::vector<float> f32;
    std::vector<double> f64;
  };
  std::unordered_map<std::string, ParamValue> params;
};

/// @brief Describes one instruction emitted by a rewrite rule. Exactly one
/// source per entry: an explicit `opcode`, a replayed instruction `capture`,
/// a stored blob expansion (`blob`), or a template instantiation
/// (`template_name`).
struct EmitPattern {
  std::optional<model::Opcode> opcode;
  std::optional<bool> saturate;
  std::optional<model::InterpolationMode> interpolation_mode;
  int32_t test_boolean = -1;
  std::vector<OperandPattern> operands;
  std::string capture;
  std::string blob;                    ///< Expand a stored blob (post-mutation copy, deep-copied on emit).
  std::string template_name;           ///< Template instantiation (mutually exclusive with opcode/capture/blob).
  std::optional<RepeatConfig> repeat;  ///< Per-iteration repeat; on template entries it overrides the template's declared repeat.
  std::vector<EmitExtendedOpcode> extended_opcodes;
  std::optional<model::ResourceDimension> dimension;
  std::array<std::optional<model::ResourceReturnType>, 4> return_type;
  uint32_t structure_stride = 0;
  std::optional<model::CbufferAccessPattern> access_pattern;
  std::optional<model::SamplerMode> mode;
  uint32_t uav_flags = 0;
};

/// @brief Describes one SM5 rewrite rule.
struct Rule {
  std::vector<InstructionPattern> match_patterns;
  std::vector<EmitPattern> emit_patterns;

  // Per-rule execution modes — honored by blob/scope scoped execution. Plain
  // match steps use the step-level fields instead (these default there).
  MatchKind match_mode = MatchKind::First;
  RewriteKind rewrite_mode = RewriteKind::Replace;
  int32_t insert_index = -1;
  int32_t range_start_offset = 0;
  int32_t range_end_offset = -1;
};

/// @brief Blob window capture: a [match_start .. match_end] (inclusive) window
/// stored as an instruction sequence under `capture`. Present on the step, it
/// scopes the step's rule to the captured interior (same as `scope:` on a
/// previously captured blob).
struct MatchBlob {
  InstructionPattern match_start;
  InstructionPattern match_end;
  std::string capture;
};

/// @brief What happens to the captured window after interior rewriting.
///   - None (default): store updated with the post-mutation copy; shader untouched.
///   - Replace: transformed blob replaces the window (same execution pass).
///   - Before / After: transformed blob inserted before window start / after
///     window end; the original window is preserved.
struct EmitBlob {
  enum class Mode : std::uint8_t {
    None = 0,
    Replace = 1,
    Before = 2,
    After = 3,
  };

  Mode mode = Mode::None;
};

}  // namespace dxp::sm5::step
