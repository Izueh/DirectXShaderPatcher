#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <variant>
#include <vector>

#include "dxp/sm5/Recipe.hpp"
#include "dxp/StepResults.hpp"
#include "tests/helper/TestHelper.hpp"

// Verifies match_capture on a match operand: a probe step captures an operand
// into the cross-step global store, then a rewrite step's match requires an
// operand that EQUALS the captured one (same register + components + indices).
int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 2) {
    std::cerr << "Usage: sm5_recipe_match_captured_test <input.ps_5_0.cso>\n";
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
    name: probe_first_mov
    rewrite_mode: none
    required: false
    rule:
      match:
        - opcode: mov
          operands:
            - capture: probe_dst
  - kind: apply_rule
    name: rewrite_same_mov
    rewrite_mode: replace
    rule:
      match:
        - opcode: mov
          operands:
            - match_capture: probe_dst
      emit:
        - opcode: nop
)YAML";

  auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, "inline-sm5-match-captured");
  if (!parse_result) {
    std::cerr << "Failed to parse inline SM5 recipe: " << parse_result.error() << "\n";
    return 1;
  }

  const auto patch_result = parse_result.value().Execute(input_bytes);
  if (!patch_result) {
    std::cerr << "Failed to patch SM5 shader: " << patch_result.error() << "\n";
    return 1;
  }

  const auto& report = patch_result.value();
  if (report.output_bytes.empty()) {
    std::cerr << "Patched output is unexpectedly empty.\n";
    return 1;
  }
  if (report.output_bytes == input_bytes) {
    std::cerr << "Patched output is identical to input; expected a mutation.\n";
    return 1;
  }

  bool probe_matched = false;
  bool rewrite_matched = false;
  for (const auto& step : report.steps) {
    const auto* apply_res = std::get_if<dxp::ApplyRuleResults>(&step.results);
    if (apply_res == nullptr) continue;
    if (step.name == "probe_first_mov" && apply_res->match_count > 0) probe_matched = true;
    if (step.name == "rewrite_same_mov" && apply_res->match_count > 0) rewrite_matched = true;
  }
  if (!probe_matched || !rewrite_matched) {
    std::cerr << "Expected probe + match_captured rewrite to both match.\n";
    return 1;
  }

  std::cout << "SM5 match_captured cross-step test passed.\n";
  return 0;
}
