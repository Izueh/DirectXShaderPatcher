#include <benchmark/benchmark.h>

#include "d3d11TokenizedProgramFormat.hpp"

#include "dxp/sm5/Container.h"
#include "dxp/sm5/Model.h"
#include "dxp/sm5/Parse.h"
#include "dxp/sm5/Patch.h"
#include "dxp/sm5/Recipe.h"
#include "dxp/sm5/Serialize.h"
#include "dxp/sm5/Transforms.h"

#include "benchmark_helpers.h"

#include <filesystem>
#include <string>
#include <vector>

// ============================================================================
// Benchmark: BM_CollectMatches
// Measures single-instruction pattern matching on a loaded program.
// This is the O(N) match loop that runs once per rule per step.
// ============================================================================
static void BM_CollectMatches(benchmark::State& state) {
  const std::filesystem::path shaderPath = DefaultTestShaderPath();
  const auto containerBytes = LoadShaderBytes(shaderPath);

  dxp::sm5::Program program;
  std::string error;
  if (!ParseShaderToProgram(containerBytes, program, error)) {
    state.SkipWithError(error.c_str());
    return;
  }

  // Build a simple MOV match pattern — matches a common opcode.
  dxp::sm5::InstructionMatch pattern;
  pattern.Opcode = dxp::sm5::Opcode{static_cast<uint32_t>(
      D3D10_SB_OPCODE_MOV)};
  pattern.HasOpcode = true;

  dxp::sm5::RecipeContext ctx;

  for (auto _ : state) {
    auto matches = dxp::sm5::CollectMatches(program, pattern, ctx.captures);
    benchmark::DoNotOptimize(matches);
  }
}
BENCHMARK(BM_CollectMatches);

// ============================================================================
// Benchmark: BM_CollectSequenceMatches
// Measures multi-pattern sequence matching on a loaded program.
// This is the O(N×M) match loop — worst case for long instruction streams.
// ============================================================================
static void BM_CollectSequenceMatches(benchmark::State& state) {
  const std::filesystem::path shaderPath = DefaultTestShaderPath();
  const auto containerBytes = LoadShaderBytes(shaderPath);

  dxp::sm5::Program program;
  std::string error;
  if (!ParseShaderToProgram(containerBytes, program, error)) {
    state.SkipWithError(error.c_str());
    return;
  }

  // Build a two-instruction sequence pattern: MOV followed by MOV.
  std::vector<dxp::sm5::InstructionMatch> patterns;
  patterns.reserve(2);

  for (int i = 0; i < 2; ++i) {
    dxp::sm5::InstructionMatch p;
    p.Opcode = dxp::sm5::Opcode{static_cast<uint32_t>(
        D3D10_SB_OPCODE_MOV)};
    p.HasOpcode = true;
    patterns.push_back(std::move(p));
  }

  dxp::sm5::RecipeContext ctx;

  for (auto _ : state) {
    auto matches = dxp::sm5::CollectSequenceMatches(program, patterns, ctx.captures);
    benchmark::DoNotOptimize(matches);
  }
}
BENCHMARK(BM_CollectSequenceMatches);

// ============================================================================
// Benchmark: BM_RefreshDeclarations
// Measures full declaration refresh (Resources, Samplers, CBuffers, etc.)
// Called once per recipe step when refresh_declarations is set.
// ============================================================================
static void BM_RefreshDeclarations(benchmark::State& state) {
  const std::filesystem::path shaderPath = DefaultTestShaderPath();
  const auto containerBytes = LoadShaderBytes(shaderPath);

  dxp::sm5::Program program;
  std::string error;
  if (!ParseShaderToProgram(containerBytes, program, error)) {
    state.SkipWithError(error.c_str());
    return;
  }

  // Make a working copy so we don't mutate the original.
  dxp::sm5::Program workingProgram = program;

  for (auto _ : state) {
    dxp::sm5::Program copy = workingProgram;
    dxp::sm5::RefreshDeclarations(copy);
    benchmark::DoNotOptimize(copy);
  }
}
BENCHMARK(BM_RefreshDeclarations);

