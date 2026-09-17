#pragma once

#include <dxp/sm5/step/DeclareTemplateStep.hpp>
#include <glaze/glaze.hpp>
#include "dxp/Condition_impl.hpp"
#include "dxp/sm5/step/common/Rule_impl.hpp"
#include "dxp/StepConcept.hpp"
#include "dxp/StepResults.hpp"
#include "dxp/ValidationContext.hpp"

namespace dxp::sm5::step {
using namespace dxp::sm5::model;

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

/// @brief YAML-parseable template step declaration.
struct TemplateStepData {
  std::string name;
  std::vector<std::string> temps;
  std::vector<std::string> params;
  std::vector<EmitInstructionData> emit;
  dxp::ConditionData condition;
  bool required = true;

  /// @brief Compile this YAML data into a DeclareTemplateStep.
  [[nodiscard]] auto Compile() const -> std::expected<DeclareTemplateStep, std::string>;
};

}  // namespace dxp::sm5::step

namespace glz {

template <>
struct meta<dxp::sm5::step::TemplateStepData> {
  using T = dxp::sm5::step::TemplateStepData;
  static constexpr auto value = glz::object(
      "name", &T::name,
      "temps", &T::temps,
      "params", &T::params,
      "emit", &T::emit,
      "condition", &T::condition,
      "required", &T::required);
  static constexpr auto validate = [](const T& self, std::string& error) {
    if (self.name.empty()) {
      error = "declare_template requires a name";
      return;
    }
    if (self.temps.empty()) {
      error = "declare_template requires at least one temp";
      return;
    }
    if (self.emit.empty()) {
      error = "declare_template requires at least one emit pattern";
      return;
    }
    for (const auto& param_name : self.params) {
      if (param_name.empty()) {
        error = "declare_template param names must be non-empty";
        return;
      }
      if (param_name == "iteration") {
        error = "declare_template param name 'iteration' is reserved (implicit 0-based repeat index variable)";
        return;
      }
    }
  };
};

}  // namespace glz
