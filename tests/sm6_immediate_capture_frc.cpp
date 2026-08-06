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
  const auto recipe_path = RepoRootPath() / "tests/recipes/sm6_immediate_capture_frc.recipe.yml";

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

  // Verify that the export key exists in immediate_values map
  auto it = result->immediate_values.find("frc_constants");
  if (it == result->immediate_values.end()) {
    std::cerr << "Expected 'frc_constants' key in immediate_values map\n";
    return 1;
  }

  // The captured Frc operand in this shader is an i32 constant (LLVM signless
  // i32): the label must be I32 by width, and the raw value right-aligned.
  if (it->second.type != dxp::ComponentType::I32) {
    std::cerr << "Expected ComponentType::I32 for Frc constant, got " << static_cast<int>(it->second.type) << "\n";
    return 1;
  }
  if (it->second.raw_values.empty()) {
    std::cerr << "Expected non-empty raw_values for Frc constant\n";
    return 1;
  }
  if (it->second.raw_values[0] > 0xFFFFFFFFULL) {
    std::cerr << "Expected 32-bit float bits (fits in low dword), got 0x" << std::hex << it->second.raw_values[0] << std::dec << "\n";
    return 1;
  }

  std::cout << "SM6 immediate capture Frc test passed\n";
  std::cout << "  Match count: " << apply_res->match_count << "\n";
  std::cout << "  frc_constants exports: " << 1 << " (type=I32, raw=" << it->second.raw_values[0] << ")\n";
  return 0;
}
