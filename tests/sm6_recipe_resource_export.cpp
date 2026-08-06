#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <variant>
#include <vector>

#include "dxp/ExportTypes.hpp"
#include "tests/helper/TestHelper.hpp"

#include "dxp/sm6/Recipe.hpp"
#include "dxp/StepResults.hpp"

int main() {
  const std::filesystem::path shader_path = RepoRootPath() / "tests/shaders/0x56C468C3.cs_6_6.cso";
  const std::filesystem::path recipe_path = RepoRootPath() / "tests/recipes/sm6_resource_export.recipe.yml";

  // Parse recipe
  auto parse_result = dxp::sm6::Recipe::ParseFromFile(recipe_path.string());
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

  // Verify that export_as key 'frc_exports' exists in immediate_values
  auto it = result->immediate_values.find("frc_exports");
  if (it == result->immediate_values.end()) {
    std::cerr << "Expected 'frc_exports' key in immediate_values\n";
    return 1;
  }

  auto& imm = it->second;
  // Verify raw_values is populated (value captured from Frc call operand)
  if (imm.raw_values.empty()) {
    std::cerr << "Expected non-empty raw_values for frc_exports\n";
    return 1;
  }
  // Type should be I32 for the captured operand (DXIL opcode = 22 for Frc)
  if (imm.type != dxp::ComponentType::I32) {
    std::cerr << "Expected ComponentType::I32, got " << static_cast<int>(imm.type) << "\n";
    return 1;
  }
  // The captured value is operand 0 of Frc call: i32 22 (DXIL opcode for Frc)
  constexpr uint64_t kExpectedImmediate = 22;
  if (imm.raw_values[0] != kExpectedImmediate) {
    std::cerr << "Expected raw_values[0] == 22 (Frc opcode), got " << imm.raw_values[0] << "\n";
    return 1;
  }

  std::cout << "SM6 export test passed\n";
  std::cout << "  Match count: " << apply_res->match_count << "\n";
  std::cout << "  Resource usage export keys: " << result->resource_usage.size() << "\n";
  std::cout << "  Immediate value export keys: " << result->immediate_values.size() << "\n";
  std::cout << "  frc_exports type: F32\n";
  std::cout << "  frc_exports component count: " << imm.raw_values.size() << "\n";
  return 0;
}
