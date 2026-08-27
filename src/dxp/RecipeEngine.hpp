#pragma once

#include <any>
#include <cstdint>
#include <expected>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <glaze/json/write.hpp>
#include <glaze/yaml/read.hpp>

#include "dxp/utils/Hash.hpp"

#include "dxp/Condition_impl.hpp"
#include "dxp/ExportTypes.hpp"
#include "dxp/Logging.hpp"
#include "dxp/PatchOptions.hpp"
#include "dxp/RecipeReport.hpp"
#include "dxp/StepResults.hpp"
#include "dxp/ValidationContext.hpp"

namespace dxp::detail {

/// @brief Validates a recipe step list. Per-step `Validate` is found via ADL
/// (dxp::sm5::step / dxp::sm6::step). Shared by both backends so the two recipe
/// engines cannot drift.
template <typename StepVariant>
std::expected<void, std::string> ValidateStepList(const std::vector<StepVariant>& steps) {
  ValidationContext ctx;

  for (const auto& step : steps) {
    std::string step_name;
    std::string step_kind;
    std::expected<void, std::string> result = std::visit([&](auto& s) -> std::expected<void, std::string> {
      step_name = s.name;
      step_kind = s.kind;
      auto vresult = Validate(s, ctx);
      if (!vresult) {
        return std::unexpected(step_kind + " '" + step_name + "': " + vresult.error());
      }
      return {};
    },
               step);
    if (!result) return std::unexpected(std::move(result).error());
    ctx.names.insert(std::move(step_name));
  }
  return {};
}

/// @brief Converts a YAML step-data list into recipe steps. Each `*Data` type's
/// `Compile()` fully populates the runtime step (name, condition, match mode),
/// so no per-backend dispatch is needed.
template <typename Recipe, typename StepDataVariant>
std::pair<Recipe, std::string> ConvertRecipeData(const std::vector<StepDataVariant>& steps) {
  Recipe recipe;
  std::string error;

  for (const auto& step_var : steps) {
    if (!error.empty()) break;
    std::visit([&](auto& step_data) {
      if (!error.empty()) return;
      auto step_or_err = step_data.Compile();
      if (!step_or_err) {
        error = std::move(step_or_err.error());
        return;
      }
      recipe.AddStep(std::move(*step_or_err));
    },
               step_var);
  }

  if (!error.empty()) return {Recipe{}, std::move(error)};
  return {std::move(recipe), {}};
}

/// @brief Converts a YAML recipe data document into a recipe, including steps and env.
template <typename Recipe, typename StepDataVariant, typename EnvMap>
std::pair<Recipe, std::string> ConvertRecipeData(const std::vector<StepDataVariant>& steps, const EnvMap& env) {
  auto [recipe, error] = ConvertRecipeData<Recipe>(steps);
  if (!error.empty()) return {Recipe{}, std::move(error)};
  for (const auto& [key, value] : env) {
    recipe.SetEnv(key, value);
  }
  return {std::move(recipe), {}};
}

/// @brief Parses a recipe from a YAML string and converts it into a recipe.
template <typename Recipe, typename RecipeData>
std::expected<Recipe, std::string> ParseRecipeFromText(const std::string& text,
                                                       const std::string& source_name = {}) {
  RecipeData doc;
  auto ec = glz::read_yaml(doc, text);
  if (ec) {
    return std::unexpected(source_name.empty() ? "recipe" : source_name + ": " + glz::format_error(ec, text));
  }
  auto [recipe, convert_error] = ConvertRecipeData<Recipe>(doc.steps, doc.env);
  if (!convert_error.empty()) {
    return std::unexpected(std::move(convert_error));
  }
  return std::move(recipe);
}

/// @brief Parses a recipe from a YAML file and converts it into a recipe.
template <typename Recipe, typename RecipeData>
std::expected<Recipe, std::string> ParseRecipeFromFile(const std::string& path) {
  RecipeData doc;
  auto ec = glz::read_file_yaml(doc, path);
  if (ec) {
    return std::unexpected(path + ": " + glz::format_error(ec, std::string{}));
  }
  auto [recipe, convert_error] = ConvertRecipeData<Recipe>(doc.steps, doc.env);
  if (!convert_error.empty()) {
    return std::unexpected(std::move(convert_error));
  }
  return std::move(recipe);
}

/// @brief Serialized output of a recipe execution: raw bytes + container report.
struct ExecutionOutput {
  std::vector<uint8_t> output_bytes;
  PatchContainerReport container_report;
};

/// @brief "0x" + uppercase hex of a 32-bit value.
inline std::string UpperHex32(uint32_t value) {
  static constexpr char kHex[] = "0123456789ABCDEF";
  std::string hex = "0x";
  for (int shift = 28; shift >= 0; shift -= 4) {
    hex += kHex[(value >> shift) & 0xfU];
  }
  return hex;
}

/// @brief "; input hash 0x..." suffix (CRC32 of the input bytes).
inline std::string InputHashSuffix(std::span<const uint8_t> bytes) {
  return "; input hash " + UpperHex32(dxp::utils::hash::ComputeCrc32(bytes));
}

/// @brief "1 texture, 2 cbuffers and 1 temp" from aggregated add_resource totals.
inline std::string FormatAddedResources(const dxp::AddResourceResults& totals) {
  std::vector<std::string> parts;
  const auto add_part = [&parts](uint32_t count, std::string_view name) {
    if (count > 0) parts.push_back(std::format("{} {}{}", count, name, count == 1 ? "" : "s"));
  };
  add_part(totals.textures_added, "texture");
  add_part(totals.raw_resources_added, "raw resource");
  add_part(totals.structured_resources_added, "structured resource");
  add_part(totals.cbuffers_added, "cbuffer");
  add_part(totals.samplers_added, "sampler");
  add_part(totals.uavs_added, "uav");
  add_part(totals.inputs_added, "input");
  add_part(totals.outputs_added, "output");
  add_part(totals.temps_added, "temp");
  if (parts.empty()) return std::string{};
  std::string line;
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i > 0) line += (i + 1 == parts.size()) ? " and " : ", ";
    line += parts[i];
  }
  return line;
}

