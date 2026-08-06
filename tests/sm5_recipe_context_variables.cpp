#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <variant>
#include <vector>

#include "dxp/PatchOptions.hpp"
#include "dxp/sm5/Recipe.hpp"
#include "dxp/StepResults.hpp"
#include "tests/helper/TestHelper.hpp"

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 2) {
    std::cerr << "Usage: sm5_recipe_context_variables <input.ps_5_0.cso>\n";
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
    name: rewrite_with_runtime_vars
    condition:
      eq:
        lhs: enable_noise_patch
        rhs: true
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
              - type: immediate32
                immediates_u32: [frame_seed]
)YAML";

  auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, "inline-sm5-context-variables-test");
  if (!parse_result) {
    std::cerr << "Failed to parse inline SM5 recipe: " << parse_result.error() << "\n";
    return 1;
  }

  dxp::PatchOptions options;
  options.SetEnv("enable_noise_patch", true);
  constexpr uint32_t kFloatOneBits = 0x3F800000u;
  options.SetEnv("frame_seed", kFloatOneBits);
  parse_result.value().SetEnv(options);

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

  std::cout << "SM5 context variables resolved in immediates via recipe SetEnv.\n";
  return 0;
}
