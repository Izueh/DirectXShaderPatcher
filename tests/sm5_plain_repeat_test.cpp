/// @file sm5_plain_repeat_test.cpp
/// @brief Tests plain repeat (no template) on rule emits through the public
///        recipe API (no bytecode decoding): repeat with per-iteration params,
///        and YAML anchor/alias emit reuse.
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#include "dxp/sm5/Recipe.hpp"
#include "dxp/StepResults.hpp"
#include "tests/helper/TestHelper.hpp"

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 2) {
    std::cerr << "Usage: sm5_plain_repeat_test <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> input_bytes;
  if (!ReadFile(args[1], input_bytes)) {
    std::cerr << "Failed to read input file: " << args[1] << "\n";
    return 1;
  }

  // --- Test 1: Plain repeat with per-iteration params ---
  {
    const char* recipe_text = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: plain_repeat
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
              components:
                selection_mode: select
                value: x
              immediates_u32:
                - offset
          repeat:
            times: 3
            params:
              offset:
                u32: [17, 18, 19]
)YAML";

    auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, "inline-sm5-plain-repeat");
    if (!parse_result) {
      std::cerr << "Test 1a: Failed to parse recipe: " << parse_result.error() << "\n";
      return 1;
    }

    auto validate_result = dxp::sm5::ValidateRecipe(parse_result.value());
    if (!validate_result) {
      std::cerr << "Test 1a: Validation failed: " << validate_result.error() << "\n";
      return 1;
    }

    const auto patch_result = parse_result.value().Execute(input_bytes);
    if (!patch_result) {
      std::cerr << "Test 1a: Execute failed: " << patch_result.error() << "\n";
      return 1;
    }

    const auto& report = patch_result.value();
    for (const auto& step : report.steps) {
      if (step.name == "plain_repeat") {
        const auto* res = std::get_if<dxp::ApplyRuleResults>(&step.results);
        if (res == nullptr || res->applied_count != 1) {
          std::cerr << "Test 1a: apply_rule applied_count != 1.\n";
          return 1;
        }
      }
    }
    const std::string artifact = DefaultArtifactOutputPath(args[1], "_sm5_plain_repeat_test_1.cso");
    if (!WriteFile(artifact, report.output_bytes.data(), report.output_bytes.size())) {
      std::cerr << "Test 1a: Failed to write artifact " << artifact << ".\n";
      return 1;
    }
    std::cout << "Test 1: artifact written (" << artifact << ").\n";
  }

  // --- Test 2: YAML anchor/alias reuse ---
  {
    const char* recipe_text = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: anchor_alias
    rule:
      match:
        - opcode: mul
          operands:
            - capture: dst
            - capture: src
      emit:
        - &frag
          opcode: mov
          operands:
            - type: temp
              capture: dst
            - type: temp
              components:
                selection_mode: select
                value: x
              immediates_u32:
                - 15
        - *frag
)YAML";

    auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, "inline-sm5-anchor-alias");
    if (!parse_result) {
      std::cerr << "Test 2a: Failed to parse recipe: " << parse_result.error() << "\n";
      return 1;
    }

    auto validate_result = dxp::sm5::ValidateRecipe(parse_result.value());
    if (!validate_result) {
      std::cerr << "Test 2a: Validation failed: " << validate_result.error() << "\n";
      return 1;
    }

    const auto patch_result = parse_result.value().Execute(input_bytes);
    if (!patch_result) {
      std::cerr << "Test 2a: Execute failed: " << patch_result.error() << "\n";
      return 1;
    }

    const auto& report = patch_result.value();
    for (const auto& step : report.steps) {
      if (step.name == "anchor_alias") {
        const auto* res = std::get_if<dxp::ApplyRuleResults>(&step.results);
        if (res == nullptr || res->applied_count != 1) {
          std::cerr << "Test 2a: apply_rule applied_count != 1.\n";
          return 1;
        }
      }
    }
    const std::string artifact = DefaultArtifactOutputPath(args[1], "_sm5_plain_repeat_test_2.cso");
    if (!WriteFile(artifact, report.output_bytes.data(), report.output_bytes.size())) {
      std::cerr << "Test 2a: Failed to write artifact " << artifact << ".\n";
      return 1;
    }
    std::cout << "Test 2: artifact written (" << artifact << ").\n";
  }

  std::cout << "SM5 plain repeat test passed.\n";
  return 0;
}
