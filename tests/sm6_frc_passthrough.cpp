#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <variant>
#include <vector>

#include "dxp/sm6/Recipe.hpp"
#include "dxp/StepResults.hpp"
#include "tests/helper/TestHelper.hpp"

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 2) {
    std::cerr << "Usage: sm6_frc_passthrough <input.cso>\n";
    return 1;
  }

  const ScopedCoInitialize coinit;

  std::vector<uint8_t> input_shader;
  if (!ReadFile(args[1], input_shader)) {
    std::cerr << "Failed to read input shader: " << args[1] << "\n";
    return 1;
  }

  // Inline recipe YAML
  const char* yaml = R"(
steps:
  - kind: apply_rule
    name: strip_frc
    required: true
    rewrite_mode: replace
    rule:
        prune: true
        match:
          - opcode: Frc
            capture: outer_frc
            operands:
              - index: 1
                kind: call
                instruction:
                  opcode: fmul
                  capture: mul_input
        emit:
          - capture: mul_input
            replace_captured: outer_frc
    match_mode: match_all
)";

  // Parse recipe from inline YAML
  auto parse_result = dxp::sm6::Recipe::ParseFromText(yaml);
  if (!parse_result) {
    std::cerr << "Failed to parse recipe: " << parse_result.error() << "\n";
    return 1;
  }

  // Execute recipe
  auto result = parse_result.value().Execute(input_shader);
  if (!result) {
    std::cerr << "Recipe execution failed: " << result.error() << "\n";
    return 1;
  }

  // Count total rule matches from report
  unsigned total_rule_matches = 0;
  for (const auto& step : result.value().steps) {
    if (const auto* apply_res = std::get_if<dxp::ApplyRuleResults>(&step.results)) {
      total_rule_matches += apply_res->match_count;
    }
  }
  if (total_rule_matches == 0) {
    std::cerr << "Expected declarative rewrite recipe to apply at least one rule.\n";
    return 1;
  }

  // Verify the rewrite step report
  const dxp::StepReport* rewrite_step_report = nullptr;
  for (const auto& step_report : result.value().steps) {
    if (step_report.name == "strip_frc") {
      rewrite_step_report = &step_report;
      break;
    }
  }

  if (rewrite_step_report == nullptr) {
    std::cerr << "Expected declarative rewrite report to include the rewrite step entry.\n";
    return 1;
  }

  const auto* apply_res = std::get_if<dxp::ApplyRuleResults>(&rewrite_step_report->results);
  if ((apply_res == nullptr) || apply_res->match_count == 0 || apply_res->applied_count == 0) {
    std::cerr << "Expected declarative rewrite rule report to record an applied mutating match.\n";
    return 1;
  }

  // Verify serialization produced valid output
  if (result.value().output_bytes.empty()) {
    std::cerr << "Serialization produced empty output.\n";
    return 1;
  }

  std::cout << "Declarative rewrite executed successfully.\n";
  std::cout.flush();
  std::cerr.flush();
  return 0;
}
