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






static void BM_CollectMatches(benchmark::State& state) {
  const std::filesystem::path shaderPath = DefaultTestShaderPath();
  const auto containerBytes = LoadShaderBytes(shaderPath);

  dxp::sm5::Program program;
  std::string error;
  if (!ParseShaderToProgram(containerBytes, program, error)) {
    state.SkipWithError(error.c_str());
    return;
  }


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






static void BM_CollectSequenceMatches(benchmark::State& state) {
  const std::filesystem::path shaderPath = DefaultTestShaderPath();
  const auto containerBytes = LoadShaderBytes(shaderPath);

  dxp::sm5::Program program;
  std::string error;
  if (!ParseShaderToProgram(containerBytes, program, error)) {
    state.SkipWithError(error.c_str());
    return;
  }


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






static void BM_RefreshDeclarations(benchmark::State& state) {
  const std::filesystem::path shaderPath = DefaultTestShaderPath();
  const auto containerBytes = LoadShaderBytes(shaderPath);

  dxp::sm5::Program program;
  std::string error;
  if (!ParseShaderToProgram(containerBytes, program, error)) {
    state.SkipWithError(error.c_str());
    return;
  }


  dxp::sm5::Program workingProgram = program;

  for (auto _ : state) {
    dxp::sm5::Program copy = workingProgram;
    dxp::sm5::RefreshDeclarations(copy);
    benchmark::DoNotOptimize(copy);
  }
}
BENCHMARK(BM_RefreshDeclarations);






static void BM_RewriteAndRebuild(benchmark::State& state) {
  const std::filesystem::path shaderPath = DefaultTestShaderPath();
  const auto containerBytes = LoadShaderBytes(shaderPath);

  dxp::sm5::Program program;
  std::string error;
  if (!ParseShaderToProgram(containerBytes, program, error)) {
    state.SkipWithError(error.c_str());
    return;
  }


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








static void BM_PatchContainer_end_to_end(benchmark::State& state) {
  const std::filesystem::path shaderPath = DefaultTestShaderPath();
  const auto containerBytes = LoadShaderBytes(shaderPath);


  const dxp::sm5::Recipe recipe = BuildNoopMovRecipe();


  dxp::sm5::Program program;
  std::string error;
  if (!ParseShaderToProgram(containerBytes, program, error)) {
    state.SkipWithError(error.c_str());
    return;
  }

  for (auto _ : state) {

    dxp::sm5::Program iterProgram;
    if (!ParseShaderToProgram(containerBytes, iterProgram, error)) {
      state.SkipWithError(error.c_str());
      break;
    }
    dxp::sm5::RecipeContext ctx;
    auto result = dxp::sm5::ExecuteRecipe(iterProgram, recipe, ctx);


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






static void BM_PatchSequenceMatch_end_to_end(benchmark::State& state) {
  const std::filesystem::path shaderPath = DefaultTestShaderPath();
  const auto containerBytes = LoadShaderBytes(shaderPath);


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


BENCHMARK_MAIN();
