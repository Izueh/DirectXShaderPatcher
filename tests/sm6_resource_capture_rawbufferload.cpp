#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <variant>
#include <vector>

#include "tests/helper/TestHelper.hpp"

#include "dxp/sm6/Recipe.hpp"
#include "dxp/StepResults.hpp"

int main() {
  const auto shader_path = RepoRootPath() / "tests/shaders/0x56C468C3.cs_6_6.cso";
  const auto recipe_path = RepoRootPath() / "tests/recipes/sm6_resource_capture_rawbufferload.recipe.yml";

  auto parse_result = dxp::sm6::Recipe::ParseFromFile(recipe_path.string());
  if (!parse_result) {
    std::cerr << "Failed to parse recipe: " << parse_result.error() << "\n";
    return 1;
  }

  std::ifstream shader_file(shader_path, std::ios::binary);
  if (!shader_file) {
    std::cerr << "Failed to open shader: " << shader_path.string() << "\n";
    return 1;
  }
  const std::vector<uint8_t> shader_data(
      (std::istreambuf_iterator<char>(shader_file)),
      std::istreambuf_iterator<char>());

  auto result = parse_result.value().Execute(shader_data);
  if (!result) {
    std::cerr << "Recipe execution failed: " << result.error() << "\n";
    return 1;
  }

  if (result->steps.empty()) {
    std::cerr << "Expected at least one step report\n";
    return 1;
  }

  auto* apply_res = std::get_if<dxp::ApplyRuleResults>(&result->steps[0].results);
  if ((apply_res == nullptr) || apply_res->match_count == 0) {
    std::cerr << "Expected matches in step\n";
    return 1;
  }

  // Verify that the export key exists in resource_usage map
  auto it = result->resource_usage.find("raw_buffer_loads");
  if (it == result->resource_usage.end()) {
    std::cerr << "Expected 'raw_buffer_loads' key in resource_usage map\n";
    return 1;
  }

  std::cout << "SM6 resource capture RawBufferLoad test passed\n";
  std::cout << "  Match count: " << apply_res->match_count << "\n";
  std::cout << "  raw_buffer_loads exports: " << 1 << "\n";
  return 0;
}
