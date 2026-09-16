/// @file sm5_template_pool_test.cpp
/// @brief Tests template pool sizing and validation.
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

#include "dxp/sm5/Recipe.hpp"
#include "dxp/StepResults.hpp"
#include "tests/helper/TestHelper.hpp"

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 2) {
    std::cerr << "Usage: sm5_template_pool_test <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> input_bytes;
  if (!ReadFile(args[1], input_bytes)) {
    std::cerr << "Failed to read input file: " << args[1] << "\n";
    return 1;
  }

  // --- Test 1: Template with many temps (valid, fits in pool) ---
  {
    std::string temps_decl;
    for (uint32_t i = 0; i < 10; ++i) {
      if (i > 0) temps_decl += ", ";
      temps_decl += "r" + std::to_string(i);
    }

    std::string emits;
    for (uint32_t i = 0; i < 10; ++i) {
      emits +=
          "\n      - opcode: mov\n        operands:\n"
          "          - type: temp\n            handle:\n              name: r"
          + std::to_string(i) +
          "\n            components:\n              selection_mode: mask\n              value: x\n"
          "          - capture: src";
    }

    std::string recipe_text =
        "version: 1\n"
        "steps:\n"
        "  - kind: declare_template\n"
        "    name: big_template\n"
        "    temps: ["
        + temps_decl +
        "]\n"
        "    emit:"
        + emits +
        "\n"
        "  - kind: apply_rule\n"
        "    name: use_big\n"
        "    rule:\n"
        "      match:\n"
        "        - opcode: mul\n"
        "          operands:\n"
        "            - capture: dst\n"
        "            - capture: src\n"
        "      emit:\n"
        "        - template: big_template";

    auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, "inline-sm5-template-pool-big");
    if (!parse_result) {
      std::cerr << "Test 1a: Failed to parse recipe: " << parse_result.error() << "\n";
      return 1;
    }

    const auto patch_result = parse_result.value().Execute(input_bytes);
    if (!patch_result) {
      std::cerr << "Test 1a: Execute failed: " << patch_result.error() << "\n";
      return 1;
    }

    if (patch_result.value().output_bytes.empty() || patch_result.value().output_bytes == input_bytes) {
      std::cerr << "Test 1a: Expected non-empty modified output.\n";
      return 1;
    }
  }

  // --- Test 2: Duplicate template name ---
  {
    const char* recipe_text = R"YAML(version: 1
steps:
  - kind: declare_template
    name: dup_name
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
          - capture: dst
  - kind: declare_template
    name: dup_name
    temps: [r1]
    emit:
      - opcode: mov
        operands:
          - type: temp
            handle:
              name: r1
            components:
              selection_mode: mask
              value: x
          - capture: dst
)YAML";

    auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, "inline-sm5-template-dup");
    if (!parse_result) {
      std::cerr << "Test 2a: Failed to parse recipe: " << parse_result.error() << "\n";
      return 1;
    }

    const auto patch_result = parse_result.value().Execute(input_bytes);
    if (patch_result) {
      std::cerr << "Test 2a: Expected failure for duplicate template name.\n";
      return 1;
    }

    if (patch_result.error().find("duplicate") == std::string::npos && patch_result.error().find("collides") == std::string::npos) {
      std::cerr << "Test 2a: Expected duplicate/collide error, got: " << patch_result.error() << "\n";
      return 1;
    }
  }

  // --- Test 3: Template name collides with step name ---
  {
    const char* recipe_text = R"YAML(version: 1
steps:
  - kind: declare_template
    name: collide_name
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
          - capture: dst
  - kind: apply_rule
    name: collide_name
    rule:
      match:
        - opcode: mul
          operands:
            - capture: dst
            - capture: src
      emit:
        - opcode: mov
          operands:
            - type: temp
              capture: dst
            - type: temp
              capture: src
)YAML";

    auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, "inline-sm5-template-col-step");
    if (!parse_result) {
      std::cerr << "Test 3a: Failed to parse recipe: " << parse_result.error() << "\n";
      return 1;
    }

    const auto patch_result = parse_result.value().Execute(input_bytes);
    if (patch_result) {
      std::cerr << "Test 3a: Expected failure for template/step name collision.\n";
      return 1;
    }

    if (patch_result.error().find("duplicate") == std::string::npos && patch_result.error().find("reused") == std::string::npos) {
      std::cerr << "Test 3a: Expected duplicate/reused error, got: " << patch_result.error() << "\n";
      return 1;
    }
  }

  std::cout << "SM5 template pool test passed.\n";
  return 0;
}
