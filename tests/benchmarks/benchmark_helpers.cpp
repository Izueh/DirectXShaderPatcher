#include "test/benchmarks/benchmark_helpers.hpp"

#include <fstream>
#include <stdexcept>

std::vector<uint8_t> LoadShaderBytes(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open shader file: " + path.string());
  }

  const std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<uint8_t> buffer(static_cast<size_t>(size));
  if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
    throw std::runtime_error("Failed to read shader file: " + path.string());
  }

  return buffer;
}

std::filesystem::path DefaultTestShaderPath() {
  return "tests/shaders/0x7AFF256C.ps_5_0.cso";
}

std::filesystem::path DefaultBenchNoopMovRecipePath() {
  return "tests/recipes/sm5_bench_noop_mov.recipe.yml";
}

std::filesystem::path DefaultBenchSequenceRecipePath() {
  return "tests/recipes/sm5_bench_sequence.recipe.yml";
}

std::filesystem::path DefaultBenchCombinedRecipePath() {
  return "tests/recipes/sm5_bench_combined.recipe.yml";
}
