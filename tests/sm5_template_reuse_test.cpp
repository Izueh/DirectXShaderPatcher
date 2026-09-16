/// @file sm5_template_reuse_test.cpp
/// @brief Tests the reuse pool model through the public recipe API (no
///        bytecode decoding): two templates sharing one pool block
///        (width = max temps), add_resource temps above the pool.
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
    std::cerr << "Usage: sm5_template_reuse_test <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> input_bytes;
  if (!ReadFile(args[1], input_bytes)) {
    std::cerr << "Failed to read input file: " << args[1] << "\n";
    return 1;
  }

  const char* recipe_text = R"YAML(version: 1
steps:
  - kind: declare_template
    name: tpl_a
    temps: [a0, a1]
    emit:
      - opcode: add
        operands:
          - type: temp
            handle:
              name: a0
            components:
              selection_mode: mask
              value: x
          - type: temp
            handle:
              name: a1
            components:
              selection_mode: select
              value: x
          - capture: src
  - kind: declare_template
    name: tpl_b
    temps: [b0]
    emit:
      - opcode: mov
        operands:
          - type: temp
            handle:
              name: b0
            components:
              selection_mode: mask
              value: x
          - capture: src
  - kind: add_resource
    name: add_extra
    temps: [extra]
  - kind: apply_rule
    name: use_templates
    rule:
      match:
        - opcode: mul
          operands:
            - capture: dst
            - capture: src
      emit:
        - template: tpl_a
        - template: tpl_b
        - opcode: mov
          operands:
            - type: temp
              capture: dst
            - type: temp
              handle:
                name: extra
              components:
                selection_mode: select
                value: x
)YAML";

  auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, "inline-sm5-template-reuse");
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

  int declare_count = 0;
  for (const auto& step : report.steps) {
    const auto* tmpl_res = std::get_if<dxp::DeclareTemplateResults>(&step.results);
    if (tmpl_res != nullptr) {
      ++declare_count;
      if (tmpl_res->emit_count != 1) {
        std::cerr << "declare_template emit_count != 1 for " << step.name << ".\n";
        return 1;
      }
    }
    if (step.name == "add_extra") {
      const auto* res = std::get_if<dxp::AddResourceResults>(&step.results);
      if (res == nullptr || res->temps_added != 1) {
        std::cerr << "add_resource temps_added != 1.\n";
        return 1;
      }
    }
    if (step.name == "use_templates") {
      const auto* res = std::get_if<dxp::ApplyRuleResults>(&step.results);
      if (res == nullptr || res->applied_count != 1) {
        std::cerr << "apply_rule applied_count != 1.\n";
        return 1;
      }
    }
  }
  if (declare_count != 2) {
    std::cerr << "Expected two declare_template steps.\n";
    return 1;
  }

  const std::string artifact = DefaultArtifactOutputPath(args[1], "_sm5_template_reuse_test.cso");
  if (!WriteFile(artifact, report.output_bytes.data(), report.output_bytes.size())) {
    std::cerr << "Failed to write artifact " << artifact << ".\n";
    return 1;
  }
  std::cout << "artifact written (" << artifact << ").\n";

  std::cout << "SM5 template reuse test passed.\n";
  return 0;
}
