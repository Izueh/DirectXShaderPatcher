#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "dxp/Condition.hpp"
#include "dxp/sm6/step/ApplyRuleStep.hpp"
#include "dxp/StepResults.hpp"

namespace dxp::sm6::step {

/// @brief Runtime step type for declaring a reusable template emit sequence.
/// Templates are registered in the execution context and can be instantiated
/// from emit streams via `template:` entries. They do not modify the shader
/// — they only make their emit patterns available for later use.
struct DeclareTemplateStep {
  static constexpr std::string_view kind = "declare_template";
  using Results = dxp::DeclareTemplateResults;

  /// @brief Template name — used as the lookup key in `template:` emit entries.
  std::string name;

  /// @brief Whether this step is required (recipe stops early if it doesn't
  ///        execute, e.g. condition false).
  bool required = true;

  /// @brief Optional condition controlling whether the template is registered.
  std::optional<ConditionNode> condition;

  /// @brief Template temp names — flat list of handle names used in the emit
  ///        sequence. These form the per-instantiation value map namespace.
  std::vector<std::string> temps;

  /// @brief The template's emit sequence — compiled emit patterns that are
  ///        expanded when the template is instantiated from an emit stream.
  std::vector<EmitPattern> emits;

  /// @brief Default constructor.
  DeclareTemplateStep() = default;

  /// @brief Construct a DeclareTemplateStep.
  /// @param name_val Template name.
  /// @param required_val Whether the step is required.
  /// @param condition_val Optional condition.
  /// @param temps_val Template temp names.
  /// @param emits_val Emit patterns.
  DeclareTemplateStep(std::string name_val, bool required_val,
                      std::optional<ConditionNode> condition_val,
                      std::vector<std::string> temps_val,
                      std::vector<EmitPattern> emits_val)
      : name(std::move(name_val)),
        required(required_val),
        condition(std::move(condition_val)),
        temps(std::move(temps_val)),
        emits(std::move(emits_val)) {}
};

}  // namespace dxp::sm6::step
