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
    std::cerr << "Usage: sm5_recipe_before_last_return <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> input_bytes;
  if (!ReadFile(args[1], input_bytes)) {
    std::cerr << "Failed to read input file: " << args[1] << "\n";
    return 1;
  }

  // ───────────────────────────────────────────────────────────────────────────
  // Test 1: before_last_return without a guard match — emit inserted before
  // the program's last ret. Output must differ from input.
  // ───────────────────────────────────────────────────────────────────────────
  {
    const char* recipe = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: inject_before_ret
    rewrite_mode: before_last_return
    rule:
      emit:
        - opcode: mov
          operands:
            - type: temp
              components:
                selection_mode: mask
                value: xyzw
              indices:
                - representation: immediate32
                  immediate_lo: 19
            - type: immediate32
              immediates_f32: [1.0]
)YAML";

    auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe, "inline-sm5-blr");
    if (!parse_result) {
      std::cerr << "Failed to parse inline SM5 before_last_return recipe: " << parse_result.error() << "\n";
      return 1;
    }

    auto patch_result = parse_result.value().Execute(input_bytes);
    if (!patch_result) {
      std::cerr << "Failed to patch with before_last_return recipe: " << patch_result.error() << "\n";
      return 1;
    }

    const auto& report = patch_result.value();
    if (report.output_bytes == input_bytes) {
      std::cerr << "before_last_return: expected insertion to change the output.\n";
      return 1;
    }

    bool applied = false;
    for (const auto& step : report.steps) {
      const auto* res = std::get_if<dxp::ApplyRuleResults>(&step.results);
      if (step.name == "inject_before_ret" && res != nullptr && res->applied_count > 0) {
        applied = true;
      }
    }
    if (!applied) {
      std::cerr << "before_last_return: expected the step to record an applied rewrite.\n";
      return 1;
    }
    std::cout << "before_last_return insertion OK\n";
  }

  // ───────────────────────────────────────────────────────────────────────────
  // Test 2: before_last_return with a guard match that does NOT match — the
  // step publishes state=false; with required: false the recipe continues and
  // the output stays byte-identical.
  // ───────────────────────────────────────────────────────────────────────────
  {
    const char* recipe = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: guarded_no_match
    required: false
    rewrite_mode: before_last_return
    rule:
      match:
        - opcode: bfi
      emit:
        - opcode: mov
          operands:
            - type: temp
              components:
                selection_mode: mask
                value: xyzw
              indices:
                - representation: immediate32
                  immediate_lo: 19
            - type: immediate32
              immediates_f32: [1.0]
)YAML";

    auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe, "inline-sm5-blr-guard");
    if (!parse_result) {
      std::cerr << "Failed to parse guarded before_last_return recipe: " << parse_result.error() << "\n";
      return 1;
    }

    auto patch_result = parse_result.value().Execute(input_bytes);
    if (!patch_result) {
      std::cerr << "Guarded before_last_return recipe failed: " << patch_result.error() << "\n";
      return 1;
    }

    const auto& report = patch_result.value();
    if (report.output_bytes != input_bytes) {
      std::cerr << "Guarded no-match before_last_return must leave the shader untouched.\n";
      return 1;
    }
    std::cout << "before_last_return guard no-match leaves shader untouched OK\n";
  }

  std::cout << "SM5 before_last_return tests passed.\n";
  return 0;
}
