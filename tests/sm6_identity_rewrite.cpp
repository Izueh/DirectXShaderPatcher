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
    std::cerr << "Usage: sm6_identity_rewrite <input.cso>\n";
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
    name: identity_pass
    required: false
    rewrite_mode: none
    rule:
        prune: true
        match:
          - opcode: TextureLoad
            capture: texture_load
            operands:
              - index: 1
                capture: texture_handle
              - index: 3
                capture: coord_x
        emit: []
    match_mode: first
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
    if (const auto* ar = std::get_if<dxp::ApplyRuleResults>(&step.results)) total_rule_matches += ar->match_count;
  }

  // Recipe may or may not match (depends on shader content)
  // Just verify it parsed and executed without errors
  std::cout << "Identity rewrite recipe executed successfully (matches: " << total_rule_matches << ").\n";
  std::cout.flush();
  std::cerr.flush();
  return 0;
}
