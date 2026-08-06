#include <benchmark/benchmark.h>

#include <filesystem>
#include <string>
#include <vector>

#include "dxp/Condition.hpp"
#include "dxp/sm5/ExecutionContext.hpp"
#include "dxp/sm5/Recipe.hpp"
#include "dxp/sm5/ShaderProgram.hpp"
#include "dxp/sm5/step/CheckOpcodeCountStep.hpp"
#include "dxp/sm5/step/CheckResourceCountStep.hpp"
#include "test/benchmarks/benchmark_helpers.hpp"

using dxp::ConditionNode;

// ---------------------------------------------------------------------------
// Shader parsing
// ---------------------------------------------------------------------------

static void BmSm5ParseShader(benchmark::State& state) {
  const std::filesystem::path shader_path = DefaultTestShaderPath();
  const auto container_bytes = LoadShaderBytes(shader_path);

  for (auto _ : state) {
    // Benchmarks load a known-valid shader; FromBytes returns expected (value() throws on malformed input).
    auto program = dxp::sm5::ShaderProgram::FromBytes(containerBytes).value();
    benchmark::DoNotOptimize(program);
  }
}
BENCHMARK(bm_sm5_parse_shader);

// ---------------------------------------------------------------------------
// Recipe parsing
// ---------------------------------------------------------------------------

static void BmSm5ParseRecipe(benchmark::State& state) {
  const std::filesystem::path recipe_path = DefaultBenchNoopMovRecipePath();
  auto parse_result = dxp::sm5::Recipe::ParseFromFile(recipe_path.string());
  if (parse_result.error.empty()) {
    std::string recipe_text;
    std::ifstream RecipeFile(recipePath, std::ios::ate);
    if (RecipeFile.is_open()) {
      file.seekg(0);
      recipeText.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }
    for (auto _ : state) {
      auto result = dxp::sm5::Recipe::ParseFromText(recipeText, recipePath.string());
      benchmark::DoNotOptimize(result);
    }
  } else {
    state.SkipWithError(parseResult.error.c_str());
  }
}
BENCHMARK(bm_sm5_parse_recipe);

// ---------------------------------------------------------------------------
// No-op MOV patch (single-instruction match + rewrite)
// ---------------------------------------------------------------------------

static void BmSm5NoopPatch(benchmark::State& state) {
  const std::filesystem::path shader_path = DefaultTestShaderPath();
  const std::filesystem::path recipe_path = DefaultBenchNoopMovRecipePath();

  auto container_bytes = LoadShaderBytes(shader_path);
  auto parse_result = dxp::sm5::Recipe::ParseFromFile(recipe_path.string());
  if (!parseResult.error.empty()) {
    state.SkipWithError(parseResult.error.c_str());
    return;
  }

  for (auto _ : state) {
    auto report = parseResult.Recipe.Execute(containerBytes);
    benchmark::DoNotOptimize(report);
  }
}
BENCHMARK(bm_sm5_noop_patch);

// ---------------------------------------------------------------------------
// Sequence match patch (two-instruction chain)
// ---------------------------------------------------------------------------

static void BmSm5SequencePatch(benchmark::State& state) {
  const std::filesystem::path shader_path = DefaultTestShaderPath();
  const std::filesystem::path recipe_path = DefaultBenchSequenceRecipePath();

  auto container_bytes = LoadShaderBytes(shader_path);
  auto parse_result = dxp::sm5::Recipe::ParseFromFile(recipe_path.string());
  if (!parseResult.error.empty()) {
    state.SkipWithError(parseResult.error.c_str());
    return;
  }

  for (auto _ : state) {
    auto report = parseResult.Recipe.Execute(containerBytes);
    benchmark::DoNotOptimize(report);
  }
}
BENCHMARK(bm_sm5_sequence_patch);

// ---------------------------------------------------------------------------
// Combined patch (check_shader_version + check_opcode_count + check_resource_count + apply_rules)
// ---------------------------------------------------------------------------

static void BmSm5CombinedPatch(benchmark::State& state) {
  const std::filesystem::path shader_path = DefaultTestShaderPath();
  const std::filesystem::path recipe_path = DefaultBenchCombinedRecipePath();

  auto container_bytes = LoadShaderBytes(shader_path);
  auto parse_result = dxp::sm5::Recipe::ParseFromFile(recipe_path.string());
  if (!parseResult.error.empty()) {
    state.SkipWithError(parseResult.error.c_str());
    return;
  }

  for (auto _ : state) {
    auto report = parseResult.Recipe.Execute(containerBytes);
    benchmark::DoNotOptimize(report);
  }
}
BENCHMARK(bm_sm5_combined_patch);