/// @brief Whether a step's result means it succeeded (report + summary + stop-fast).
template <typename Results, typename Context>
bool StepSucceeded(const Results& r, const Context& ctx) {
  if constexpr (std::is_same_v<Results, dxp::ApplyRuleResults>) {
    return r.match_count > 0;
  } else if constexpr (std::is_same_v<Results, dxp::CheckShaderVersionResults>) {
    return r.major_version == ctx.major_version && r.minor_version == ctx.minor_version;
  }
  return true;
}

/// @brief Executes a recipe step list against a backend execution context.
///
/// Shared step loop for both backends. `make_context` loads the shader program
/// and versions into a fresh context; `build_report` serializes the modified
/// program (backend-specific: DXBC vs DXIL container + COM init) and produces
/// the final output. `ShouldExecute`/`Execute` are resolved by ADL from the
/// step's namespace.
///
/// @param input Borrowed view of the input container bytes; the caller must
///        keep the buffer alive for the duration of the call.
template <typename Context, typename StepVariant, typename ContextFactory, typename ReportBuilder>
std::expected<RecipeReport, std::string> ExecuteSteps(
    const std::vector<StepVariant>& steps,
    const std::unordered_map<std::string, PrimitiveValue>& env,
    std::span<const uint8_t> input,
    const PatchOptions& options,
    ContextFactory&& make_context,
    ReportBuilder&& build_report) {
  // Sink installed before the factory runs so load diagnostics reach it.
  auto context_result = make_context(input, options);
  if (!context_result) {
    LogContext{options.logger, options.log_level}.Log(LogLevel::Error,
                                                      "recipe failed: " + context_result.error());
    return std::unexpected(std::move(context_result.error()));
  }
  Context& exec_context = *context_result;

  for (const auto& [k, v] : env) {
    exec_context.variables[k] = v;
  }
  options.MergeEnvInto(exec_context.variables);

  exec_context.logger.sink = options.logger;
  exec_context.logger.level = options.log_level;
  exec_context.logger.Log(LogLevel::Debug,
                          "recipe execution started (shader SM " + std::to_string(exec_context.major_version) + "." + std::to_string(exec_context.minor_version) + ")");

  RecipeReport result;
  ConditionResolver<Context> resolver(exec_context);

  uint32_t matched_count = 0;
  uint32_t no_match_count = 0;
  uint32_t skipped_count = 0;
  std::string stopped_early_step;
  dxp::AddResourceResults added_totals;

  for (const auto& step_var : steps) {
    bool should_exec = false;
    ResultsVariant step_results;
    std::string step_name;
    std::string_view step_kind;
    bool had_error = false;
    std::string error_msg;
    bool step_required = false;

    std::visit([&](auto& step) {
      should_exec = ShouldExecute(step, resolver);
      step_name = step.name;
      step_kind = step.kind;
      if constexpr (requires { step.required; }) {
        step_required = step.required;
      }
      if (should_exec) {
        exec_context.logger.Log(LogLevel::Trace,
                                std::format("[{}] {}: starting", step.kind, step.name));
        auto result_inner = Execute(step, exec_context);
        if (result_inner) {
          step_results = std::move(*result_inner);
        } else {
          had_error = true;
          error_msg = std::move(result_inner.error());
        }
      }
    },
               step_var);

    if (!should_exec) {
      ++skipped_count;
      exec_context.logger.Log(LogLevel::Trace,
                              std::format("[{}] {}: skipped (condition false)", step_kind, step_name));
      continue;
    }

    if (had_error) {
      exec_context.logger.Log(LogLevel::Error, "recipe failed: " + error_msg + InputHashSuffix(input));
      return std::unexpected(std::move(error_msg));
    }

    // Per-step result message (Trace) + success via ADL DescribeOutcome.
    bool succeeded = false;
    std::visit([&](const auto& step) {
      using Results = typename std::decay_t<decltype(step)>::Results;
      const auto& results = std::get<Results>(step_results);
      const std::string message = DescribeOutcome(step, results, exec_context);
      exec_context.logger.Log(LogLevel::Trace, std::format("[{}] {}: {}", step.kind, step.name, message));
      succeeded = StepSucceeded(results, exec_context);
      // Pretty-printed step results (Trace only).
      if (exec_context.logger.level >= LogLevel::Trace) {
        std::string json;
        if (!glz::write<glz::opts{.prettify = true}>(results, json)) {
          exec_context.logger.Log(LogLevel::Trace,
                                  std::format("[{}] {} results:\n{}", step.kind, step.name, json));
        }
      }
    },
               step_var);
    exec_context.logger.Log(LogLevel::Trace, std::format("[{}] {}: completed", step_kind, step_name));
    if (succeeded) {
      ++matched_count;
    } else {
      ++no_match_count;
    }

    // Aggregate add_resource totals for the end summary.
    if (const auto* ar = std::get_if<dxp::AddResourceResults>(&step_results)) {
      added_totals.textures_added += ar->textures_added;
      added_totals.raw_resources_added += ar->raw_resources_added;
      added_totals.structured_resources_added += ar->structured_resources_added;
      added_totals.cbuffers_added += ar->cbuffers_added;
      added_totals.samplers_added += ar->samplers_added;
      added_totals.uavs_added += ar->uavs_added;
      added_totals.inputs_added += ar->inputs_added;
      added_totals.outputs_added += ar->outputs_added;
      added_totals.temps_added += ar->temps_added;
    }

    for (auto& [key, resource] : exec_context.resource_exports) {
      result.resource_usage[key] = std::move(resource);
    }
    for (auto& [key, immediate] : exec_context.immediate_exports) {
      result.immediate_values[key] = std::move(immediate);
    }

    exec_context.results[step_name] = std::any(step_results);

    StepReport step_report;
    step_report.name = step_name;
    step_report.results = std::move(step_results);
    step_report.success = succeeded;

    for (auto& [handle, binding] : exec_context.resource_bindings) {
      if (!handle.empty()) {
        result.new_bindings[handle] = binding;
      }
    }

    result.steps.push_back(std::move(step_report));

    // Stop-fast: required step with no match ends the run (not an error).
    if (step_required && !succeeded) {
      exec_context.logger.Log(LogLevel::Trace,
                              std::format("[{}] {}: stopping early (required)", step_kind, step_name));
      stopped_early_step = step_name;
      break;
    }
  }

  auto output = build_report(exec_context, input);
  if (!output) {
    exec_context.logger.Log(LogLevel::Error, "recipe failed: " + output.error() + InputHashSuffix(input));
    return std::unexpected(std::move(output.error()));
  }
  result.output_bytes = std::move(output->output_bytes);
  result.output_container = std::move(output->container_report);
  result.modified = exec_context.program_modified;

  // End-of-execution summary (Info): what was tested and what was accomplished.
  std::string summary = "recipe succeeded: " + std::to_string(steps.size()) + " steps — " + std::to_string(matched_count) + " matched, " + std::to_string(no_match_count) + " no-match, " + std::to_string(skipped_count) + " skipped";
  if (!stopped_early_step.empty()) {
    summary += " (stopped early at '" + stopped_early_step + "' — required)";
  }
  const std::string added = FormatAddedResources(added_totals);
  if (!added.empty()) {
    summary += "; added " + added;
  }
  summary += "; output " + std::to_string(result.output_bytes.size()) + " bytes (" + (result.modified ? "modified" : "unmodified") + ")";
  summary += InputHashSuffix(input);
  summary += ", output hash " + UpperHex32(dxp::utils::hash::ComputeCrc32(result.output_bytes));
  exec_context.logger.Log(LogLevel::Info, summary);
  return result;
}

}  // namespace dxp::detail
