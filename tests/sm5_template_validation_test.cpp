/// @file sm5_template_validation_test.cpp
/// @brief Tests template feature failure modes (no execution needed):
///        unknown template reference, nested template:, reserved `iteration`
///        param name, rule emit referencing a template temp by handle:,
///        operand-count violations (rule + template emits), temp-handle
///        operands without components (rule + template emits), and a
///        template requiring a capture the invoking rule's match does not
///        capture.
#include <cstddef>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#include "dxp/sm5/Recipe.hpp"

namespace {

/// @brief Expect a validation error containing a substring.
bool expectValidationContains(const char* recipe_text, const char* source_name, const std::string& needle) {
  auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, source_name);
  if (!parse_result) {
    // Compile-time rejection (e.g. nested template:) is valid for this mode.
    const std::string err = parse_result.error();
    if (err.find(needle) != std::string::npos) {
      return true;
    }
    std::cerr << "Unexpected parse error: " << err << "\n";
    return false;
  }
  auto validate_result = dxp::sm5::ValidateRecipe(parse_result.value());
  if (validate_result) {
    std::cerr << "Expected validation failure, got success.\n";
    return false;
  }
  const std::string err = validate_result.error();
  if (err.find(needle) == std::string::npos) {
    std::cerr << "Validation error missing expected text '" << needle << "': " << err << "\n";
    return false;
  }
  return true;
}

/// @brief Expect a successful parse + validate.
bool expectValidationOk(const char* recipe_text, const char* source_name) {
  auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, source_name);
  if (!parse_result) {
    std::cerr << "Unexpected parse error: " << parse_result.error() << "\n";
    return false;
  }
  auto validate_result = dxp::sm5::ValidateRecipe(parse_result.value());
  if (!validate_result) {
    std::cerr << "Unexpected validation failure: " << validate_result.error() << "\n";
    return false;
  }
  return true;
}

}  // namespace

int main() {
  // --- Test 1: Unknown template reference in a rule emit ---
  const char* unknown_template = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: rule
    rule:
      match:
        - opcode: mul
          operands:
            - capture: dst
      emit:
        - template: no_such_template
)YAML";
  if (!expectValidationContains(unknown_template, "inline-sm5-unknown-template", "unknown template")) {
    std::cerr << "Test 1: unknown template reference not rejected.\n";
    return 1;
  }

  // --- Test 2: Nested template: in a template emit ---
  const char* nested_template = R"YAML(version: 1
steps:
  - kind: declare_template
    name: outer
    temps: [r0]
    emit:
      - template: inner
)YAML";
  if (!expectValidationContains(nested_template, "inline-sm5-nested-template", "nested template instantiation is not supported")) {
    std::cerr << "Test 2: nested template: not rejected.\n";
    return 1;
  }

  // --- Test 3: Param named `iteration` (template repeat) ---
  const char* iteration_param = R"YAML(version: 1
steps:
  - kind: declare_template
    name: tmpl
    temps: [r0]
    emit:
      - opcode: mov
        operands:
          - type: temp
            handle:
              name: r0
            components:
              selection_mode: mask
              value: x
          - capture: src
    repeat:
      times: 2
      params:
        iteration:
          u32: [0, 1]
  - kind: apply_rule
    name: rule
    rule:
      match:
        - opcode: mul
          operands:
            - capture: dst
            - capture: src
      emit:
        - template: tmpl
)YAML";
  if (!expectValidationContains(iteration_param, "inline-sm5-iteration-param", "reserved")) {
    std::cerr << "Test 3: reserved `iteration` param not rejected.\n";
    return 1;
  }

  // --- Test 4: Rule emit references a template temp by handle: ---
  const char* template_temp_ref = R"YAML(version: 1
steps:
  - kind: declare_template
    name: tmpl
    temps: [scoped_r]
    emit:
      - opcode: mov
        operands:
          - type: temp
            handle:
              name: scoped_r
            components:
              selection_mode: mask
              value: x
          - capture: dst
  - kind: apply_rule
    name: rule
    rule:
      match:
        - opcode: mul
          operands:
            - capture: dst
      emit:
        - opcode: mov
          operands:
            - type: temp
              capture: dst
            - type: temp
              handle:
                name: scoped_r
)YAML";
  if (!expectValidationContains(template_temp_ref, "inline-sm5-template-temp-ref", "cannot be referenced outside a template instantiation")) {
    std::cerr << "Test 4: template temp handle reference not rejected.\n";
    return 1;
  }

  // --- Test 5 (control): a valid template recipe still validates ---
  const char* valid_recipe = R"YAML(version: 1
