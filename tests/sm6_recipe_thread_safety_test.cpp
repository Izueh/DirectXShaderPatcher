// Thread-safety and reuse tests for dxp::sm6::Recipe.
//
// Same contract as the SM5 test, plus it exercises the per-thread LLVMContext:
// every worker thread lazily creates its own thread-local LLVMContext on first
// Execute and reuses it across calls, matching DXC's thread-confined
// compilation model. Each Execute still performs a fresh full parse/serialize,
// so a shared recipe running concurrently over many shaders must produce
// byte-identical output to the single-threaded baseline.

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

const char* kRecipeText = R"YAML(
steps:
  - kind: apply_rule
    name: identity_pass
    required: false
    rewrite_mode: none
    condition:
      eq:
        lhs: enable_identity
        rhs: true
    rule:
        prune: true
        match:
          - opcode: TextureLoad
            capture: texture_load
            operands:
              - index: 1
                capture: texture_handle
              - index: 3
                capture: coord_x
        emit: []
    match_mode: first
)YAML";

dxp::sm6::Recipe LoadRecipe() {
  auto parse_result = dxp::sm6::Recipe::ParseFromText(kRecipeText, "inline-sm6-thread-safety-test");
  if (!parse_result) {
    std::cerr << "Failed to parse inline SM6 recipe: " << parse_result.error() << "\n";
    std::exit(1);
  }
  return std::move(parse_result.value());
}

bool RunSingle(const dxp::sm6::Recipe& recipe, const std::vector<uint8_t>& input, bool enable_identity,
               std::vector<uint8_t>& out, std::string& error) {
  dxp::PatchOptions options;
  options.SetEnv("enable_identity", enable_identity);
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
    std::cerr << "Usage: sm6_recipe_thread_safety_test <input.cso> [more.cso ...]\n";
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

  // Phase 1 — one shared recipe, concurrent execution over all shaders,
  // identical per-call options; every result must equal the single-threaded
  // baseline byte-for-byte (proves thread-local LLVMContext isolation + the
  // atomic validation cache under real parallelism).
  {
    const dxp::sm6::Recipe recipe = LoadRecipe();
    std::vector<std::vector<uint8_t>> baseline(shaders.size());
    for (size_t s = 0; s < shaders.size(); ++s) {
      std::string error;
      if (!RunSingle(recipe, shaders[s], true, baseline[s], error)) {
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
            if (!RunSingle(recipe, shaders[s], true, out, error)) {
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

  // Phase 2 — per-thread env via per-call PatchOptions: half the threads
  // enable the step (prune path), half disable it (no-op path); each must
  // match its single-threaded baseline for the same option value.
  {
    const dxp::sm6::Recipe recipe = LoadRecipe();
    std::vector<uint8_t> baseline_on;
    std::vector<uint8_t> baseline_off;
    std::string error;
    if (!RunSingle(recipe, shaders[0], true, baseline_on, error) || !RunSingle(recipe, shaders[0], false, baseline_off, error)) {
      std::cerr << "Phase 2 baseline failed: " << error << "\n";
      return 1;
    }

    std::atomic<bool> failed{false};
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(kThreadCount));
    for (int t = 0; t < kThreadCount; ++t) {
      const bool enable = (t % 2) == 0;
      threads.emplace_back([&, t, enable] {
        std::vector<uint8_t> out;
        std::string err;
        if (!RunSingle(recipe, shaders[0], enable, out, err)) {
          std::cerr << "Phase 2 execution failed (thread " << t << "): " << err << "\n";
          failed.store(true, std::memory_order_relaxed);
          return;
        }
        const auto& expected = enable ? baseline_on : baseline_off;
        if (out != expected) {
          std::cerr << "Phase 2 output mismatch (thread " << t << ", enable=" << enable << ")\n";
          failed.store(true, std::memory_order_relaxed);
        }
      });
    }
    for (auto& thread : threads) thread.join();
    if (failed.load()) return 1;
  }

  // Phase 3 — first-execute validation-cache race: all threads fire Execute on
  // a fresh, never-validated recipe at the same instant.
  {
    const dxp::sm6::Recipe baseline_recipe = LoadRecipe();
    std::vector<uint8_t> baseline;
    std::string error;
    if (!RunSingle(baseline_recipe, shaders[0], true, baseline, error)) {
      std::cerr << "Phase 3 baseline failed: " << error << "\n";
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
        if (!RunSingle(race_recipe, shaders[0], true, out, err)) {
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

  std::cout << "sm6_recipe_thread_safety_test passed (" << shaders.size() << " shaders, "
            << kThreadCount << " threads).\n";
  std::cout.flush();
  return 0;
}
