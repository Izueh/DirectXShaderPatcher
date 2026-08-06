#include <iostream>
#include <string>

#include "dxp/sm5/Recipe.hpp"

namespace {

bool Contains(const std::string& text, const std::string& needle) {
  return text.find(needle) != std::string::npos;
}

}  // namespace

int main() {
  // Recipe with relative_operand on an index that does NOT use immediate32_plus_relative
  // or immediate64_plus_relative. This should be rejected at parse time.
  const char* recipe_text = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: relative_operand_validation_test
    rule:
        match:
          - opcode: mov
            operands:
            - type: temp
              capture: dst
        emit:
          - opcode: mov
            operands:
              - type: temp
                indices:
                  - representation: immediate32
                    immediate_lo: 0
                    relative_operand:
                      type: temp
)YAML";

  auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, "inline-sm5-relative-operand-validation-test");
  if (!parse_result) {
    std::cerr << "Expected parse to succeed (compile is conversion-only); got: " << parse_result.error() << "\n";
    return 1;
  }

  auto validate_result = dxp::sm5::ValidateRecipe(parse_result.value());
  if (validate_result) {
    std::cerr << "Expected validation to fail due to relative_operand without proper representation.\n";
    return 1;
  }

  // Verify the error message contains expected context.
  if (!Contains(validate_result.error(), "relative_operand") && !Contains(validate_result.error(), "immediate32_plus_relative") && !Contains(validate_result.error(), "immediate64_plus_relative")) {
    std::cerr << "Expected error to mention relative_operand or valid representations, got: "
              << validate_result.error() << "\n";
    return 1;
  }

  std::cout << "SM5 validation rejected relative_operand without proper representation as expected.\n";
  return 0;
}
