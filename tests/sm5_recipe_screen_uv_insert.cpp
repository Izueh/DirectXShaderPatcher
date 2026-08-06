#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <span>
#include <vector>

#include "dxp/sm5/Recipe.hpp"
#include "tests/helper/TestHelper.hpp"

#ifndef NDEBUG
#include "tests/helper/StackTraceHelper.hpp"
#endif

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
#ifndef NDEBUG
  InstallCrashHandler();
#endif

  if (argc != 2) {
    std::cerr << "Usage: sm5_recipe_screen_uv_insert <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> input_bytes;
  if (!ReadFile(args[1], input_bytes)) {
    std::cerr << "Failed to read input file: " << args[1] << "\n";
    return 1;
  }

  const std::filesystem::path recipe_path =
      RepoRootPath() / "tests/recipes/physically_based_standard_screen_uv.recipe.yml";
  auto parse_result = dxp::sm5::Recipe::ParseFromFile(recipe_path.string());
  if (!parse_result) {
    std::cerr << "Failed to parse SM5 recipe file: " << parse_result.error() << "\n";
    return 1;
  }

  const auto patch_result = parse_result.value().Execute(input_bytes);
  if (!patch_result) {
    std::cerr << "Failed to patch SM5 shader: " << patch_result.error() << "\n";
    return 1;
  }

  const auto& report = patch_result.value();

  // Verify recipe report structure
  if (report.output_container.format != "DXBC") {
    std::cerr << "Expected SM5 patch report to identify DXBC output format.\n";
    return 1;
  }

  if (report.steps.empty()) {
    std::cerr << "Expected SM5 patch report to record at least one step.\n";
    return 1;
  }

  if (report.output_container.total_size_in_bytes != report.output_bytes.size()) {
    std::cerr << "Expected SM5 patch report to expose final DXBC container size.\n";
    return 1;
  }

  constexpr size_t kHexHashLength = 32;
  if (report.output_container.hash_hex.size() != kHexHashLength) {
    std::cerr << "Expected SM5 patch report to expose a 32-character DXBC hash.\n";
    return 1;
  }

  if (report.output_container.chunks.empty()) {
    std::cerr << "Expected SM5 patch report to enumerate DXBC chunks.\n";
    return 1;
  }

  bool found_shader_chunk = false;
  for (const auto& chunk : report.output_container.chunks) {
    if (chunk.size_in_bytes == 0) {
      std::cerr << "Expected SM5 patch report chunk sizes to be populated.\n";
      return 1;
    }
    if (chunk.id == "SHDR" || chunk.id == "SHEX") {
      found_shader_chunk = true;
    }
  }
  if (!found_shader_chunk) {
    std::cerr << "Expected SM5 patch report to include the shader chunk.\n";
    return 1;
  }

  std::cout << "SM5 recipe screen UV insert test passed.\n";
  return 0;
}
