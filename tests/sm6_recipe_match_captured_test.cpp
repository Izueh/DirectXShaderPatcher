#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <variant>
#include <vector>

#include "dxp/sm6/Recipe.hpp"
#include "dxp/StepResults.hpp"
#include "tests/helper/TestHelper.hpp"

// Verifies match_capture on a match instruction pattern (sm6): a probe step
// captures an instruction into the cross-step global store, then a rewrite
// step's match requires the matched instruction to EQUAL the captured one.
int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 2) {
    std::cerr << "Usage: sm6_recipe_match_captured_test <input.cs_6_6.cso>\n";
    return 1;
  }

  std::vector<uint8_t> input_shader;
  if (!ReadFile(args[1], input_shader)) {
    std::cerr << "Failed to read input shader: " << args[1] << "\n";
    return 1;
  }

  const char* yaml = R"(
steps:
  - kind: apply_rule
    name: probe_first_frc
    rewrite_mode: none
    required: false
    rule:
      match:
        - opcode: Frc
          capture: any_frc
    match_mode: first
  - kind: apply_rule
    name: verify_same_frc
    rewrite_mode: none
    required: true
    rule:
      match:
        - opcode: Frc
          capture: matched_frc
          match_capture: any_frc
    match_mode: first
)";

  auto parse_result = dxp::sm6::Recipe::ParseFromText(yaml);
  if (!parse_result) {
    std::cerr << "Failed to parse recipe: " << parse_result.error() << "\n";
    return 1;
  }

  auto result = parse_result.value().Execute(input_shader);
  if (!result) {
    std::cerr << "Recipe execution failed: " << result.error() << "\n";
    return 1;
  }

  bool probe_matched = false;
  bool verify_matched = false;
  for (const auto& step : result.value().steps) {
    const auto* ar = std::get_if<dxp::ApplyRuleResults>(&step.results);
    if (ar == nullptr) continue;
    if (step.name == "probe_first_frc" && ar->match_count > 0) probe_matched = true;
    if (step.name == "verify_same_frc" && ar->match_count > 0) verify_matched = true;
  }
  if (!probe_matched || !verify_matched) {
    std::cerr << "Expected the probe and the match_captured verify step to both match.\n";
    return 1;
  }

  std::cout << "SM6 match_captured cross-step test passed.\n";
  return 0;
}
