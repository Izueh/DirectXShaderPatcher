#include "dxp/Condition_impl.hpp"
#include "dxp/RecipeEngine.hpp"
#include "dxp/RecipeReport.hpp"
#include "dxp/sm6/ExecutionContext.hpp"
#include "dxp/sm6/Recipe_impl.hpp"
#include "dxp/sm6/ShaderProgram.hpp"
#include "dxp/sm6/step/AddResourceStep_impl.hpp"
#include "dxp/sm6/step/ApplyRuleStep_impl.hpp"
#include "dxp/StepResults.hpp"
// clang-format off
// WinIncludes.h must precede dxcapi.h — dxcapi.h uses COM types but doesn't
// include its own Windows dependencies. DXC's own code does the same.
#include <dxc/Support/WinIncludes.h>
#include <dxc/dxcapi.h>
#include <dxc/Support/Global.h>
// clang-format on
#include <combaseapi.h>
#include <any>
#include <cstddef>
#include <cstdint>
#include <dxp/ExportTypes.hpp>
#include <dxp/sm6/Recipe.hpp>
#include <expected>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#include "step/CheckOpcodeCountStep_impl.hpp"
#include "step/CheckResourceCountStep_impl.hpp"
#include "step/CheckShaderVersionStep_impl.hpp"

namespace dxp::sm6 {

std::expected<void, std::string> ValidateRecipe(const Recipe& recipe) {
  if (recipe.validated_.load(std::memory_order_acquire)) return {};

  auto result = dxp::detail::ValidateStepList(recipe.steps_);
  if (!result) return result;

  recipe.validated_.store(true, std::memory_order_release);
  return {};
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
      [](std::span<const uint8_t> bytes, const PatchOptions& options) -> std::expected<ExecutionContext, std::string> {
        ExecutionContext ctx;
        ctx.logger.sink = options.logger;
        ctx.logger.level = options.log_level;
        if (auto load_result = ShaderProgram::FromBytes(bytes, ctx.program); !load_result) {
          return std::unexpected(std::move(load_result.error()));
        }
        for (const auto& warning : ctx.program.warnings) {
          ctx.logger.Log(LogLevel::Warning, warning);
        }
        if (auto* dxil = ctx.program.GetDxilModule()) {
          const auto* sm = dxil->GetShaderModel();
          if (sm != nullptr) {
            ctx.major_version = sm->GetMajor();
            ctx.minor_version = sm->GetMinor();
          }
        }
        return ctx;
      },
      [](ExecutionContext& ctx, std::span<const uint8_t> input_bytes)
          -> std::expected<dxp::detail::ExecutionOutput, std::string> {
        dxp::detail::ExecutionOutput output;
        const HRESULT kHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const bool kComShouldUninitialize = !DXC_FAILED(kHr);
        if (ctx.program_modified) {
          auto serialized = ctx.program.Serialize();
          if (!serialized) {
            if (kComShouldUninitialize) {
              CoUninitialize();
            }
            return std::unexpected(std::move(serialized.error()));
          }
          output.output_bytes = std::move(*serialized);
        } else {
          output.output_bytes.assign(input_bytes.begin(), input_bytes.end());
        }
        if (kComShouldUninitialize) {
          CoUninitialize();
        }
        auto report_result = ShaderProgram::BuildContainerReport(output.output_bytes, output.container_report);
        if (!report_result) {
          return std::unexpected(std::move(report_result.error()));
        }
        return output;
      });
}

}  // namespace dxp::sm6
