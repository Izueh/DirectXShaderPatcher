// Thread-safety and reuse tests for dxp::sm5::Recipe.
//
// Verifies the advertised contract:
//  * a recipe is immutable once shared; concurrent Execute() calls on one
//    shared recipe are race-free and produce byte-identical output to a
//    single-threaded baseline;
//  * per-thread environment variation goes through per-call PatchOptions
//    (merged into each call's fresh context) and is deterministic under
//    concurrency;
//  * the lazy validation cache (mutable atomic flag) is race-free when many
//    threads fire Execute() on a fresh, never-validated recipe at once.

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include "dxp/PatchOptions.hpp"
#include "dxp/sm5/Recipe.hpp"
#include "tests/helper/TestHelper.hpp"

namespace {

constexpr int kThreadCount = 16;
constexpr int kIterations = 25;
constexpr uint32_t kFloatOneBits = 0x3F800000u;  // 1.0f

const char* kRecipeText = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: rewrite_with_runtime_vars
    condition:
      eq:
        lhs: enable_noise_patch
        rhs: true
    rule:
      match:
        - opcode: mul
          operands:
            - capture: dst
            - capture: src
      emit:
        - opcode: mov
          operands:
            - capture: dst
            - type: immediate32
              immediates_u32: [frame_seed]
)YAML";

dxp::sm5::Recipe LoadRecipe() {
  auto parse_result = dxp::sm5::Recipe::ParseFromText(kRecipeText, "inline-sm5-thread-safety-test");
  if (!parse_result) {
    std::cerr << "Failed to parse inline SM5 recipe: " << parse_result.error() << "\n";
    std::exit(1);
  }
  return std::move(parse_result.value());
}

bool RunSingle(const dxp::sm5::Recipe& recipe, const std::vector<uint8_t>& input, uint32_t frame_seed,
               std::vector<uint8_t>& out, std::string& error) {
  dxp::PatchOptions options;
  options.SetEnv("enable_noise_patch", true);
  options.SetEnv("frame_seed", frame_seed);
  auto result = recipe.Execute(input, options);
  if (!result) {
    error = result.error();
    return false;
  }
  out = std::move(result.value().output_bytes);
  return true;
}

}  // namespace

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc < 2) {
    std::cerr << "Usage: sm5_recipe_thread_safety_test <input.cso> [more.cso ...]\n";
    return 1;
  }

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

  // Phase 1 — one shared recipe, concurrent execution over all shaders,
  // identical per-call options; every result must equal the single-threaded
  // baseline byte-for-byte.
  {
    const dxp::sm5::Recipe recipe = LoadRecipe();
    std::vector<std::vector<uint8_t>> baseline(shaders.size());
    for (size_t s = 0; s < shaders.size(); ++s) {
      std::string error;
      if (!RunSingle(recipe, shaders[s], kFloatOneBits, baseline[s], error)) {
        std::cerr << "Phase 1 baseline failed (shader " << s << "): " << error << "\n";
        return 1;
      }
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
            if (!RunSingle(recipe, shaders[s], kFloatOneBits, out, error)) {
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

  // Phase 2 — per-thread env via per-call PatchOptions: each thread uses a
  // distinct frame_seed; its output must equal the single-threaded baseline
  // for that same seed.
  {
    const dxp::sm5::Recipe recipe = LoadRecipe();
    std::vector<uint32_t> seeds;
    seeds.reserve(static_cast<size_t>(kThreadCount));
    for (int t = 0; t < kThreadCount; ++t) {
      seeds.push_back(kFloatOneBits + static_cast<uint32_t>(t) + 1);
    }

    std::vector<std::vector<uint8_t>> baseline(seeds.size());
    for (size_t i = 0; i < seeds.size(); ++i) {
      std::string error;
      if (!RunSingle(recipe, shaders[0], seeds[i], baseline[i], error)) {
        std::cerr << "Phase 2 baseline failed (seed " << i << "): " << error << "\n";
        return 1;
      }
    }

    std::atomic<bool> failed{false};
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(kThreadCount));
    for (int t = 0; t < kThreadCount; ++t) {
      threads.emplace_back([&, t] {
        std::vector<uint8_t> out;
        std::string error;
        if (!RunSingle(recipe, shaders[0], seeds[static_cast<size_t>(t)], out, error)) {
          std::cerr << "Phase 2 execution failed (thread " << t << "): " << error << "\n";
          failed.store(true, std::memory_order_relaxed);
          return;
        }
        if (out != baseline[static_cast<size_t>(t)]) {
          std::cerr << "Phase 2 output mismatch (thread " << t << ")\n";
          failed.store(true, std::memory_order_relaxed);
        }
      });
    }
    for (auto& thread : threads) thread.join();
    if (failed.load()) return 1;
  }

  // Phase 3 — first-execute validation-cache race: all threads fire Execute on
  // a fresh, never-validated recipe at the same instant. Before the atomic
  // validation flag this was a data race on the mutable cache; now all threads
  // must succeed and agree with an independent baseline recipe.
  {
    const dxp::sm5::Recipe baseline_recipe = LoadRecipe();
    std::vector<uint8_t> baseline;
    std::string error;
    if (!RunSingle(baseline_recipe, shaders[0], kFloatOneBits, baseline, error)) {
      std::cerr << "Phase 3 baseline failed: " << error << "\n";
      return 1;
    }

    const dxp::sm5::Recipe race_recipe = LoadRecipe();  // never validated yet
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
        if (!RunSingle(race_recipe, shaders[0], kFloatOneBits, out, err)) {
          std::cerr << "Phase 3 execution failed (thread " << t << "): " << err << "\n";
          failed.store(true, std::memory_order_relaxed);
          return;
        }
        if (out != baseline) {
          std::cerr << "Phase 3 output mismatch (thread " << t << ")\n";
          failed.store(true, std::memory_order_relaxed);
        }
      });
    }
    start.store(true, std::memory_order_release);  // release the gate
    for (auto& thread : threads) thread.join();
    if (failed.load()) return 1;
  }

  std::cout << "sm5_recipe_thread_safety_test passed (" << shaders.size() << " shaders, "
            << kThreadCount << " threads).\n";
  std::cout.flush();
  return 0;
}
