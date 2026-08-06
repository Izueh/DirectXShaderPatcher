// Error-contract test for dxp::sm6::Recipe.
//
// Verifies that failures surface as specific messages through
// Recipe::Execute (no stderr side-channel): a valid shader executes, while a
// corrupt container yields a specific extraction error.

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#include "dxp/sm6/Recipe.hpp"
#include "tests/helper/TestHelper.hpp"

namespace {

const char* kRecipeText = R"YAML(
steps:
  - kind: apply_rule
    name: identity_pass
    required: false
    rewrite_mode: none
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

}  // namespace

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 2) {
    std::cerr << "Usage: sm6_recipe_error_contract_test <input.cs_6_6.cso>\n";
    return 1;
  }

  const ScopedCoInitialize coinit;

  std::vector<uint8_t> input_bytes;
  if (!ReadFile(args[1], input_bytes)) {
    std::cerr << "Failed to read input shader: " << args[1] << "\n";
    return 1;
  }

  auto parse_result = dxp::sm6::Recipe::ParseFromText(kRecipeText, "inline-sm6-error-contract-test");
  if (!parse_result) {
    std::cerr << "Failed to parse inline SM6 recipe: " << parse_result.error() << "\n";
    return 1;
  }
  const dxp::sm6::Recipe& recipe = parse_result.value();

  // Test 1 — a valid shader executes successfully.
  {
    auto result = recipe.Execute(input_bytes);
    if (!result) {
      std::cerr << "Test 1: valid shader execution failed: " << result.error() << "\n";
      return 1;
    }
  }

  // Test 2 — a corrupt container yields a specific error via Execute.
  {
    const std::vector<uint8_t> corrupt(16, 0xAB);  // not a DXIL container
    auto result = recipe.Execute(corrupt);
    if (result) {
      std::cerr << "Test 2: corrupt input unexpectedly succeeded\n";
      return 1;
    }
    const std::string& error = result.error();
    if (error.find("extract") == std::string::npos) {
      std::cerr << "Test 2: expected a specific 'extract' error, got: " << error << "\n";
      return 1;
    }
  }

  std::cout << "sm6_recipe_error_contract_test passed (specific errors via Execute).\n";
  std::cout.flush();
  return 0;
}