steps:
  - kind: declare_template
    name: tmpl
    temps: [r0]
    emit:
      - opcode: mov
        operands:
          - type: temp
            handle:
              name: r0
            components:
              selection_mode: mask
              value: x
          - capture: src
  - kind: apply_rule
    name: rule
    rule:
      match:
        - opcode: mul
          operands:
            - capture: dst
            - capture: src
      emit:
        - template: tmpl
)YAML";
  if (!expectValidationOk(valid_recipe, "inline-sm5-template-valid")) {
    std::cerr << "Test 5: valid template recipe rejected.\n";
    return 1;
  }

  // --- Test 6: Template emit with wrong operand count (1-operand mov) ---
  const char* template_count = R"YAML(version: 1
steps:
  - kind: declare_template
    name: tmpl
    temps: [r0]
    emit:
      - opcode: mov
        operands:
          - type: temp
            handle:
              name: r0
            components:
              selection_mode: mask
              value: x
  - kind: apply_rule
    name: rule
    rule:
      match:
        - opcode: mul
          operands:
            - capture: dst
      emit:
        - template: tmpl
)YAML";
  if (!expectValidationContains(template_count, "inline-sm5-template-count", "expects 2 operands")) {
    std::cerr << "Test 6: template emit operand count not rejected.\n";
    return 1;
  }

  // --- Test 7: Template emit temp handle without components: ---
  const char* template_no_components = R"YAML(version: 1
steps:
  - kind: declare_template
    name: tmpl
    temps: [r0]
    emit:
      - opcode: mov
        operands:
          - type: temp
            handle:
              name: r0
          - capture: src
  - kind: apply_rule
    name: rule
    rule:
      match:
        - opcode: mul
          operands:
            - capture: dst
            - capture: src
      emit:
        - template: tmpl
)YAML";
  if (!expectValidationContains(template_no_components, "inline-sm5-template-no-components", "temp handle operand has no component selection")) {
    std::cerr << "Test 7: template emit temp handle without components not rejected.\n";
    return 1;
  }

  // --- Test 8: Rule emit temp handle without components: ---
  const char* rule_no_components = R"YAML(version: 1
steps:
  - kind: add_resource
    name: extra_res
    temps: [extra]
  - kind: apply_rule
    name: rule
    rule:
      match:
        - opcode: mul
          operands:
            - capture: dst
      emit:
        - opcode: mov
          operands:
            - type: temp
              capture: dst
            - type: temp
              handle:
                name: extra
)YAML";
  if (!expectValidationContains(rule_no_components, "inline-sm5-rule-no-components", "temp handle operand has no component selection")) {
    std::cerr << "Test 8: rule emit temp handle without components not rejected.\n";
    return 1;
  }

  // --- Test 9: Rule emit with wrong operand count (1-operand mov) ---
  const char* rule_count = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: rule
    rule:
      match:
        - opcode: mul
          operands:
            - capture: dst
      emit:
        - opcode: mov
          operands:
            - type: temp
              capture: dst
)YAML";
  if (!expectValidationContains(rule_count, "inline-sm5-rule-count", "expects 2 operands")) {
    std::cerr << "Test 9: rule emit operand count not rejected.\n";
    return 1;
  }

  // --- Test 10: Template requires a capture the rule's match does not capture ---
  const char* template_requires_capture = R"YAML(version: 1
steps:
  - kind: declare_template
    name: tmpl
    temps: [r0]
    emit:
      - opcode: mov
        operands:
          - type: temp
            handle:
              name: r0
            components:
              selection_mode: mask
              value: x
          - capture: src
  - kind: apply_rule
    name: rule
    rule:
      match:
        - opcode: mul
          operands:
            - capture: dst
      emit:
        - template: tmpl
)YAML";
  if (!expectValidationContains(template_requires_capture, "inline-sm5-template-requires-capture", "requires capture")) {
    std::cerr << "Test 10: template required capture not enforced.\n";
    return 1;
  }

  std::cout << "SM5 template validation test passed.\n";
  return 0;
}
