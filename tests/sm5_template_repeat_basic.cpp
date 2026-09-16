/// @file sm5_template_repeat_basic.cpp
/// @brief Tests template repeat expansion with per-iteration parameters
///        through the public recipe API (no bytecode decoding): parse,
///        validate, execute, and step-report assertions.
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
    std::cerr << "Usage: sm5_template_repeat_basic <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> input_bytes;
  if (!ReadFile(args[1], input_bytes)) {
    std::cerr << "Failed to read input file: " << args[1] << "\n";
    return 1;
  }

  // Recipe: declare a template whose second operand references the `offset`
  // variable; instantiate with repeat 3 and per-iteration param values. The
  // template declares five temps so the reuse pool block covers the
  // per-iteration source registers (r21, r22, r23).
  const char* recipe_text = R"YAML(version: 1
steps:
  - kind: declare_template
    name: tri_mov
    temps: [r0, r1, r2, r3, r4]
    emit:
      - opcode: mov
        operands:
          - type: temp
            handle:
              name: r0
            components:
              selection_mode: mask
              value: x
          - type: temp
            components:
              selection_mode: select
              value: x
            immediates_u32:
              - offset
  - kind: apply_rule
    name: emit_tri
    rule:
      match:
        - opcode: mul
          operands:
            - capture: dst
            - capture: src
      emit:
        - template: tri_mov
          repeat:
            times: 3
            params:
              offset:
                u32: [21, 22, 23]
)YAML";

  auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, "inline-sm5-template-repeat");
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

  // Verify template step results.
  bool found_template_result = false;
  bool found_rule_result = false;
  for (const auto& step : report.steps) {
    const auto* tmpl_res = std::get_if<dxp::DeclareTemplateResults>(&step.results);
    if (tmpl_res != nullptr) {
      found_template_result = true;
      if (tmpl_res->emit_count != 1) {
        std::cerr << "declare_template emit_count != 1.\n";
        return 1;
      }
    }
    if (step.name == "emit_tri") {
      const auto* rule_res = std::get_if<dxp::ApplyRuleResults>(&step.results);
      found_rule_result = rule_res != nullptr && rule_res->applied_count == 1;
    }
  }
  if (!found_template_result) {
    std::cerr << "Expected declare_template step in results.\n";
    return 1;
  }
  if (!found_rule_result) {
    std::cerr << "Expected apply_rule step with applied_count == 1.\n";
    return 1;
  }

  const std::string artifact = DefaultArtifactOutputPath(args[1], "_sm5_template_repeat_basic.cso");
  if (!WriteFile(artifact, report.output_bytes.data(), report.output_bytes.size())) {
    std::cerr << "Failed to write artifact " << artifact << ".\n";
    return 1;
  }
  std::cout << "artifact written (" << artifact << ").\n";

  std::cout << "SM5 template repeat basic test passed.\n";
  return 0;
}
