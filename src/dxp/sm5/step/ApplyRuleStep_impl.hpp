#pragma once
#include <dxp/sm5/step/ApplyRuleStep.hpp>
#include "dxp/Condition_impl.hpp"
#include "dxp/sm5/ExecutionContext.hpp"
#include "dxp/sm5/Model_impl.hpp"
#include "dxp/StepConcept.hpp"
#include "dxp/StepResults.hpp"
#include "dxp/ValidationContext.hpp"

namespace dxp::sm5::step {
using namespace dxp::sm5::model;

// Internal convenience aliases — the public pattern types live nested inside
// ApplyRuleStep so the step namespace holds only the step structs.
using MatchKind = ApplyRuleStep::MatchKind;
using RewriteKind = ApplyRuleStep::RewriteKind;
using OperandIndexRepresentation = ApplyRuleStep::OperandIndexRepresentation;
using ImmediateFamily = ApplyRuleStep::IndexImmediateType;
using OperandIndexPattern = ApplyRuleStep::OperandIndexPattern;
using OperandPattern = ApplyRuleStep::OperandPattern;
using InstructionPattern = ApplyRuleStep::InstructionPattern;
using EmitPattern = ApplyRuleStep::EmitPattern;
using ExtendedOpcodePattern = ApplyRuleStep::ExtendedOpcodePattern;
using Rule = ApplyRuleStep::Rule;

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
std::expected<void, std::string> Validate(const ApplyRuleStep& step, std::string& error, dxp::ValidationContext& ctx);
/// @brief Formats the step's result as a Trace log message.
std::string DescribeOutcome(const ApplyRuleStep& step, const dxp::ApplyRuleResults& results, const ExecutionContext& ctx);

struct OperandData {
  bool any = false;
  std::optional<OperandType> type;

  /// @brief One index slot in an operand — mirrors Operand::Index.
  struct IndexData {
    bool any = false;
    OperandIndexRepresentation representation = OperandIndexRepresentation::Immediate32;
    std::optional<uint32_t> immediate_lo;
    std::optional<uint32_t> immediate_hi;
    std::string capture;
    std::string match_capture;
    std::unique_ptr<OperandData> relative_operand;  ///< Relative operand for index-level relative addressing.
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
  Components components;
  std::optional<OperandModifier> modifier;
  std::string capture;
  std::string match_capture;
};

/// @brief One extended-opcode expectation within a match entry's
/// `extended_opcodes` list. Exactly one of `any` / `type` / `raw` must be set;
/// structured payload fields refine a `type` expectation.
struct ExtendedOpcodeMatchData {
  bool any = false;                                             ///< `any: true` — position wildcard.
  std::optional<ExtendedOpcodeType> type;                       ///< `type: sample_controls|resource_dim|resource_type`.
  std::optional<uint32_t> raw;                                  ///< `raw: <token>` — exact 32-bit token.
  std::optional<SampleControlsPayload> sample_controls;         ///< u/v/w offsets (type: sample_controls).
  std::optional<ResourceDimPayload> resource_dim;               ///< dimension/stride (type: resource_dim).
  std::optional<std::array<uint32_t, 4>> resource_return_type;  ///< per-component types (type: resource_type).
};

struct InstructionMatchData {
  std::optional<Opcode> opcode;
  std::string capture;
  std::optional<bool> saturate;
  std::optional<InterpolationMode> interpolation;
  int32_t test_boolean = -1;
  std::vector<OperandData> operands;
  /// @brief Extended-opcode expectations; absent = wildcard (any chain),
  /// empty list = the instruction must carry no extended tokens.
  std::optional<std::vector<ExtendedOpcodeMatchData>> extended_opcodes;
};

/// @brief One extended-opcode emit entry. Exactly one of `type` / `raw` must be
/// set; a `type` entry requires the matching structured payload key.
struct EmitExtendedOpcodeData {
  std::optional<ExtendedOpcodeType> type;                       ///< `type: sample_controls|resource_dim|resource_type`.
  std::optional<uint32_t> raw;                                  ///< `raw: <token>` (bits 30:00).
  std::optional<SampleControlsPayload> sample_controls;         ///< u/v/w offsets (type: sample_controls).
  std::optional<ResourceDimPayload> resource_dim;               ///< dimension/stride (type: resource_dim).
  std::optional<std::array<uint32_t, 4>> resource_return_type;  ///< per-component types (type: resource_type).
};

struct EmitInstructionData {
  std::optional<Opcode> opcode;
  std::optional<bool> saturate;
  std::optional<InterpolationMode> interpolation;
  int32_t test_boolean = -1;
  std::vector<OperandData> operands;
  std::string capture;
  std::vector<EmitExtendedOpcodeData> extended_opcodes;
};

struct RuleData {
  std::vector<InstructionMatchData> match;
  std::vector<EmitInstructionData> emit;

  /**
   * @brief Compile this YAML data into a Rule.
   * @return Compiled rule or error message.
   */
  [[nodiscard]] auto Compile() const -> std::expected<Rule, std::string>;
};

struct ApplyRuleData {
  std::string name;
  RewriteKind rewrite_mode = RewriteKind::Replace;
  MatchKind match_mode = MatchKind::First;
  bool required = true;
  dxp::ConditionData condition;
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

}  // namespace dxp::sm5::step

namespace glz {

template <>
struct meta<dxp::sm5::step::ApplyRuleStep::MatchKind> {
  static constexpr auto value = enumerate("first", dxp::sm5::step::ApplyRuleStep::MatchKind::First, "last", dxp::sm5::step::ApplyRuleStep::MatchKind::Last, "match_all", dxp::sm5::step::ApplyRuleStep::MatchKind::MatchAll);
};

template <>
struct meta<dxp::sm5::step::ApplyRuleStep::RewriteKind> {
  static constexpr auto value = enumerate("none", dxp::sm5::step::ApplyRuleStep::RewriteKind::None, "replace", dxp::sm5::step::ApplyRuleStep::RewriteKind::Replace, "before", dxp::sm5::step::ApplyRuleStep::RewriteKind::Before, "after", dxp::sm5::step::ApplyRuleStep::RewriteKind::After, "replace_range", dxp::sm5::step::ApplyRuleStep::RewriteKind::ReplaceRange);
};

template <>
struct meta<dxp::sm5::step::ApplyRuleStep::OperandIndexRepresentation> {
  static constexpr auto value = enumerate("immediate32", dxp::sm5::step::ApplyRuleStep::OperandIndexRepresentation::Immediate32, "immediate64", dxp::sm5::step::ApplyRuleStep::OperandIndexRepresentation::Immediate64, "relative", dxp::sm5::step::ApplyRuleStep::OperandIndexRepresentation::Relative, "immediate32_plus_relative", dxp::sm5::step::ApplyRuleStep::OperandIndexRepresentation::Immediate32PlusRelative, "immediate64_plus_relative", dxp::sm5::step::ApplyRuleStep::OperandIndexRepresentation::Immediate64PlusRelative);
};

template <>
struct meta<dxp::sm5::ExtendedOpcodeType> {
  static constexpr auto value = enumerate("empty", dxp::sm5::ExtendedOpcodeType::Empty, "sample_controls", dxp::sm5::ExtendedOpcodeType::SampleControls, "resource_dim", dxp::sm5::ExtendedOpcodeType::ResourceDim, "resource_type", dxp::sm5::ExtendedOpcodeType::ResourceType);
};

}  // namespace glz
