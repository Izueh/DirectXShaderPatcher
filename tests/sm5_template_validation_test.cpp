/// @file sm5_template_validation_test.cpp
/// @brief Tests template feature failure modes (no execution needed):
///        unknown template reference, nested template:, reserved `iteration`
///        param name, rule emit referencing a template temp by handle:,
///        template output contract (declared outputs provided by caller
///        temps or captures, collision + provision violations),
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

  // --- Test 3: Param named `iteration` (per-emit repeat on a template emit) ---
  const char* iteration_param = R"YAML(version: 1
steps:
  - kind: declare_template
    name: tmpl
    temps: [r0]
    emit:
      - opcode: mov
        repeat:
          times: 2
          params:
            iteration:
              u32: [0, 1]
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

  // --- Test 11: Output contract happy path — declared output provided by an
  // add_resource temp (owner register) ---
  const char* output_valid = R"YAML(version: 1
steps:
  - kind: declare_template
    name: tmpl
    temps: [r0]
    params: [acc]
    emit:
      - opcode: add
        operands:
          - type: temp
            handle:
              name: acc
            components:
              selection_mode: mask
              value: x
          - type: temp
            capture: dst
          - type: temp
            handle:
              name: r0
            components:
              selection_mode: mask
              value: x
  - kind: add_resource
    name: extra_res
    temps: [owner_acc]
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
          params:
            acc: owner_acc
)YAML";
  if (!expectValidationOk(output_valid, "inline-sm5-template-output-valid")) {
    std::cerr << "Test 11: valid template output recipe rejected.\n";
    return 1;
  }

  // --- Test 12: Declared output not provided by the instantiating entry ---
  const char* output_missing = R"YAML(version: 1
steps:
  - kind: declare_template
    name: tmpl
    temps: [r0]
    params: [acc]
    emit:
      - opcode: mov
        operands:
          - type: temp
            handle:
              name: acc
            components:
              selection_mode: mask
              value: x
          - type: temp
            capture: dst
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
  if (!expectValidationContains(output_missing, "inline-sm5-template-output-missing", "is not provided by the instantiating emit entry")) {
    std::cerr << "Test 12: missing output provision not rejected.\n";
    return 1;
  }

  // --- Test 13: Entry provides an output the template does not declare ---
  const char* output_undeclared = R"YAML(version: 1
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
          - type: temp
            capture: dst
  - kind: add_resource
    name: extra_res
    temps: [owner_acc]
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
          params:
            acc: owner_acc
)YAML";
  if (!expectValidationContains(output_undeclared, "inline-sm5-template-output-undeclared", "does not declare param")) {
    std::cerr << "Test 13: undeclared output provision not rejected.\n";
    return 1;
  }

  // --- Test 14: Output provided name is neither a known temp nor capture ---
  const char* output_unknown_provider = R"YAML(version: 1
steps:
  - kind: declare_template
    name: tmpl
    temps: [r0]
    params: [acc]
    emit:
      - opcode: mov
        operands:
          - type: temp
            handle:
              name: acc
            components:
              selection_mode: mask
              value: x
          - type: temp
            capture: dst
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
          params:
            acc: no_such_register
)YAML";
  if (!expectValidationContains(output_unknown_provider, "inline-sm5-template-output-unknown-provider", "is neither a known add_resource temp nor a known capture")) {
    std::cerr << "Test 14: unknown output provider not rejected.\n";
    return 1;
  }

  // --- Test 15: Output name collides with a template temp name ---
  const char* output_temp_collision = R"YAML(version: 1
steps:
  - kind: declare_template
    name: tmpl
    temps: [r0]
    params: [r0]
    emit:
      - opcode: mov
        operands:
          - type: temp
            handle:
              name: r0
            components:
              selection_mode: mask
              value: x
          - type: temp
            capture: dst
)YAML";
  if (!expectValidationContains(output_temp_collision, "inline-sm5-template-output-temp-collision", "collides with a template temp")) {
    std::cerr << "Test 15: output/temp name collision not rejected.\n";
    return 1;
  }

  // --- Test 16: Output name collides with an add_resource temp name ---
  const char* output_handle_collision = R"YAML(version: 1
steps:
  - kind: declare_template
    name: tmpl
    temps: [r0]
    params: [owner_acc]
    emit:
      - opcode: mov
        operands:
          - type: temp
            handle:
              name: r0
            components:
              selection_mode: mask
              value: x
          - type: temp
            capture: dst
  - kind: add_resource
    name: extra_res
    temps: [owner_acc]
)YAML";
  if (!expectValidationContains(output_handle_collision, "inline-sm5-template-output-handle-collision", "collides with a declared template param")) {
    std::cerr << "Test 16: output/handle name collision not rejected.\n";
    return 1;
  }

  // --- Test 17: outputs on a non-template emit entry ---
  const char* output_non_template_entry = R"YAML(version: 1
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
          params:
            acc: owner_acc
          operands:
            - type: temp
              capture: dst
)YAML";
  if (!expectValidationContains(output_non_template_entry, "inline-sm5-template-output-non-template-entry", "only valid on template: entries")) {
    std::cerr << "Test 17: outputs on non-template entry not rejected.\n";
    return 1;
  }

  std::cout << "SM5 template validation test passed.\n";
  return 0;
}
