#pragma once
#include <dxp/sm5/step/common/Rule.hpp>
#include <glaze/glaze.hpp>
#include <memory>
#include <unordered_map>
#include "dxp/Condition_impl.hpp"
#include "dxp/sm5/ExecutionContext.hpp"
#include "dxp/sm5/Model_impl.hpp"
#include "dxp/StepConcept.hpp"
#include "dxp/StepResults.hpp"
#include "dxp/ValidationContext.hpp"

namespace dxp::sm5::step {

/// @brief Repeat configuration — literal count with per-iteration typed params.
///
/// Note: `ParamArray` stays a plain struct (not a std::variant) — Glaze's
/// variant mechanism cannot carry array-of-numbers alternatives in the YAML
/// surface: tagged variants require object alternatives (a tag key merged into
/// the alternative's object body), and untagged inference cannot distinguish
/// the six numeric-array types (`no_matching_variant_type`). The "exactly one
/// typed family" guarantee is enforced by ValidateRepeatConfig at parse/compile
/// time instead.
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

struct OperandData {
  bool any = false;
  std::optional<model::OperandType> type;

  /// @brief Declaration cross-reference constraint (match operand patterns only).
  /// Nested `decl:` subobject — every specified field must equal the declaration
  /// the operand's register resolves to. Field validity per operand type is
  /// checked at parse time.
  struct DeclData {
    std::optional<model::ResourceDimension> dimension;
    std::array<std::optional<model::ResourceReturnType>, 4> return_type;
    std::optional<uint32_t> structure_stride;
    std::optional<model::SamplerMode> mode;
    std::optional<model::CbufferAccessPattern> access_pattern;
    std::optional<model::SignatureSemantic> semantic;
    std::optional<model::InterpolationMode> interpolation;
  };

  /// @brief One index slot in an operand — mirrors Operand::Index.
  struct IndexData {
    bool any = false;
    OperandIndexRepresentation representation = OperandIndexRepresentation::Immediate32;
    std::optional<uint32_t> immediate_lo;
    std::optional<uint32_t> immediate_hi;
    std::string capture;
    std::string match_capture;
    std::unique_ptr<OperandData> relative_operand;  ///< Relative operand for index-level relative addressing.

    IndexData() = default;

    /// @brief Deep copy — the recursive relative operand is copied by value.
    IndexData(const IndexData& other)
        : any(other.any), representation(other.representation), immediate_lo(other.immediate_lo), immediate_hi(other.immediate_hi), capture(other.capture), match_capture(other.match_capture), relative_operand(other.relative_operand ? std::make_unique<OperandData>(*other.relative_operand) : nullptr) {}

    IndexData& operator=(const IndexData& other) {
      if (this != &other) {
        any = other.any;
        representation = other.representation;
        immediate_lo = other.immediate_lo;
        immediate_hi = other.immediate_hi;
        capture = other.capture;
        match_capture = other.match_capture;
        relative_operand = other.relative_operand
                               ? std::make_unique<OperandData>(*other.relative_operand)
                               : nullptr;
      }
      return *this;
    }
  };

  std::optional<std::string> export_as;
  std::vector<IndexData> indices;
  std::vector<std::variant<std::string, uint32_t>> immediates_u32;
  std::vector<std::variant<std::string, uint64_t>> immediates_u64;
  std::vector<std::variant<std::string, int32_t>> immediates_i32;
  std::vector<std::variant<std::string, int64_t>> immediates_i64;
  std::vector<std::variant<std::string, float>> immediates_f32;
  std::vector<std::variant<std::string, double>> immediates_f64;
  struct FromHandleData {
    std::string name;
    std::optional<std::variant<std::string, uint32_t>> element_index;  ///< Variable name (string) or literal uint32_t value.
  };
  std::optional<FromHandleData> handle;
  std::optional<DeclData> decl;
  /// @brief Explicit component spec (presence = the user specified `components:`).
  /// Optional because a `capture:` operand inherits its component mode from the
  /// captured operand at runtime; temp handles carry no component mode, so a
  /// temp-handle operand must specify it explicitly (rejected at Validate).
  std::optional<model::Components> components;
  std::optional<model::OperandModifier> modifier;
  std::string capture;
  std::string match_capture;

  OperandData() = default;

  /// @brief Deep copy — indices (and their recursive relative operands) are
  /// copied by value via IndexData's deep copy.
  OperandData(const OperandData& other)
      : any(other.any), type(other.type), export_as(other.export_as), indices(other.indices), immediates_u32(other.immediates_u32), immediates_u64(other.immediates_u64), immediates_i32(other.immediates_i32), immediates_i64(other.immediates_i64), immediates_f32(other.immediates_f32), immediates_f64(other.immediates_f64), handle(other.handle), decl(other.decl), components(other.components), modifier(other.modifier), capture(other.capture), match_capture(other.match_capture) {}

