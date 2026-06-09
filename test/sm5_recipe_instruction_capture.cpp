#include "TestSupport.h"
#include "dxp/sm5/Patch.h"
#include "dxp/sm5/RecipeParse.h"

#include <iostream>
#include <vector>

namespace {

static bool OperandsEqual(const dxp::sm5::ProgramOperand &lhs,
                          const dxp::sm5::ProgramOperand &rhs) {
  if (lhs.Type != rhs.Type || lhs.NumComponents != rhs.NumComponents ||
      lhs.ComponentMode != rhs.ComponentMode || lhs.Modifier != rhs.Modifier ||
      lhs.Indices != rhs.Indices ||
      lhs.ImmediateValues != rhs.ImmediateValues) {
    return false;
  }

  if (lhs.RelativeOperands.size() != rhs.RelativeOperands.size()) {
    return false;
  }

  if (!lhs.RelativeOperands.empty() &&
      !OperandsEqual(lhs.RelativeOperands.front(),
                     rhs.RelativeOperands.front())) {
    return false;
  }

  return true;
}

static int FindFirstMul(const dxp::sm5::ProgramInspection &program) {
  for (size_t index = 0; index < program.Instructions.size(); ++index) {
    const auto &instruction = program.Instructions[index];
    if (instruction.Opcode == D3D10_SB_OPCODE_MUL &&
        instruction.Operands.size() >= 2) {
      return static_cast<int>(index);
    }
  }
  return -1;
}





static bool TestSingleInstructionCapture() {
  std::cout << "Test: single-instruction capture + emit as raw copy\n";

  const char *recipeText = R"YAML(version: 1
steps:
  - name: replace_mul_with_mov
    rules:
      - name: capture_and_replace
        match:
          opcode: mul
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

  dxp::sm5::RecipeParseResult parseResult;
  if (!dxp::sm5::ParseRecipeText(recipeText, parseResult,
                                 "inline-sm5-single-instruction-capture-test")) {
    std::cerr << "Failed to parse inline SM5 recipe: " << parseResult.Error
              << "\n";
    return false;
  }


  std::cout << "  Single-instruction capture recipe parsed successfully.\n";
  return true;
}





static bool TestSequenceInstructionCapture() {
  std::cout << "Test: sequence instruction capture + emit as raw copy\n";

  const char *recipeText = R"YAML(version: 1
steps:
  - name: capture_sequence_emit
    rules:
      - name: capture_frc_mul
        match:
          sequence:
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

  dxp::sm5::RecipeParseResult parseResult;
  if (!dxp::sm5::ParseRecipeText(recipeText, parseResult,
                                 "inline-sm5-sequence-instruction-capture-test")) {
    std::cerr << "Failed to parse sequence instruction capture recipe: "
              << parseResult.Error << "\n";
    return false;
  }

  std::cout << "  Sequence instruction capture recipe parsed successfully.\n";
  return true;
}





static bool TestCrossStepInstructionCapture() {
  std::cout << "Test: cross-step instruction capture\n";

  const char *recipeText = R"YAML(version: 1
steps:
  - name: capture_step
    rules:
      - name: capture_rule
        match:
          opcode: mul
          capture: captured_mul
          rewrite_mode: none
          operands:
            - capture: dst
            - capture: src
  - name: emit_step
    rules:
      - name: emit_captured
        match:
          opcode: mul
          operands:
            - capture: dst
            - capture: src
        emit:
          - capture: captured_mul
)YAML";

  dxp::sm5::RecipeParseResult parseResult;
  if (!dxp::sm5::ParseRecipeText(recipeText, parseResult,
                                 "inline-sm5-cross-step-instruction-capture-test")) {
    std::cerr << "Failed to parse cross-step instruction capture recipe: "
              << parseResult.Error << "\n";
    return false;
  }

  std::cout << "  Cross-step instruction capture recipe parsed successfully.\n";
  return true;
}





static bool TestInstructionCaptureWithFields() {
  std::cout << "Test: instruction capture with capture_fields projection\n";

  const char *recipeText = R"YAML(version: 1
steps:
  - name: capture_step
    rules:
      - name: capture_rule
        match:
          opcode: mul
          capture: captured_mul
          rewrite_mode: none
          operands:
            - capture: dst
            - capture: src
  - name: emit_step
    rules:
      - name: emit_partial
        match:
          opcode: mul
          operands:
            - capture: dst
            - capture: src
        emit:
          - capture: captured_mul
            capture_fields:
              opcode: true
              operands: true
)YAML";

  dxp::sm5::RecipeParseResult parseResult;
  if (!dxp::sm5::ParseRecipeText(recipeText, parseResult,
                                 "inline-sm5-instruction-capture-fields-test")) {
    std::cerr << "Failed to parse instruction capture_fields recipe: "
              << parseResult.Error << "\n";
    return false;
  }

  std::cout << "  Instruction capture_fields recipe parsed successfully.\n";
  return true;
}





