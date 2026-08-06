#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#include "dxp/sm5/Recipe.hpp"
#include "tests/helper/TestHelper.hpp"

namespace {

}  // namespace

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 2) {
    std::cerr << "Usage: sm5_recipe_invalid_resolved_emit <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> input_bytes;
  if (!ReadFile(args[1], input_bytes)) {
    std::cerr << "Failed to read input file: " << args[1] << "\n";
    return 1;
  }

  const char* recipe_text = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: invalid_resolved_emit
    rule:
        match:
          - opcode: frc
            operands:
            - capture: dst
            - capture: src
        emit:
          - opcode: mov
            operands:
              - capture: dst
              - type: temp
                indices:
                  - representation: immediate32_plus_relative
                    immediate_lo: 0
)YAML";

  auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, "inline-sm5-invalid-resolved-emit-test");
  if (!parse_result) {
    std::cerr << "Expected parse to succeed (compile is conversion-only); got: " << parse_result.error() << "\n";
    return 1;
  }

  auto validate_result = dxp::sm5::ValidateRecipe(parse_result.value());
  if (validate_result) {
    std::cerr << "Expected validation to fail due to invalid index representation." << "\n";
    return 1;
  }

  if (!validate_result.error().contains("immediate32_plus_relative")) {
    std::cerr << "Expected validation error to contain immediate32_plus_relative, got: "
              << validate_result.error() << "\n";
    return 1;
  }

  std::cout << "SM5 recipe validator rejected invalid index representation as expected." << "\n";
  return 0;
}
