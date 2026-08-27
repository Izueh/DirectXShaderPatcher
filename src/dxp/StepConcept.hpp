#pragma once

#include <concepts>
#include <expected>
#include <string_view>
#include <type_traits>

#include <dxp/StepResults.hpp>
#include "dxp/ValidationContext.hpp"

namespace dxp {

/// @brief Validates that a type satisfies the basic declarative structure of a recipe step.
///
/// This concept enforces the public API surface area, ensuring the type provides
/// a type alias for its results alongside core properties: name, required status, and conditions.
template <typename T>
concept RecipeStep = requires(T step) {
  typename T::Results;
  { step.name } -> std::convertible_to<std::string_view>;
  { step.required } -> std::convertible_to<bool>;
  { step.condition };
};

/// @brief Validates that a recipe step type supports internal validation and engine execution.
///
/// This concept builds directly upon RecipeStep, adding constraints for backend engine processing.
/// It is intended for internal engine use, such as compile-time assertions in .cpp files.
template <typename Step, typename Context>
concept ExecutableStep = RecipeStep<Step> && requires(const Step& step, Context& exec_ctx, ValidationContext& validation_ctx) {
  { Validate(step, validation_ctx) }
  -> std::same_as<std::expected<void, std::string>>;
  { Execute(step, exec_ctx) }
  -> std::same_as<std::expected<typename Step::Results, std::string>>;
};

}  // namespace dxp