static bool TestInvalidInstructionCaptureReference() {
  std::cout << "Test: validation rejects invalid instruction capture reference\n";

  const char *recipeText = R"YAML(version: 1
steps:
  - name: bad_step
    rules:
      - name: bad_rule
        match:
          opcode: mul
          operands:
            - capture: dst
        emit:
          - opcode: mov
            operands:
              - capture: dst
          - capture: nonexistent_instruction
)YAML";

  dxp::sm5::RecipeParseResult parseResult;
  if (dxp::sm5::ParseRecipeText(recipeText, parseResult,
                                 "inline-sm5-invalid-instruction-capture-test")) {
    std::cerr << "Expected parsing to reject invalid instruction capture "
                 "reference.\n";
    return false;
  }

  std::cout << "  Invalid instruction capture reference correctly rejected: "
            << parseResult.Error << "\n";
  return true;
}





static bool TestCaptureFieldsWithoutCapture() {
  std::cout << "Test: validation rejects capture_fields without capture name\n";

  const char *recipeText = R"YAML(version: 1
steps:
  - name: bad_step
    rules:
      - name: bad_rule
        match:
          opcode: mul
          operands:
            - capture: dst
        emit:
          - opcode: mov
            operands:
              - capture: dst
            capture_fields:
              opcode: true
)YAML";

  dxp::sm5::RecipeParseResult parseResult;
  if (dxp::sm5::ParseRecipeText(recipeText, parseResult,
                                 "inline-sm5-capture-fields-without-capture-test")) {
    std::cerr << "Expected parsing to reject capture_fields without capture.\n";
    return false;
  }

  std::cout << "  capture_fields without capture correctly rejected: "
            << parseResult.Error << "\n";
  return true;
}





static bool TestOpcodeAndCaptureConflict() {
  std::cout << "Test: validation rejects opcode + capture on same emit\n";

  const char *recipeText = R"YAML(version: 1
steps:
  - name: bad_step
    rules:
      - name: bad_rule
        match:
          opcode: mul
          operands:
            - capture: dst
        emit:
          - opcode: mov
            capture: captured_mul
)YAML";

  dxp::sm5::RecipeParseResult parseResult;
  if (dxp::sm5::ParseRecipeText(recipeText, parseResult,
                                 "inline-sm5-opcode-and-capture-conflict-test")) {
    std::cerr << "Expected parsing to reject opcode + capture on same emit.\n";
    return false;
  }

  std::cout << "  opcode + capture conflict correctly rejected: "
            << parseResult.Error << "\n";
  return true;
}






static bool TestEndToEndSingleInstruction() {
  std::cout << "Test: end-to-end single-instruction capture + emit\n";

  const char *recipeText = R"YAML(version: 1
steps:
  - name: capture_mul
    rules:
      - name: capture_rule
        match:
          opcode: mul
          capture: captured_mul
          rewrite_mode: none
          operands:
            - capture: dst
            - capture: src
  - name: replace_with_captured
    rules:
      - name: replace_rule
        match:
          opcode: mul
          operands:
            - capture: dst
            - capture: src
        emit:
          - capture: captured_mul
)YAML";

  dxp::sm5::RecipeParseResult parseResult;
  if (!dxp::sm5::ParseRecipeText(recipeText, parseResult,
                                 "inline-sm5-e2e-single-instruction-capture-test")) {
    std::cerr << "Failed to parse end-to-end recipe: " << parseResult.Error
              << "\n";
    return false;
  }

  std::cout << "  End-to-end single-instruction capture recipe parsed successfully.\n";
  return true;
}

}

int main() {
  int failures = 0;

  if (!TestSingleInstructionCapture()) ++failures;
  if (!TestSequenceInstructionCapture()) ++failures;
  if (!TestCrossStepInstructionCapture()) ++failures;
  if (!TestInstructionCaptureWithFields()) ++failures;
  if (!TestInvalidInstructionCaptureReference()) ++failures;
  if (!TestCaptureFieldsWithoutCapture()) ++failures;
  if (!TestOpcodeAndCaptureConflict()) ++failures;
  if (!TestEndToEndSingleInstruction()) ++failures;

  if (failures > 0) {
    std::cerr << "FAILED: " << failures << " test(s) failed.\n";
    return 1;
  }

  std::cout << "All instruction capture tests passed.\n";
  return 0;
}
