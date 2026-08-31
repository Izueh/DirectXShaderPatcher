// Thread-safety test for dxp::sm6::Recipe with a real emit pipeline.
//
// Companion to sm6_recipe_thread_safety_test (which uses `emit: []`). This
// variant runs a multi-emit rewrite chain under the same concurrency contract:
//   CBufferLoadLegacy -> extractvalue -> urem -> TextureLoad (replace_captured)
// Every worker thread therefore exercises, concurrently on its own per-thread
// LLVMContext and DxilModule, the code paths that mutate per-instance state:
//   - hlsl::OP::GetOpFunc (overload-cache insertion + Function creation)
//   - IRBuilder insertion with dominance validation (ValidateEmitDominance)
//   - replace_captured rewiring (use-scan + rewire)
// As before, one shared recipe runs concurrently over all shaders and every
// result must be byte-identical to the single-threaded baseline.

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include "dxp/PatchOptions.hpp"
#include "dxp/sm6/Recipe.hpp"
#include "tests/helper/TestHelper.hpp"

namespace {

constexpr int kThreadCount = 16;
constexpr int kIterations = 25;

// Multi-emit chain rewiring a matched TextureLoad through a cbuffer-derived
// slice index, all wired explicitly via replace_captured. Matches the
// 0x56C468C3.cs_6_6.cso compute shader (same recipe shape as
// sm6_blue_noise_emit.cpp, which applies it single-threaded).
const char* kRecipeText = R"YAML(
steps:
  - kind: add_resource
    name: add_resources
    textures:
      - handle: fast_noise
        kind: Texture2DArray
        space: 50
    cbuffers:
      - handle: frame_constants
        space: 0
        size: 16
        type: ISFastFrameConstants
        fields:
          - name: FrameIndex
            type: U32
            width: 1
            offset: 0
  - kind: apply_rule
    name: blue_noise_scalar_slice
    required: false
    rewrite_mode: replace
    rule:
        prune: true
        match:
          - opcode: TextureLoad
            capture: texture_load
            operands:
              - index: 1
                kind: resource
                resource_class: SRV
                resource_kind: Texture2D
                register_index: 7
                space: 0
              - index: 3
                kind: call
                capture: coord_x
              - index: 4
                kind: call
                capture: coord_y
        emit:
          - opcode: CBufferLoadLegacy
            result_component_type: I32
            name: frame_load
            operands:
              - index: 1
                kind: resource
                handle: frame_constants
              - index: 2
                kind: constant
                constant_int_values: [0]
          - aggregate: frame_load
            extract_index: 0
            name: frame_index
          - opcode: urem
            result_component_type: I32
            name: slice_index
            operands:
              - index: 0
                kind: call
                capture: frame_index
              - index: 1
                kind: constant
                constant_int_values: [32]
          - opcode: TextureLoad
            result_component_type: F32
            name: noise_load
            replace_captured: texture_load
            operands:
              - index: 1
                kind: resource
                handle: fast_noise
              - index: 2
                kind: constant
                constant_int_values: [0]
              - index: 3
                kind: call
                capture: coord_x
              - index: 4
                kind: call
                capture: coord_y
              - index: 5
                kind: call
                capture: slice_index
              - index: 6
              - index: 7
              - index: 8
    match_mode: match_all
)YAML";

dxp::sm6::Recipe LoadRecipe() {
  auto parse_result = dxp::sm6::Recipe::ParseFromText(kRecipeText, "inline-sm6-emit-thread-safety-test");
  if (!parse_result) {
    std::cerr << "Failed to parse inline SM6 emit recipe: " << parse_result.error() << "\n";
    std::exit(1);
  }
  return std::move(parse_result.value());
}

