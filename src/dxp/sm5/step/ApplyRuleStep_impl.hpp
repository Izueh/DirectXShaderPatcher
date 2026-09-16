#pragma once
#include <dxp/sm5/step/ApplyRuleStep.hpp>
#include "dxp/Condition_impl.hpp"
#include "dxp/sm5/ExecutionContext.hpp"
#include "dxp/sm5/Model_impl.hpp"
#include "dxp/sm5/step/common/Rule_impl.hpp"
#include "dxp/StepConcept.hpp"
#include "dxp/StepResults.hpp"
#include "dxp/ValidationContext.hpp"

namespace dxp::sm5::step {

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
std::expected<void, std::string> Validate(const ApplyRuleStep& step, dxp::ValidationContext& ctx);
/// @brief Formats the step's result as a Trace log message.
std::string DescribeOutcome(const ApplyRuleStep& step, const dxp::ApplyRuleResults& results, const ExecutionContext& ctx);

/// @brief Stamps resource dimension/return type from the operand's declaration
/// so canonical extended-opcode chains can be synthesized.
void StampResourceAccessControls(ExecutionContext& context, model::Instruction& instr);

struct ApplyRuleData {
  std::string name;
  RewriteKind rewrite_mode = RewriteKind::Replace;
  MatchKind match_mode = MatchKind::First;
  bool required = true;
  dxp::ConditionData condition;
  int32_t insert_index = -1;
  int32_t range_start_offset = 0;
  int32_t range_end_offset = -1;
  std::optional<MatchBlobData> match_blob;
  std::optional<EmitBlobData> emit_blob;
  std::string scope;
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
struct meta<dxp::sm5::step::MatchKind> {
  static constexpr auto value = enumerate("first", dxp::sm5::step::MatchKind::First, "last", dxp::sm5::step::MatchKind::Last, "match_all", dxp::sm5::step::MatchKind::MatchAll);
};

template <>
struct meta<dxp::sm5::step::RewriteKind> {
  static constexpr auto value = enumerate("none", dxp::sm5::step::RewriteKind::None, "replace", dxp::sm5::step::RewriteKind::Replace, "before", dxp::sm5::step::RewriteKind::Before, "after", dxp::sm5::step::RewriteKind::After, "replace_range", dxp::sm5::step::RewriteKind::ReplaceRange, "before_last_return", dxp::sm5::step::RewriteKind::BeforeLastReturn);
};

template <>
struct meta<dxp::sm5::step::EmitBlob::Mode> {
  static constexpr auto value = enumerate("none", dxp::sm5::step::EmitBlob::Mode::None, "replace", dxp::sm5::step::EmitBlob::Mode::Replace, "before", dxp::sm5::step::EmitBlob::Mode::Before, "after", dxp::sm5::step::EmitBlob::Mode::After);
};

template <>
struct meta<dxp::sm5::step::OperandIndexRepresentation> {
  static constexpr auto value = enumerate("immediate32", dxp::sm5::step::OperandIndexRepresentation::Immediate32, "immediate64", dxp::sm5::step::OperandIndexRepresentation::Immediate64, "relative", dxp::sm5::step::OperandIndexRepresentation::Relative, "immediate32_plus_relative", dxp::sm5::step::OperandIndexRepresentation::Immediate32PlusRelative, "immediate64_plus_relative", dxp::sm5::step::OperandIndexRepresentation::Immediate64PlusRelative);
};

template <>
struct meta<dxp::sm5::ExtendedOpcodeType> {
  static constexpr auto value = enumerate("empty", dxp::sm5::ExtendedOpcodeType::Empty, "sample_controls", dxp::sm5::ExtendedOpcodeType::SampleControls, "resource_dim", dxp::sm5::ExtendedOpcodeType::ResourceDim, "resource_type", dxp::sm5::ExtendedOpcodeType::ResourceType);
};

}  // namespace glz
