#include <cstddef>
#include <cstdint>
#include <dxp/sm5/Recipe.hpp>
#include "dxp/Condition_impl.hpp"
#include "dxp/RecipeEngine.hpp"
#include "dxp/RecipeReport.hpp"
#include "dxp/sm5/ExecutionContext.hpp"
#include "dxp/sm5/Recipe_impl.hpp"
#include "dxp/sm5/ShaderProgram.hpp"
#include "dxp/StepResults.hpp"

#include <expected>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#include "dxp/sm5/step/AddResourceStep_impl.hpp"
#include "dxp/sm5/step/ApplyRuleStep_impl.hpp"
#include "dxp/sm5/step/CheckOpcodeCountStep_impl.hpp"
#include "dxp/sm5/step/CheckResourceCountStep_impl.hpp"
#include "dxp/sm5/step/CheckShaderVersionStep_impl.hpp"

namespace dxp::sm5 {
using namespace dxp::sm5::model;

std::expected<void, std::string> ValidateRecipe(const Recipe& recipe) {
  if (recipe.validated_.load(std::memory_order_acquire)) return {};

  auto result = dxp::detail::ValidateStepList(recipe.steps_);
  if (!result) return result;

  recipe.validated_.store(true, std::memory_order_release);
  return {};
}

std::pair<Recipe, std::string> ConvertRecipe(const RecipeData& data) {
  return dxp::detail::ConvertRecipeData<Recipe>(data.steps);
}

size_t Recipe::GetStepCount() const {
  return steps_.size();
}

std::expected<Recipe, std::string> Recipe::ParseFromFile(const std::string& path) {
  return dxp::detail::ParseRecipeFromFile<Recipe, RecipeData>(path);
}

std::expected<Recipe, std::string> Recipe::ParseFromText(const std::string& text, const std::string& source_name) {
  return dxp::detail::ParseRecipeFromText<Recipe, RecipeData>(text, source_name);
}

auto Recipe::Execute(std::span<const uint8_t> input,
                     const PatchOptions& options) const -> std::expected<RecipeReport, std::string> {
  // ValidateRecipe owns the thread-safe lazy cache (atomic flag); concurrent
  // Execute calls on a shared recipe may all validate simultaneously — the
  // validation itself is pure, so re-validation is idempotent.
  auto validationError = ValidateRecipe(*this);
  if (!validationError) {
    return std::unexpected(std::move(validationError.error()));
  }

  return dxp::detail::ExecuteSteps<ExecutionContext>(
      steps_, env_, input, options,
      [](std::span<const uint8_t> bytes, const PatchOptions&) -> std::expected<ExecutionContext, std::string> {
        ExecutionContext ctx;
        auto program = ShaderProgram::FromBytes(bytes);
        if (!program) {
          return std::unexpected(std::move(program.error()));
        }
        ctx.program = std::move(*program);
        if (ctx.program.instructions.empty() && ctx.program.temp_count == 0) {
          return std::unexpected("failed to parse shader chunk");
        }
        ctx.major_version = ctx.program.major_version;
        ctx.minor_version = ctx.program.minor_version;
        ctx.captures.Clear();
        ctx.reserved_temp_base = ctx.program.temp_count;
        ctx.reserved_temp_count = 0;
        return ctx;
      },
      [](ExecutionContext& ctx, std::span<const uint8_t> input_bytes)
          -> std::expected<dxp::detail::ExecutionOutput, std::string> {
        dxp::detail::ExecutionOutput output;
        if (ctx.program_modified) {
          auto serialized = ctx.program.Serialize();
          if (!serialized) {
            return std::unexpected(std::move(serialized.error()));
          }
          output.output_bytes = std::move(*serialized);
        } else {
          output.output_bytes.assign(input_bytes.begin(), input_bytes.end());
        }
        output.container_report = ctx.program.BuildReport();
        return output;
      });
}

}  // namespace dxp::sm5
