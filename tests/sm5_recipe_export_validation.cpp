#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "tests/helper/TestHelper.hpp"

#include "dxp/sm5/Recipe.hpp"

int main() {
  const std::filesystem::path recipe_path = RepoRootPath() / "tests/recipes/sm5_export_validation_invalid.yml";

  // Parse recipe - parsing succeeds, validation fails during execution
  auto parse_result = dxp::sm5::Recipe::ParseFromFile(recipe_path.string());
  if (!parse_result) {
    std::cerr << "Parse failed unexpectedly: " << parse_result.error() << "\n";
    return 1;
  }

  // Execute recipe - validation should fail due to invalid export_as key
  // Use a real shader for execution
  const std::filesystem::path shader_path = RepoRootPath() / "tests/shaders/0x7AFF256C.ps_5_0.cso";
  std::ifstream shader_file(shader_path, std::ios::binary);
  if (!shader_file) {
    std::cerr << "Failed to open shader: " << shader_path.string() << "\n";
    return 1;
  }
  const std::vector<uint8_t> shader_data(
      (std::istreambuf_iterator<char>(shader_file)),
      std::istreambuf_iterator<char>());
  auto exec_result = parse_result.value().Execute(shader_data);

  // Validation should fail
  if (!exec_result) {
    const std::string& error_msg = exec_result.error();
    if (error_msg.find("duplicate") != std::string::npos || error_msg.find("conflicts") != std::string::npos || error_msg.find("invalid export_as") != std::string::npos) {
      std::cout << "SM5 export validation test passed\n";
      std::cout << "  Error: " << error_msg << "\n";
      return 0;
    }
    std::cerr << "Expected error to mention conflict or invalid export_as, got: " << error_msg << "\n";
    return 1;
  }

  std::cerr << "Expected recipe execution to fail for conflicting export_as key\n";
  return 1;
}
