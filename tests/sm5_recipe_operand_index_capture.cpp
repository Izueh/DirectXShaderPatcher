#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <variant>
#include <vector>

#include "dxp/sm5/Recipe.hpp"
#include "dxp/StepResults.hpp"
#include "tests/helper/TestHelper.hpp"

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 2) {
    std::cerr << "Usage: sm5_recipe_operand_index_capture <input.ps_5_0.cso>\n";
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
    name: mul_to_mov
    rule:
        match:
          - opcode: mul
            operands:
            - capture: dst
            - capture: src
        emit:
          - opcode: mov
            operands:
              - capture: dst
              - capture: src
)YAML";

  auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, "inline-sm5-operand-index-capture");
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

  bool found_step = false;
  for (const auto& step : report.steps) {
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

  std::cout << "SM5 operand index capture test passed.\n";
  return 0;
}
