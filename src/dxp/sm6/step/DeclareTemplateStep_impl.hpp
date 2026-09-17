#pragma once

#include <dxp/sm6/step/DeclareTemplateStep.hpp>
#include <glaze/glaze.hpp>
#include "dxp/Condition_impl.hpp"
#include "dxp/sm6/ExecutionContext.hpp"
#include "dxp/StepConcept.hpp"
#include "dxp/StepResults.hpp"
#include "dxp/ValidationContext.hpp"

namespace dxp::sm6::step {

/// @brief Execute the DeclareTemplateStep against the shader program.
/// @param step The step to execute.
/// @param ctx Execution context containing the shader program.
/// @return Results with instantiation count, or error message.
std::expected<dxp::DeclareTemplateResults, std::string> Execute(const DeclareTemplateStep& step, ExecutionContext& ctx);

/// @brief Validate the DeclareTemplateStep.
/// @param step The step to validate.
/// @param error Output error message on failure.
/// @param ctx Validation context.
/// @return void on success, error message on failure.
std::expected<void, std::string> Validate(const DeclareTemplateStep& step, dxp::ValidationContext& ctx);

/// @brief Formats the step's result as a Trace log message.
std::string DescribeOutcome(const DeclareTemplateStep&, const dxp::DeclareTemplateResults& results,
                            const ExecutionContext& ctx);

/// @brief One operand in a template emit entry (YAML-parseable).
struct TemplateOperandData {
  std::string capture;
  std::string match_capture;
  struct Handle {
    std::string name;
    std::optional<std::variant<std::string, uint32_t>> element_index;
  };
  std::optional<Handle> handle;
  std::optional<std::string> type;
  std::vector<std::variant<std::string, uint32_t>> immediates_u32;
  std::vector<std::variant<std::string, float>> immediates_f32;
  std::string mask;
  std::string swizzle;
  std::string select;
  std::optional<std::string> kind;
  std::vector<int64_t> constant_int_values;
  std::vector<double> constant_float_values;
  unsigned operand_index = 0;
  std::optional<std::string> export_as;
};

/// @brief One emit entry in a template (YAML-parseable).
struct TemplateEmitData {
  std::optional<std::string> opcode;
  std::string name;
  std::vector<TemplateOperandData> operands;
  std::string capture;
  std::string blob;
  std::optional<bool> saturate;
  std::optional<std::string> interpolation;
  int32_t test_boolean = -1;
  std::optional<std::string> replace_captured;
};

/// @brief YAML-parseable template step declaration.
struct TemplateStepData {
  std::string name;
  std::vector<std::string> temps;
  std::vector<TemplateEmitData> emit;
  dxp::ConditionData condition;
  bool required = true;

  /// @brief Compile this YAML data into a DeclareTemplateStep.
  [[nodiscard]] auto Compile() const -> std::expected<DeclareTemplateStep, std::string>;
};

}  // namespace dxp::sm6::step

namespace glz {

template <>
struct meta<dxp::sm6::step::TemplateOperandData> {
  using T = dxp::sm6::step::TemplateOperandData;
  static constexpr auto value = object(
      "capture", &T::capture, "match_capture", &T::match_capture,
      "handle", &T::handle, "type", &T::type,
      "immediates_u32", &T::immediates_u32, "immediates_f32", &T::immediates_f32,
      "mask", &T::mask, "swizzle", &T::swizzle, "select", &T::select,
      "kind", &T::kind, "constant_int_values", &T::constant_int_values,
      "constant_float_values", &T::constant_float_values,
      "operand_index", &T::operand_index, "export_as", &T::export_as);
};

template <>
struct meta<dxp::sm6::step::TemplateEmitData> {
  using T = dxp::sm6::step::TemplateEmitData;
  static constexpr auto value = object(
      "opcode", &T::opcode, "name", &T::name, "operands", &T::operands,
      "capture", &T::capture, "blob", &T::blob, "saturate", &T::saturate,
      "interpolation", &T::interpolation, "test_boolean", &T::test_boolean,
      "replace_captured", &T::replace_captured);
};

template <>
struct meta<dxp::sm6::step::TemplateStepData> {
  using T = dxp::sm6::step::TemplateStepData;
  static constexpr auto value = object(
      "name", &T::name, "temps", &T::temps, "emit", &T::emit,
      "condition", &T::condition, "required", &T::required);
};

}  // namespace glz