  OperandData& operator=(const OperandData& other) {
    if (this != &other) {
      any = other.any;
      type = other.type;
      export_as = other.export_as;
      indices = other.indices;
      immediates_u32 = other.immediates_u32;
      immediates_u64 = other.immediates_u64;
      immediates_i32 = other.immediates_i32;
      immediates_i64 = other.immediates_i64;
      immediates_f32 = other.immediates_f32;
      immediates_f64 = other.immediates_f64;
      handle = other.handle;
      decl = other.decl;
      components = other.components;
      modifier = other.modifier;
      capture = other.capture;
      match_capture = other.match_capture;
    }
    return *this;
  }
};

/// @brief One extended-opcode expectation within a match entry's
/// `extended_opcodes` list. Exactly one of `any` / `type` / `raw` must be set;
/// structured payload fields refine a `type` expectation.
struct ExtendedOpcodeMatchData {
  bool any = false;                                             ///< `any: true` — position wildcard.
  std::optional<model::ExtendedOpcodeType> type;                ///< `type: sample_controls|resource_dim|resource_type`.
  std::optional<uint32_t> raw;                                  ///< `raw: <token>` — exact 32-bit token.
  std::optional<model::SampleControlsPayload> sample_controls;  ///< u/v/w offsets (type: sample_controls).
  std::optional<model::ResourceDimPayload> resource_dim;        ///< dimension/stride (type: resource_dim).
  std::optional<std::array<uint32_t, 4>> resource_return_type;  ///< per-component types (type: resource_type).
};

struct InstructionMatchData {
  std::optional<model::Opcode> opcode;
  std::string capture;
  std::optional<bool> saturate;
  std::optional<model::InterpolationMode> interpolation;
  int32_t test_boolean = -1;
  std::vector<OperandData> operands;
  /// @brief Extended-opcode expectations; absent = wildcard (any chain),
  /// empty list = the instruction must carry no extended tokens.
  std::optional<std::vector<ExtendedOpcodeMatchData>> extended_opcodes;
  /// @brief Instruction-level fields for declaration opcodes (dcl_resource, dcl_constant_buffer, dcl_sampler, dcl_uav_*).
  std::optional<model::ResourceDimension> dimension;
  std::array<std::optional<model::ResourceReturnType>, 4> return_type;
  uint32_t structure_stride = 0;
  std::optional<model::CbufferAccessPattern> access_pattern;
  std::optional<model::SamplerMode> mode;
  uint32_t uav_flags = 0;
};

/// @brief One extended-opcode emit entry. Exactly one of `type` / `raw` must be
/// set; a `type` entry requires the matching structured payload key.
struct EmitExtendedOpcodeData {
  std::optional<model::ExtendedOpcodeType> type;                ///< `type: sample_controls|resource_dim|resource_type`.
  std::optional<uint32_t> raw;                                  ///< `raw: <token>` (bits 30:00).
  std::optional<model::SampleControlsPayload> sample_controls;  ///< u/v/w offsets (type: sample_controls).
  std::optional<model::ResourceDimPayload> resource_dim;        ///< dimension/stride (type: resource_dim).
  std::optional<std::array<uint32_t, 4>> resource_return_type;  ///< per-component types (type: resource_type).
};

struct EmitInstructionData {
  std::optional<model::Opcode> opcode;
  std::optional<bool> saturate;
  std::optional<model::InterpolationMode> interpolation;
  int32_t test_boolean = -1;
  std::vector<OperandData> operands;
  std::string capture;
  std::string blob;                  ///< Expand a stored blob (mutually exclusive with opcode/capture).
  std::string template_name;         ///< Template instantiation (mutually exclusive with opcode/capture).
  std::optional<RepeatData> repeat;  ///< Per-iteration repeat configuration.
  std::vector<EmitExtendedOpcodeData> extended_opcodes;
  /// @brief Instruction-level fields for declaration opcodes (dcl_resource, dcl_constant_buffer, dcl_sampler, dcl_uav_*).
  std::optional<model::ResourceDimension> dimension;
  std::array<std::optional<model::ResourceReturnType>, 4> return_type;
  uint32_t structure_stride = 0;
  std::optional<model::CbufferAccessPattern> access_pattern;
  std::optional<model::SamplerMode> mode;
  uint32_t uav_flags = 0;
};

struct RuleData {
  std::vector<InstructionMatchData> match;
  std::vector<EmitInstructionData> emit;

  // Per-rule execution modes — honored by blob/scope scoped execution. Plain
  // match steps use the step-level fields instead (these default there).
  MatchKind match_mode = MatchKind::First;
  RewriteKind rewrite_mode = RewriteKind::Replace;
  int32_t insert_index = -1;
  int32_t range_start_offset = 0;
  int32_t range_end_offset = -1;

