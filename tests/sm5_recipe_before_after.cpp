#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <variant>
#include <vector>

#include "dxp/sm5/Recipe.hpp"
#include "dxp/StepResults.hpp"
#include "tests/helper/TestHelper.hpp"

#ifndef NDEBUG
#include "tests/helper/StackTraceHelper.hpp"
#endif

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
#ifndef NDEBUG
  InstallCrashHandler();
#endif

  if (argc != 2) {
    std::cerr << "Usage: sm5_recipe_before_after <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> input_bytes;
  if (!ReadFile(args[1], input_bytes)) {
    std::cerr << "Failed to read input file: " << args[1] << "\n";
    return 1;
  }

  const char* before_recipe_text = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: before_replacement
    rewrite_mode: before
    rule:
        insert_index: 0
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

  auto before_parse_result = dxp::sm5::Recipe::ParseFromText(before_recipe_text, "inline-sm5-before-test");
  if (!before_parse_result) {
    std::cerr << "Failed to parse inline SM5 before recipe: " << before_parse_result.error() << "\n";
    return 1;
  }

  const auto before_patch_result = before_parse_result.value().Execute(input_bytes);
  if (!before_patch_result) {
    std::cerr << "Failed to patch SM5 shader with before recipe: " << before_patch_result.error() << "\n";
    return 1;
  }

  const auto& before_report = before_patch_result.value();

  if (before_report.output_bytes.empty()) {
    std::cerr << "Patched output is unexpectedly empty.\n";
    return 1;
  }

  if (before_report.output_bytes == input_bytes) {
    std::cerr << "Patched output is identical to input; expected a mutation.\n";
    return 1;
  }

  // Verify the apply_rule step succeeded with matches
  bool found_step = false;
  for (const auto& step : before_report.steps) {
    const auto* apply_res = std::get_if<dxp::ApplyRuleResults>(&step.results);
    if ((apply_res != nullptr) && apply_res->match_count > 0) {
      found_step = true;
      break;
    }
  }
  if (!found_step) {
    std::cerr << "Expected recipe to match and apply rules.\n";
    return 1;
  }

  std::cout << "SM5 before-after recipe test passed.\n";
  return 0;
}
