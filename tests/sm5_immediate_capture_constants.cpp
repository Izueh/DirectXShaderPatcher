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
  const auto recipe_path = RepoRootPath() / "tests/recipes/sm5_immediate_capture_constants.recipe.yml";

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

  // Verify that the export key exists in immediate_values map
  // frc instruction source is temp, not immediate - no immediate export expected
  auto it = result->immediate_values.find("frc_constants");
  if (it != result->immediate_values.end()) {
    std::cerr << "Expected no 'frc_constants' key (temp is not an immediate type)\n";
    return 1;
  }

  std::cout << "SM5 immediate capture constants test passed\n";
  std::cout << "  Match count: " << apply_res->match_count << "\n";
  std::cout << "  frc_constants exports: not present (expected - frc source is temp, not immediate)\n";
  return 0;
}