// ============================================================================
// Benchmark: BM_RewriteAndRebuild
// Measures the worst-case scenario: MatchAll with many matches, each
// triggering a rewrite and a RebuildMetadata call.
// ============================================================================
static void BM_RewriteAndRebuild(benchmark::State& state) {
  const std::filesystem::path shaderPath = DefaultTestShaderPath();
  const auto containerBytes = LoadShaderBytes(shaderPath);

  dxp::sm5::Program program;
  std::string error;
  if (!ParseShaderToProgram(containerBytes, program, error)) {
    state.SkipWithError(error.c_str());
    return;
  }

  // Build a MOV match pattern.
  dxp::sm5::InstructionMatch pattern;
  pattern.Opcode = dxp::sm5::Opcode{static_cast<uint32_t>(
      D3D10_SB_OPCODE_MOV)};
  pattern.HasOpcode = true;

  dxp::sm5::RecipeContext ctx;
  auto matches = dxp::sm5::CollectMatches(program, pattern, ctx.captures);
  if (matches.empty()) {
    state.SkipWithError("No MOV instructions found in test shader");
    return;
  }

  // Build a replacement MOV instruction (noop rewrite).
  dxp::sm5::Instruction replacement = program.Instructions[0];
  replacement.RawTokens.clear();

  std::vector<dxp::sm5::RewriteAction> actions;
  actions.reserve(matches.size());
  for (const auto& match : matches) {
    dxp::sm5::RewriteAction action;
    action.Type = dxp::sm5::RewriteActionType::ReplaceRange;
    action.RangeStart = match.RangeStartIndex;
    action.RangeEnd = match.RangeEndIndex;
    action.NewInstructions.push_back(replacement);
    actions.push_back(std::move(action));
  }

  // Sort actions by index descending (as the real pipeline does).
  std::sort(actions.begin(), actions.end(),
            [](const dxp::sm5::RewriteAction& lhs,
               const dxp::sm5::RewriteAction& rhs) {
              return lhs.RangeStart > rhs.RangeStart;
            });

  for (auto _ : state) {
    dxp::sm5::Program workingProgram = program;
    bool ok = dxp::sm5::ApplyRewriteActions(workingProgram, actions);
    benchmark::DoNotOptimize(ok);
  }
}
BENCHMARK(BM_RewriteAndRebuild);

// ============================================================================
// Benchmark: BM_RebuildShaderChunk
// Measures full program-to-bytes serialization.
// Called after every patch execution.
// ============================================================================
static void BM_RebuildShaderChunk(benchmark::State& state) {
  const std::filesystem::path shaderPath = DefaultTestShaderPath();
  const auto containerBytes = LoadShaderBytes(shaderPath);

  dxp::sm5::Program program;
  std::string error;
  if (!ParseShaderToProgram(containerBytes, program, error)) {
    state.SkipWithError(error.c_str());
    return;
  }

  for (auto _ : state) {
    std::vector<uint8_t> outBytes;
    bool ok = dxp::sm5::RebuildShaderChunk(program, outBytes);
    benchmark::DoNotOptimize(ok);
    benchmark::DoNotOptimize(outBytes);
  }
}
BENCHMARK(BM_RebuildShaderChunk);

// ============================================================================
// Benchmark: BM_SerializeDxbcContainer
// Measures full container serialization (chunks + header + offset table).
// Called once per patch execution.
// ============================================================================
static void BM_SerializeDxbcContainer(benchmark::State& state) {
  const std::filesystem::path shaderPath = DefaultTestShaderPath();
  const auto containerBytes = LoadShaderBytes(shaderPath);

  dxp::sm5::Container container;
  if (!dxp::sm5::ParseDxbcContainer(containerBytes, container)) {
    state.SkipWithError("Failed to parse DXBC container");
    return;
  }

  for (auto _ : state) {
    std::vector<uint8_t> outBytes;
    bool ok = dxp::sm5::SerializeDxbcContainer(container, outBytes);
    benchmark::DoNotOptimize(ok);
    benchmark::DoNotOptimize(outBytes);
  }
}
BENCHMARK(BM_SerializeDxbcContainer);

// ============================================================================
// Benchmark: BM_RecipeCompile
// Measures recipe compilation cost (CompileRule for all rules).
// Recipes are compiled once from YAML and executed many times,
// so this benchmark isolates that one-time cost.
// ============================================================================
static void BM_RecipeCompile(benchmark::State& state) {
  const dxp::sm5::Recipe recipe = BuildNoopMovRecipe();

  const std::filesystem::path shaderPath = DefaultTestShaderPath();
  const auto containerBytes = LoadShaderBytes(shaderPath);

  dxp::sm5::Program program;
  std::string error;
  if (!ParseShaderToProgram(containerBytes, program, error)) {
    state.SkipWithError(error.c_str());
    return;
  }

  for (auto _ : state) {
    dxp::sm5::RecipeContext ctx;
    auto result = dxp::sm5::ExecuteRecipe(program, recipe, ctx);
    benchmark::DoNotOptimize(result);
  }
}
BENCHMARK(BM_RecipeCompile);