  /**
   * @brief Compile this YAML data into a Rule.
   * @return Compiled rule or error message.
   */
  [[nodiscard]] auto Compile() const -> std::expected<Rule, std::string>;
};

struct MatchBlobData {
  InstructionMatchData match_start;
  InstructionMatchData match_end;
  std::string capture;
};

struct EmitBlobData {
  EmitBlob::Mode mode = EmitBlob::Mode::None;
};

/// @brief Compiles one YAML OperandData into an OperandPattern.
/// Shared by rule Compile() and template Compile() — declared here so
/// DeclareTemplateStep.cpp can call it without duplicating logic.
auto CompileOperandPattern(const OperandData& operand_data, bool is_emit_operand) -> std::expected<OperandPattern, std::string>;

/// @brief Validate a repeat config: literal times >= 1, no reserved param
/// name (`iteration`), every param has exactly one typed family, and every
/// param array length equals times.
std::expected<void, std::string> ValidateRepeatConfig(const RepeatConfig& repeat, const std::string& path);

/// @brief Convert a YAML repeat declaration into a compiled repeat config.
/// Null when no repeat is declared.
std::optional<RepeatConfig> CompileRepeatConfig(const std::optional<RepeatData>& repeat_data);

/// @brief Maps a component letter to its xyzw index (x=0, y=1, z=2, w=3), or -1.
int ComponentIndex(char c);

/// @brief Converts an operand pattern's component spec (mask/swizzle/select value
/// strings) into the ground-truth component mode (token bits 2-11). Returns
/// std::nullopt when the pattern carries no component constraint.
std::optional<uint32_t> PatternComponentMode(const OperandPattern& op);

/// @brief Collect the capture names referenced by an emit pattern sequence
/// (instruction-level, operand-level, and index-level `capture`) — the
/// template's required captures, registered at declare time.
std::unordered_set<std::string> CollectRequiredCaptures(const std::vector<EmitPattern>& emits);

/// @brief Shared emit-pattern validation (called from the Validate phase, not
/// Compile/parse): operand count against the opcode's instruction layout
/// (declaration adjustments included), per-operand completeness (including the
/// temp-handle component requirement), destination mask-only rule, per-slot
/// expected operand type, and declaration-field opcode restrictions.
/// Common to rule emits and template emits.
std::expected<void, std::string> ValidateEmitPatterns(const std::vector<EmitPattern>& emits, const std::string& path_prefix);

}  // namespace dxp::sm5::step

namespace glz {

template <>
struct meta<dxp::sm5::step::OperandData::IndexData> {
  using T = dxp::sm5::step::OperandData::IndexData;
  static constexpr auto value = glz::object(
      "any", &T::any,
      "representation", &T::representation,
      "immediate_lo", &T::immediate_lo,
      "immediate_hi", &T::immediate_hi,
      "capture", &T::capture,
      "match_capture", &T::match_capture,
      "relative_operand", &T::relative_operand);
};

template <>
struct meta<dxp::sm5::step::OperandData> {
  using T = dxp::sm5::step::OperandData;
  static constexpr auto value = glz::object(
      "any", &T::any,
      "type", &T::type,
      "export_as", &T::export_as,
      "indices", &T::indices,
      "immediates_u32", &T::immediates_u32,
      "immediates_u64", &T::immediates_u64,
      "immediates_i32", &T::immediates_i32,
      "immediates_i64", &T::immediates_i64,
      "immediates_f32", &T::immediates_f32,
      "immediates_f64", &T::immediates_f64,
      "handle", &T::handle,
      "decl", &T::decl,
      "components", &T::components,
      "modifier", &T::modifier,
      "capture", &T::capture,
      "match_capture", &T::match_capture);
};

template <>
struct meta<dxp::sm5::step::EmitInstructionData> {
  using T = dxp::sm5::step::EmitInstructionData;
  static constexpr auto value = glz::object(
      "opcode", &T::opcode,
      "saturate", &T::saturate,
      "interpolation", &T::interpolation,
      "test_boolean", &T::test_boolean,
      "operands", &T::operands,
      "capture", &T::capture,
      "blob", &T::blob,
      "template", &T::template_name,
      "repeat", &T::repeat,
      "extended_opcodes", &T::extended_opcodes,
      "dimension", &T::dimension,
      "return_type", &T::return_type,
      "structure_stride", &T::structure_stride,
      "access_pattern", &T::access_pattern,
      "mode", &T::mode,
      "uav_flags", &T::uav_flags);
};

template <>
struct meta<dxp::sm5::step::RuleData> {
  using T = dxp::sm5::step::RuleData;
  static constexpr auto value = glz::object(
      "match", &T::match,
      "emit", &T::emit,
      "match_mode", &T::match_mode,
      "rewrite_mode", &T::rewrite_mode,
      "insert_index", &T::insert_index,
      "range_start_offset", &T::range_start_offset,
      "range_end_offset", &T::range_end_offset);
};

template <>
struct meta<dxp::sm5::step::RepeatData> {
  using T = dxp::sm5::step::RepeatData;
  static constexpr auto value = glz::object(
      "times", &T::times,
      "params", &T::params);
};

}  // namespace glz
