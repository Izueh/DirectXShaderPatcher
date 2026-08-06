#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <variant>
#include <vector>

#include "tests/helper/TestHelper.hpp"

#include "dxp/sm5/Recipe.hpp"
#include "dxp/StepResults.hpp"

int main() {
  const auto shader_path = RepoRootPath() / "tests/shaders/0x7AFF256C.ps_5_0.cso";
  const auto recipe_path = RepoRootPath() / "tests/recipes/sm5_resource_capture_sample.recipe.yml";

  auto parse_result = dxp::sm5::Recipe::ParseFromFile(recipe_path.string());
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
  auto it = result->resource_usage.find("sampled_textures");
  if (it == result->resource_usage.end()) {
    std::cerr << "Expected 'sampled_textures' key in resource_usage map\n";
    return 1;
  }

  auto& usage = it->second;
  if (usage.handle != "texture") {
    std::cerr << "Expected handle='texture', got '" << usage.handle << "'\n";
    return 1;
  }
  // Verify accessed_components is a valid 4-bit mask (non-zero means components were accessed)
  if (usage.accessed_components.none()) {
    std::cerr << "Expected non-zero accessed_components for sampled texture\n";
    return 1;
  }
  if (usage.accessed_components.size() != 4) {
    std::cerr << "Expected bitset<4> for accessed_components\n";
    return 1;
  }
  // Verify register_index is a valid texture register (0-9 in this shader)
  constexpr uint32_t kMaxExpectedRegisterIndex = 9;
  if (usage.register_index > kMaxExpectedRegisterIndex) {
    std::cerr << "Expected register_index 0-9, got " << usage.register_index << "\n";
    return 1;
  }

  std::cout << "SM5 resource capture sample test passed\n";
  std::cout << "  Match count: " << apply_res->match_count << "\n";
  std::cout << "  sampled_textures handle: " << usage.handle << "\n";
  std::cout << "  sampled_textures register_index: " << usage.register_index << "\n";
  std::cout << "  sampled_textures accessed_components: " << usage.accessed_components.to_ulong() << "\n";
  return 0;
}