// ============================================================================
// Benchmark: BM_PatchContainer_end_to_end
// Measures the full patch pipeline: parse → execute recipe → serialize.
// Recipe is pre-compiled outside the loop to match real-world usage
// (YAML parsed once, then executed many times).
// This is the real-world throughput measurement.
// ============================================================================
static void BM_PatchContainer_end_to_end(benchmark::State& state) {
  const std::filesystem::path shaderPath = DefaultTestShaderPath();
  const auto containerBytes = LoadShaderBytes(shaderPath);

  // Pre-compile recipe outside the loop (one-time cost).
  const dxp::sm5::Recipe recipe = BuildNoopMovRecipe();

  // Parse once outside the loop to isolate execution cost.
  dxp::sm5::Program program;
  std::string error;
  if (!ParseShaderToProgram(containerBytes, program, error)) {
    state.SkipWithError(error.c_str());
    return;
  }

  for (auto _ : state) {
    // Re-parse each iteration to measure the full pipeline.
    dxp::sm5::Program iterProgram;
    if (!ParseShaderToProgram(containerBytes, iterProgram, error)) {
      state.SkipWithError(error.c_str());
      break;
    }
    dxp::sm5::RecipeContext ctx;
    auto result = dxp::sm5::ExecuteRecipe(iterProgram, recipe, ctx);

    // Serialize to complete the full pipeline measurement.
    std::vector<uint8_t> shaderBytes;
    dxp::sm5::RebuildShaderChunk(iterProgram, shaderBytes);

    dxp::sm5::Container container;
    if (dxp::sm5::ParseDxbcContainer(containerBytes, container)) {
      dxp::sm5::DxbcChunk *chunk = container.GetShaderChunk();
      if (chunk) chunk->Data = std::move(shaderBytes);
      std::vector<uint8_t> outBytes;
      dxp::sm5::SerializeDxbcContainer(container, outBytes);
      benchmark::DoNotOptimize(outBytes);
    }
    benchmark::DoNotOptimize(result);
  }
}
BENCHMARK(BM_PatchContainer_end_to_end);

// ============================================================================
// Benchmark: BM_PatchSequenceMatch_end_to_end
// Measures end-to-end patching with sequence matching (harder path).
// Recipe is pre-compiled outside the loop.
// ============================================================================
static void BM_PatchSequenceMatch_end_to_end(benchmark::State& state) {
  const std::filesystem::path shaderPath = DefaultTestShaderPath();
  const auto containerBytes = LoadShaderBytes(shaderPath);

  // Pre-compile recipe outside the loop (one-time cost).
  const dxp::sm5::Recipe recipe = BuildSequenceMatchRecipe();

  for (auto _ : state) {
    dxp::sm5::Program iterProgram;
    std::string error;
    if (!ParseShaderToProgram(containerBytes, iterProgram, error)) {
      state.SkipWithError(error.c_str());
      break;
    }
    dxp::sm5::RecipeContext ctx;
    auto result = dxp::sm5::ExecuteRecipe(iterProgram, recipe, ctx);

    // Serialize to complete the full pipeline measurement.
    std::vector<uint8_t> shaderBytes;
    dxp::sm5::RebuildShaderChunk(iterProgram, shaderBytes);

    dxp::sm5::Container container;
    if (dxp::sm5::ParseDxbcContainer(containerBytes, container)) {
      dxp::sm5::DxbcChunk *chunk = container.GetShaderChunk();
      if (chunk) chunk->Data = std::move(shaderBytes);
      std::vector<uint8_t> outBytes;
      dxp::sm5::SerializeDxbcContainer(container, outBytes);
      benchmark::DoNotOptimize(outBytes);
    }
    benchmark::DoNotOptimize(result);
  }
}
BENCHMARK(BM_PatchSequenceMatch_end_to_end);

// Register a custom main so we can pass benchmark flags.
BENCHMARK_MAIN();
