/// @file sm5_template_test.cpp
/// @brief Tests template instantiation through the public recipe API: parse,
///        validate, execute, and step-report assertions (no bytecode decoding).
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#include "dxp/sm5/Recipe.hpp"
#include "dxp/StepResults.hpp"
#include "tests/helper/TestHelper.hpp"

namespace {

bool WriteArtifact(const std::string& input_path, const std::string& suffix,
                   const std::vector<uint8_t>& output_bytes, const std::string& tag) {
  const std::string artifact = DefaultArtifactOutputPath(input_path, "_" + suffix + ".cso");
  if (!WriteFile(artifact, output_bytes.data(), output_bytes.size())) {
    std::cerr << tag << ": failed to write artifact " << artifact << ".\n";
    return false;
  }
  std::cout << tag << ": artifact written (" << artifact << ").\n";
  return true;
}

}  // namespace

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 2) {
    std::cerr << "Usage: sm5_template_test <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> input_bytes;
  if (!ReadFile(args[1], input_bytes)) {
    std::cerr << "Failed to read input file: " << args[1] << "\n";
    return 1;
  }

  // --- Test 1: Single instantiation (no repeat) ---
  {
    const char* recipe_text = R"YAML(version: 1
steps:
  - kind: declare_template
    name: single_mov
    temps: [r0]
    emit:
      - opcode: mov
        operands:
          - type: temp
            handle:
              name: r0
            components:
              selection_mode: mask
              value: x
          - capture: src
  - kind: apply_rule
    name: emit_single
    rule:
      match:
        - opcode: mul
          operands:
            - capture: dst
            - capture: src
      emit:
        - template: single_mov
)YAML";

    auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, "inline-sm5-template-single");
    if (!parse_result) {
      std::cerr << "Test 1a: Failed to parse recipe: " << parse_result.error() << "\n";
      return 1;
    }

    auto validate_result = dxp::sm5::ValidateRecipe(parse_result.value());
    if (!validate_result) {
      std::cerr << "Test 1a: Validation failed: " << validate_result.error() << "\n";
      return 1;
    }

    const auto patch_result = parse_result.value().Execute(input_bytes);
    if (!patch_result) {
      std::cerr << "Test 1a: Execute failed: " << patch_result.error() << "\n";
      return 1;
    }

    const auto& report = patch_result.value();
    bool declared = false, rule_applied = false;
    for (const auto& step : report.steps) {
      if (step.name == "single_mov") {
        declared = true;
        const auto* tmpl = std::get_if<dxp::DeclareTemplateResults>(&step.results);
        if (tmpl == nullptr || tmpl->emit_count != 1) {
          std::cerr << "Test 1a: declare_template emit_count != 1.\n";
          return 1;
        }
      } else if (step.name == "emit_single") {
        rule_applied = true;
        const auto* rule = std::get_if<dxp::ApplyRuleResults>(&step.results);
        if (rule == nullptr || rule->applied_count != 1) {
          std::cerr << "Test 1a: apply_rule applied_count != 1.\n";
          return 1;
        }
      }
    }
    if (!declared || !rule_applied) {
      std::cerr << "Test 1a: expected declare_template and apply_rule steps.\n";
      return 1;
    }
    if (!WriteArtifact(args[1], "sm5_template_test_single", report.output_bytes, "Test 1")) {
      return 1;
    }
  }

  // --- Test 2: Accumulator pattern — template with two temps, repeat ---
  {
    const char* recipe_text = R"YAML(version: 1
steps:
  - kind: declare_template
    name: accum
    temps: [r0, r1]
    emit:
      - opcode: add
        operands:
          - type: temp
            handle:
              name: r0
            components:
              selection_mode: mask
              value: x
          - type: temp
            handle:
              name: r1
            components:
              selection_mode: select
              value: x
          - capture: src
  - kind: apply_rule
    name: emit_accum
    rule:
      match:
        - opcode: mul
          operands:
            - capture: dst
            - capture: src
      emit:
        - template: accum
          repeat:
            times: 2
            params:
              r0:
                u32: [0, 1]
              r1:
                u32: [5, 10]
)YAML";

    auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, "inline-sm5-template-accum");
    if (!parse_result) {
      std::cerr << "Test 2a: Failed to parse recipe: " << parse_result.error() << "\n";
      return 1;
    }

    auto validate_result = dxp::sm5::ValidateRecipe(parse_result.value());
    if (!validate_result) {
      std::cerr << "Test 2a: Validation failed: " << validate_result.error() << "\n";
      return 1;
    }

    const auto patch_result = parse_result.value().Execute(input_bytes);
    if (!patch_result) {
      std::cerr << "Test 2a: Execute failed: " << patch_result.error() << "\n";
      return 1;
    }

    const auto& report = patch_result.value();
    for (const auto& step : report.steps) {
      if (step.name == "emit_accum") {
        const auto* rule = std::get_if<dxp::ApplyRuleResults>(&step.results);
        if (rule == nullptr || rule->applied_count != 1) {
          std::cerr << "Test 2a: apply_rule applied_count != 1.\n";
          return 1;
        }
      }
    }
    if (!WriteArtifact(args[1], "sm5_template_test_accum", report.output_bytes, "Test 2")) {
      return 1;
    }
  }

  std::cout << "SM5 template test passed.\n";
  return 0;
}
