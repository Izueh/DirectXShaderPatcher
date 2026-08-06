#pragma once

#include <atomic>
#include <concepts>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

#include <expected>
#include "dxp/Condition.hpp"
#include "dxp/ExportTypes.hpp"
#include "dxp/PatchOptions.hpp"
#include "dxp/RecipeReport.hpp"
#include "dxp/sm5/Model.hpp"
#include "dxp/sm5/step/AddResourceStep.hpp"
#include "dxp/sm5/step/ApplyRuleStep.hpp"
#include "dxp/sm5/step/CheckOpcodeCountStep.hpp"
#include "dxp/sm5/step/CheckResourceCountStep.hpp"
#include "dxp/sm5/step/CheckShaderVersionStep.hpp"

namespace dxp::sm5 {

struct Recipe {
  using StepVariant = std::variant<
      ::dxp::sm5::step::AddResourceStep, ::dxp::sm5::step::CheckShaderVersionStep,
      ::dxp::sm5::step::CheckOpcodeCountStep, ::dxp::sm5::step::CheckResourceCountStep,
      ::dxp::sm5::step::ApplyRuleStep>;

 public:
  Recipe() = default;

  /// @brief Copy constructor — copies steps and env; the validation flag follows the source.
  Recipe(const Recipe& other)
      : steps_(other.steps_),
        env_(other.env_),
        validated_(other.validated_.load(std::memory_order_relaxed)) {}

  /// @brief Copy assignment — see copy constructor.
  Recipe& operator=(const Recipe& other) {
    if (this != &other) {
      steps_ = other.steps_;
      env_ = other.env_;
      validated_.store(other.validated_.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }
    return *this;
  }

  /// @brief Move constructor — moves steps and env; the validation flag follows the source.
  Recipe(Recipe&& other) noexcept
      : steps_(std::move(other.steps_)),
        env_(std::move(other.env_)),
        validated_(other.validated_.load(std::memory_order_relaxed)) {}

  /// @brief Move assignment — see move constructor.
  Recipe& operator=(Recipe&& other) noexcept {
    if (this != &other) {
      steps_ = std::move(other.steps_);
      env_ = std::move(other.env_);
      validated_.store(other.validated_.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }
    return *this;
  }

  /**
   * @brief Add a step to the recipe (invalidates validation cache).
   * @param step The step to add.
   * @return *this for chaining.
   *
   * Setup-only API: must not race with Execute() or other AddStep/SetEnv calls
   * (see the thread-safety contract: recipes are immutable once shared).
   */
  template <typename T>
  Recipe& AddStep(T step) {
    steps_.push_back(StepVariant{std::move(step)});
    validated_.store(false, std::memory_order_relaxed);
    return *this;
  }

  /**
   * @brief Seed environment variables from PatchOptions into the recipe.
   * @param options The options to copy env vars from.
   * @return *this for chaining.
   *
   * Note: this pre-seeds the recipe's env. Options passed to Execute() are merged
   * again at execute time (later values win, idempotent) — SetEnv(PatchOptions)
   * is only needed when reusing one recipe across multiple Execute calls with
   * different shaders.
   */
  Recipe& SetEnv(const PatchOptions& options) {
    // Env vars do not affect structural validation (ValidateRecipe checks names,
    // captures, and references only), so the validated_ cache is NOT invalidated.
    // Revisit if validation ever reads env values.
    options.MergeEnvInto(this->env_);
    return *this;
  }

  /**
   * @brief Set a single environment variable.
   * @param key Variable name.
   * @param value Variable value.
   * @return *this for chaining.
   */
  Recipe& SetEnv(std::string key, PrimitiveValue value) {
    this->env_[std::move(key)] = value;
    return *this;
  }

  /**
   * @brief Parse a recipe from a YAML file.
   * @param path Path to the YAML file.
   * @return Parsed recipe or error message.
   */
  static std::expected<Recipe, std::string> ParseFromFile(const std::string& path);
  /**
   * @brief Parse a recipe from a YAML string.
   * @param text YAML string content.
   * @param name Optional recipe name.
   * @return Parsed recipe or error message.
   */
  static std::expected<Recipe, std::string> ParseFromText(const std::string& text, const std::string& source_name = {});

  /**
   * @brief Get the number of steps in the recipe.
   * @return Number of steps.
   */
  size_t GetStepCount() const;

  /**
   * @brief Execute the recipe on the given DXBC container bytes.
   * @param input DXBC container bytes to patch.
   * @param options Optional patch options.
   * @return std::expected<RecipeReport, string> with results and serialized output.
   */
  std::expected<RecipeReport, std::string> Execute(const std::vector<uint8_t>& input,
                                                   const PatchOptions& options = {}) const;

 private:
  std::vector<StepVariant> steps_;
  std::unordered_map<std::string, PrimitiveValue> env_;

  /// @brief Lazy validation cache. Written by ValidateRecipe (via the first
  /// Execute or an explicit validate) and reset by AddStep. Logically const —
  /// recipes are reused and treated as const once validated.
  ///
  /// Atomic so concurrent Execute() calls on the same shared recipe are race-
  /// free: validation itself is pure (reads only steps_, uses a local
  /// ValidationContext), so concurrent re-validation is idempotent and the flag
  /// is just a cache. Acquire/release pairs the store in ValidateRecipe with the
  /// load in Execute.
  mutable std::atomic<bool> validated_{false};

  friend std::expected<void, std::string> ValidateRecipe(const Recipe& recipe);
};

/// @brief Validates the recipe's structure (step names, captures, references) and
/// caches the result so subsequent Execute calls on the const recipe skip re-validation.
/// Thread-safe: safe to call concurrently on a shared const recipe.
std::expected<void, std::string> ValidateRecipe(const Recipe& recipe);

/// @brief Result of parsing a recipe from YAML.
using RecipeParseResult = std::expected<Recipe, std::string>;

}  // namespace dxp::sm5
