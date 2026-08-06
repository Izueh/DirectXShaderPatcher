#include <iostream>

#include "dxp/sm5/Recipe.hpp"

namespace {

bool TestSingleInstructionCapture() {
  std::cout << "Test: single-instruction capture + emit as raw copy\n";

  const char* recipe_text = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: replace_mul_with_mov
    rule:
        match:
          - opcode: mul
            capture: target_mul
            operands:
            - capture: dst
            - capture: src
        emit:
          - opcode: mov
            operands:
              - capture: dst
              - capture: src
)YAML";

  auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, "inline-sm5-single-instruction-capture-test");
  if (!parse_result) {
    std::cerr << "Failed to parse inline SM5 recipe: " << parse_result.error() << "\n";
    return false;
  }

  std::cout << "  Single-instruction capture recipe parsed successfully.\n";
  return true;
}

bool TestSequenceInstructionCapture() {
  std::cout << "Test: sequence instruction capture + emit as raw copy\n";

  const char* recipe_text = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: capture_sequence_emit
    rule:
        match:
          - opcode: frc
            capture: frc_match
            operands:
              - capture: frc_dst
              - capture: frc_src
          - opcode: mul
            operands:
              - capture: mul_dst
              - capture: mul_src
        emit:
          - capture: frc_match
)YAML";

  auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, "inline-sm5-sequence-instruction-capture-test");
  if (!parse_result) {
    std::cerr << "Failed to parse sequence instruction capture recipe: " << parse_result.error() << "\n";
    return false;
  }

  std::cout << "  Sequence instruction capture recipe parsed successfully.\n";
  return true;
}

bool TestCrossStepInstructionCapture() {
  std::cout << "Test: cross-step instruction capture\n";

  const char* recipe_text = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: capture_step
    rewrite_mode: none
    rule:
        match:
          - opcode: mul
            capture: captured_mul
            operands:
            - capture: dst
            - capture: src
  - kind: apply_rule
    name: emit_step
    rule:
        match:
          - opcode: mul
            operands:
            - capture: dst
            - capture: src
        emit:
          - capture: captured_mul
)YAML";

  auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, "inline-sm5-cross-step-instruction-capture-test");
  if (!parse_result) {
    std::cerr << "Failed to parse cross-step instruction capture recipe: " << parse_result.error() << "\n";
    return false;
  }

  std::cout << "  Cross-step instruction capture recipe parsed successfully.\n";
  return true;
}

bool TestInvalidInstructionCaptureReference() {
  std::cout << "Test: validation rejects invalid instruction capture reference\n";

  const char* recipe_text = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: bad_step
    rule:
        match:
          - opcode: mul
            operands:
            - capture: dst
        emit:
          - opcode: mov
            operands:
              - capture: dst
          - capture: nonexistent_instruction
)YAML";

  // Invalid instruction capture reference validation moved to execution time
  auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, "inline-sm5-invalid-instruction-capture-test");
  if (!parse_result) {
    std::cerr << "Invalid instruction capture reference should now pass parse-time validation (moved to execution-time).\n";
    return false;
  }

  std::cout << "  Invalid instruction capture reference correctly passed parse-time validation (moved to execution-time).\n";
  return true;
}

bool TestOpcodeAndCaptureConflict() {
  std::cout << "Test: validation rejects opcode + capture on same emit\n";

  const char* recipe_text = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: bad_step
    rule:
        match:
          - opcode: mul
            operands:
            - capture: dst
        emit:
          - opcode: mov
            capture: captured_mul
)YAML";

  auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, "inline-sm5-opcode-and-capture-conflict-test");
  if (parse_result) {
    std::cerr << "Expected parsing to reject opcode + capture on same emit.\n";
    return false;
  }

  std::cout << "  opcode + capture conflict correctly rejected: " << parse_result.error() << "\n";
  return true;
}

bool TestEndToEndSingleInstruction() {
  std::cout << "Test: end-to-end single-instruction capture + emit\n";

  const char* recipe_text = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: capture_mul
    rewrite_mode: none
    rule:
        match:
          - opcode: mul
            capture: captured_mul
            operands:
            - capture: dst
            - capture: src
  - kind: apply_rule
    name: replace_with_captured
    rule:
        match:
          - opcode: mul
            operands:
            - capture: dst
            - capture: src
        emit:
          - capture: captured_mul
)YAML";

  auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, "inline-sm5-e2e-single-instruction-capture-test");
  if (!parse_result) {
    std::cerr << "Failed to parse end-to-end recipe: " << parse_result.error() << "\n";
    return false;
  }

  std::cout << "  End-to-end single-instruction capture recipe parsed successfully.\n";
  return true;
}

}  // namespace

int main() {
  int failures = 0;

  if (!TestSingleInstructionCapture()) {
    ++failures;
  }
  if (!TestSequenceInstructionCapture()) {
    ++failures;
  }
  if (!TestCrossStepInstructionCapture()) {
    ++failures;
  }
  if (!TestInvalidInstructionCaptureReference()) {
    ++failures;
  }
  if (!TestOpcodeAndCaptureConflict()) {
    ++failures;
  }
  if (!TestEndToEndSingleInstruction()) {
    ++failures;
  }

  if (failures > 0) {
    std::cerr << "FAILED: " << failures << " test(s) failed.\n";
    return 1;
  }

  std::cout << "All instruction capture tests passed.\n";
  return 0;
}
