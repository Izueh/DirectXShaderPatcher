/// @file sm5_temp_array_test.cpp
/// @brief Tests temp array declarations (`count:`) end-to-end through the
///        public recipe API (no bytecode decoding): a declared temp array
///        indexed per iteration via `element_index: iteration` (0-based).
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
    std::cerr << "Usage: sm5_temp_array_test <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> input_bytes;
  if (!ReadFile(args[1], input_bytes)) {
    std::cerr << "Failed to read input file: " << args[1] << "\n";
    return 1;
  }

  // Recipe: declare a 4-temp array binding (tap), then emit a mov per
  // iteration indexing the array with the 0-based `iteration` variable.
  const char* recipe_text = R"YAML(version: 1
steps:
  - kind: add_resource
    name: add_tap
    temps:
      - name: tap
        count: 4
  - kind: apply_rule
    name: use_tap
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
              handle:
                name: tap
                element_index: iteration
              components:
                selection_mode: select
                value: x
          repeat:
            times: 4
)YAML";

  auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, "inline-sm5-temp-array");
  if (!parse_result) {
    std::cerr << "Failed to parse inline SM5 recipe: " << parse_result.error() << "\n";
    return 1;
  }

  auto validate_result = dxp::sm5::ValidateRecipe(parse_result.value());
  if (!validate_result) {
    std::cerr << "Recipe validation failed: " << validate_result.error() << "\n";
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
    std::cerr << "Patched output is identical to input; expected template emissions.\n";
    return 1;
  }

  for (const auto& step : report.steps) {
    if (step.name == "add_tap") {
      const auto* res = std::get_if<dxp::AddResourceResults>(&step.results);
      if (res == nullptr || res->temps_added != 4) {
        std::cerr << "add_resource temps_added != 4.\n";
        return 1;
      }
    }
    if (step.name == "use_tap") {
      const auto* res = std::get_if<dxp::ApplyRuleResults>(&step.results);
      if (res == nullptr || res->applied_count != 1) {
        std::cerr << "apply_rule applied_count != 1.\n";
        return 1;
      }
    }
  }

  const std::string artifact = DefaultArtifactOutputPath(args[1], "_sm5_temp_array_test.cso");
  if (!WriteFile(artifact, report.output_bytes.data(), report.output_bytes.size())) {
    std::cerr << "Failed to write artifact " << artifact << ".\n";
    return 1;
  }
  std::cout << "artifact written (" << artifact << ").\n";

  std::cout << "SM5 temp array test passed.\n";
  return 0;
}