bool RunSingle(const dxp::sm6::Recipe& recipe, const std::vector<uint8_t>& input,
               std::vector<uint8_t>& out, std::string& error) {
  try {
    dxp::PatchOptions options;
    auto result = recipe.Execute(input, options);
    if (!result) {
      error = result.error();
      return false;
    }
    out = std::move(result.value().output_bytes);
    return true;
  } catch (const std::exception& e) {
    // An exception escaping Recipe::Execute on a worker thread would call
    // std::terminate (silent fail-fast crash); surface it as a test failure.
    error = std::string("uncaught exception from Execute: ") + e.what();
    return false;
  } catch (...) {
    error = "uncaught non-standard exception from Execute";
    return false;
  }
}

}  // namespace

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc < 2) {
    std::cerr << "Usage: sm6_emit_thread_safety_test <input.cso> [more.cso ...]\n";
    return 1;
  }

  const ScopedCoInitialize coinit;

  std::vector<std::vector<uint8_t>> shaders;
  shaders.reserve(static_cast<size_t>(argc) - 1);
  for (int i = 1; i < argc; ++i) {
    std::vector<uint8_t> bytes;
    if (!ReadFile(args[i], bytes)) {
      std::cerr << "Failed to read input shader: " << args[i] << "\n";
      return 1;
    }
    shaders.push_back(std::move(bytes));
  }

  // Phase 1 — shared recipe, concurrent emit-rewrites over all shaders; every
  // result must equal the single-threaded baseline byte-for-byte (proves the
  // per-thread OP-cache / IRBuilder / dominance-validation paths are isolated).
  {
    const dxp::sm6::Recipe recipe = LoadRecipe();
    std::vector<std::vector<uint8_t>> baseline(shaders.size());
    unsigned total_matches = 0;
    for (size_t s = 0; s < shaders.size(); ++s) {
      std::string error;
      if (!RunSingle(recipe, shaders[s], baseline[s], error)) {
        std::cerr << "Phase 1 baseline failed (shader " << s << "): " << error << "\n";
        return 1;
      }
      auto match_report = recipe.Execute(shaders[s]);
      if (!match_report) {
        std::cerr << "Phase 1 match-count execution failed (shader " << s << "): "
                  << match_report.error() << "\n";
        return 1;
      }
      for (const auto& step : match_report.value().steps) {
        if (const auto* ar = std::get_if<dxp::ApplyRuleResults>(&step.results)) total_matches += ar->match_count;
      }
    }
    if (total_matches == 0) {
      std::cerr << "Test is ineffective: emit rule matched nothing on any input shader.\n";
      return 1;
    }

    std::atomic<bool> failed{false};
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(kThreadCount));
    for (int t = 0; t < kThreadCount; ++t) {
      threads.emplace_back([&, t] {
        for (int iter = 0; iter < kIterations && !failed.load(std::memory_order_relaxed); ++iter) {
          for (size_t s = 0; s < shaders.size(); ++s) {
            std::vector<uint8_t> out;
            std::string error;
            if (!RunSingle(recipe, shaders[s], out, error)) {
              std::cerr << "Phase 1 execution failed (thread " << t << ", shader " << s
                        << "): " << error << "\n";
              failed.store(true, std::memory_order_relaxed);
              return;
            }
            if (out != baseline[s]) {
              std::cerr << "Phase 1 output mismatch (thread " << t << ", shader " << s
                        << ", iter " << iter << ")\n";
              failed.store(true, std::memory_order_relaxed);
              return;
            }
          }
        }
      });
    }
    for (auto& thread : threads) thread.join();
    if (failed.load()) return 1;
  }

  // Phase 2 — first-execute validation-cache race with emits: all threads fire
  // Execute on a fresh, never-validated recipe at the same instant.
  {
    const dxp::sm6::Recipe baseline_recipe = LoadRecipe();
    std::vector<uint8_t> baseline;
    std::string error;
    if (!RunSingle(baseline_recipe, shaders[0], baseline, error)) {
      std::cerr << "Phase 2 baseline failed: " << error << "\n";
      return 1;
    }

    const dxp::sm6::Recipe race_recipe = LoadRecipe();  // never validated yet
    std::atomic<bool> start{false};
    std::atomic<bool> failed{false};
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(kThreadCount));
    for (int t = 0; t < kThreadCount; ++t) {
      threads.emplace_back([&, t] {
        while (!start.load(std::memory_order_acquire)) {
          std::this_thread::yield();
        }
        std::vector<uint8_t> out;
        std::string err;
        if (!RunSingle(race_recipe, shaders[0], out, err)) {
          std::cerr << "Phase 2 execution failed (thread " << t << "): " << err << "\n";
          failed.store(true, std::memory_order_relaxed);
          return;
        }
        if (out != baseline) {
          std::cerr << "Phase 2 output mismatch (thread " << t << ")\n";
          failed.store(true, std::memory_order_relaxed);
        }
      });
    }
    start.store(true, std::memory_order_release);  // release the gate
    for (auto& thread : threads) thread.join();
    if (failed.load()) return 1;
  }

  std::cout << "sm6_emit_thread_safety_test passed (" << shaders.size() << " shaders, "
            << kThreadCount << " threads).\n";
  std::cout.flush();
  return 0;
}