// ---------------------------------------------------------------------------
// End-to-end: parse shader + parse recipe + execute + serialize
// ---------------------------------------------------------------------------

static void BmSm5EndToEnd(benchmark::State& state) {
  const std::filesystem::path shader_path = DefaultTestShaderPath();
  const std::filesystem::path recipe_path = DefaultBenchNoopMovRecipePath();

  auto container_bytes = LoadShaderBytes(shader_path);
  auto recipe_parse = dxp::sm5::Recipe::ParseFromFile(recipe_path.string());
  if (recipe_parse.error.empty()) {
    for (auto _ : state) {
      auto report = recipeParse.Recipe.Execute(containerBytes);
      benchmark::DoNotOptimize(report);
      benchmark::DoNotOptimize(report.output_bytes);
    }
  } else {
    state.SkipWithError(recipeParse.error.c_str());
  }
}
BENCHMARK(bm_sm5_end_to_end);

// ---------------------------------------------------------------------------
// CheckOpcodeCountStep benchmark
// ---------------------------------------------------------------------------

static void BmSm5CheckOpcodeCount(benchmark::State& state) {
  const std::filesystem::path shader_path = DefaultTestShaderPath();
  auto container_bytes = LoadShaderBytes(shader_path);

  // Benchmarks load a known-valid shader; FromBytes returns expected (value() throws on malformed input).
  auto program = dxp::sm5::ShaderProgram::FromBytes(containerBytes).value();
  dxp::sm5::ExecutionContext ctx;
  ctx.program = std::move(program);

  auto step = dxp::sm5::step::CheckOpcodeCountStep{"count_check", {"mov", "add", "mul", "frc"}, true, ConditionNode{}};

  for (auto _ : state) {
    auto result = step.Execute(ctx);
    benchmark::DoNotOptimize(result);
  }
}
BENCHMARK(bm_sm5_check_opcode_count);

// ---------------------------------------------------------------------------
// CheckResourceCountStep benchmark
// ---------------------------------------------------------------------------

static void BmSm5CheckResourceCount(benchmark::State& state) {
  const std::filesystem::path shader_path = DefaultTestShaderPath();
  auto container_bytes = LoadShaderBytes(shader_path);

  // Benchmarks load a known-valid shader; FromBytes returns expected (value() throws on malformed input).
  auto program = dxp::sm5::ShaderProgram::FromBytes(containerBytes).value();
  dxp::sm5::ExecutionContext ctx;
  ctx.program = std::move(program);

  auto step = dxp::sm5::step::CheckResourceCountStep{"resource_check", true, ConditionNode{}};

  for (auto _ : state) {
    auto result = step.Execute(ctx);
    benchmark::DoNotOptimize(result);
  }
}
BENCHMARK(bm_sm5_check_resource_count);

// ---------------------------------------------------------------------------
// Combined check steps benchmark
// ---------------------------------------------------------------------------

static void BmSm5CheckSteps(benchmark::State& state) {
  const std::filesystem::path shader_path = DefaultTestShaderPath();
  auto container_bytes = LoadShaderBytes(shader_path);

  // Benchmarks load a known-valid shader; FromBytes returns expected (value() throws on malformed input).
  auto program = dxp::sm5::ShaderProgram::FromBytes(containerBytes).value();
  dxp::sm5::ExecutionContext ctx;
  ctx.program = std::move(program);

  auto opcode_step = dxp::sm5::step::CheckOpcodeCountStep{"count_opcodes", {"mov", "add", "mul"}, true, ConditionNode{}};
  auto resource_step = dxp::sm5::step::CheckResourceCountStep {"count_resources", true, ConditionNode{});

    for (auto _ : state) {
      auto r1 = opcodeStep.Execute(ctx);
      auto r2 = resourceStep.Execute(ctx);
      benchmark::DoNotOptimize(r1);
      benchmark::DoNotOptimize(r2);
    }
  }
  BENCHMARK(bm_sm5_check_steps);

  BenchmarkMain();
