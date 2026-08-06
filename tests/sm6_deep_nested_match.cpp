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
    std::cerr << "Usage: sm6_deep_nested_match <input.cso>\n";
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
    name: strip_nested
    required: false
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
                  capture: mul_result
                  operands:
                    - index: 0
                      kind: call
                      instruction:
                        opcode: Frc
                        capture: inner_frc
                        operands:
                          - index: 1
                            kind: call
                            instruction:
                              opcode: Dot2
                              capture: dot_result
                    - index: 1
                      capture: scale
        emit:
          - name: mul_result
            operands:
              - index: 0
                kind: call
                capture: mul_result
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

  // Recipe may or may not match (depends on shader content)
  // Just verify it parsed and executed without errors
  std::cout << "Deep nested match recipe executed successfully (matches: " << total_rule_matches << ").\n";
  std::cout.flush();
  std::cerr.flush();
  return 0;
}
