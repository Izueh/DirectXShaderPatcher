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
    std::cerr << "Usage: sm5_recipe_blob_scope <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> input_bytes;
  if (!ReadFile(args[1], input_bytes)) {
    std::cerr << "Failed to read input file: " << args[1] << "\n";
    return 1;
  }

  // Three-step flow:
  //   1. capture-only (emit_blob defaults to none) — shader untouched
  //   2. scope: mutate the stored blob (frc -> mov) — shader still untouched
  //   3. before_last_return: insert the mutated blob before the last ret
  // Final output must differ from the input, and the intermediate steps must
  // not have mutated the shader.
  const char* recipe = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: capture_window
    match_blob:
      match_start:
        opcode: dp2
      match_end:
        opcode: sample_l
      capture: scoped_block
    match_mode: first

  - kind: apply_rule
    name: mutate_scoped
    scope: scoped_block
    rule:
      match_mode: match_all
      rewrite_mode: replace
      match:
        - opcode: frc
          operands:
            - capture: scoped_dst
            - capture: scoped_src
      emit:
        - opcode: mov
          operands:
            - capture: scoped_dst
            - capture: scoped_src

  - kind: apply_rule
    name: inject_mutated
    rewrite_mode: before_last_return
    rule:
      emit:
        - blob: scoped_block
)YAML";

  auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe, "inline-sm5-blob-scope");
  if (!parse_result) {
    std::cerr << "Failed to parse inline SM5 scope recipe: " << parse_result.error() << "\n";
    return 1;
  }

  auto patch_result = parse_result.value().Execute(input_bytes);
  if (!patch_result) {
    std::cerr << "Failed to patch SM5 shader with scope recipe: " << patch_result.error() << "\n";
    return 1;
  }

  const auto& report = patch_result.value();

  if (report.output_bytes.empty()) {
    std::cerr << "scope flow: output unexpectedly empty.\n";
    return 1;
  }
  if (report.output_bytes == input_bytes) {
    std::cerr << "scope flow: expected the injected blob to change the output.\n";
    return 1;
  }

  // All three steps must be present and match/apply
  int steps_found = 0;
  for (const auto& step : report.steps) {
    const auto* res = std::get_if<dxp::ApplyRuleResults>(&step.results);
    if (res == nullptr) continue;
    if (step.name == "capture_window" && res->match_count > 0) ++steps_found;
    if (step.name == "mutate_scoped" && res->match_count > 0) ++steps_found;
    if (step.name == "inject_mutated" && res->applied_count > 0) ++steps_found;
  }
  if (steps_found != 3) {
    std::cerr << "scope flow: expected all three steps to match/apply (found " << steps_found << "/3).\n";
    return 1;
  }

  std::cout << "SM5 blob scope flow (capture -> scoped mutate -> before_last_return insert) passed.\n";
  return 0;
}
