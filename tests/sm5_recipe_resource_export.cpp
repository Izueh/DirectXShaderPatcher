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
  const std::filesystem::path shader_path = RepoRootPath() / "tests/shaders/0x7AFF256C.ps_5_0.cso";
  const std::filesystem::path recipe_path = RepoRootPath() / "tests/recipes/sm5_resource_export.recipe.yml";

  // Parse recipe
  auto parse_result = dxp::sm5::Recipe::ParseFromFile(recipe_path.string());
  if (!parse_result) {
    std::cerr << "Failed to parse recipe: " << parse_result.error() << "\n";
    return 1;
  }

  // Read shader
  std::ifstream shader_file(shader_path, std::ios::binary);
  if (!shader_file) {
    std::cerr << "Failed to open shader: " << shader_path.string() << "\n";
    return 1;
  }
  const std::vector<uint8_t> shader_data(
      (std::istreambuf_iterator<char>(shader_file)),
      std::istreambuf_iterator<char>());

  // Execute recipe
  auto result = parse_result.value().Execute(shader_data);
  if (!result) {
    std::cerr << "Recipe execution failed: " << result.error() << "\n";
    return 1;
  }

  // Verify step report has match_count
  if (result->steps.empty()) {
    std::cerr << "Expected at least one step report\n";
    return 1;
  }

  auto* apply_res = std::get_if<dxp::ApplyRuleResults>(&result->steps[0].results);
  if (apply_res == nullptr) {
    std::cerr << "Expected ApplyRuleResults in step\n";
    return 1;
  }

  if (apply_res->match_count == 0) {
    std::cerr << "Expected matches in step\n";
    return 1;
  }

  // Verify top-level export maps exist (may be empty if no matching operand types)
  // The captured operand is a temp, not a resource or immediate, so exports should be empty
  if (!result->resource_usage.empty()) {
    std::cerr << "Expected empty resource_usage for temp operand export\n";
    return 1;
  }
  if (!result->immediate_values.empty()) {
    std::cerr << "Expected empty immediate_values for temp operand export\n";
    return 1;
  }

  std::cout << "SM5 export test passed\n";
  std::cout << "  Match count: " << apply_res->match_count << "\n";
  std::cout << "  Resource usage export keys: " << result->resource_usage.size() << "\n";
  std::cout << "  Immediate value export keys: " << result->immediate_values.size() << "\n";
  return 0;
}
