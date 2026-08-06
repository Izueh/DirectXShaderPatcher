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
    std::cerr << "Usage: sm6_complex_emit <input.cso>\n";
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
  - kind: add_resource
    name: add_resources
    textures:
      - handle: fast_noise
        kind: Texture2DArray
        space: 50
    cbuffers:
      - handle: frame_constants
        space: 0
        size: 16
        type: ISFastFrameConstants
        fields:
          - name: FrameIndex
            type: U32
            width: 1
            offset: 0
  - kind: apply_rule
    name: ign_noise_rhs
    required: false
    rewrite_mode: none
    rule:
        prune: true
        match:
          - opcode: Frc
            capture: ign_root
            operands:
              - index: 1
                kind: call
                capture: outer_mul
                instruction:
                  opcode: fmul
                  capture: outer_mul
                  operands:
                    - index: 0
                      kind: call
                      capture: inner_frc
                      instruction:
                        opcode: Frc
                        capture: inner_frc
                        operands:
                          - index: 1
                            kind: call
                            instruction:
                              opcode: Dot2
                              capture: dot_call
                    - index: 1
                      capture: scale
        emit: []
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
    if (const auto* ar = std::get_if<dxp::ApplyRuleResults>(&step.results)) total_rule_matches += ar->match_count;
  }
  if (total_rule_matches == 0) {
    std::cerr << "Expected recipe to apply at least one rule.\n";
    return 1;
  }

  // Verify serialization produced valid output
  if (result.value().output_bytes.empty()) {
    std::cerr << "Serialization produced empty output.\n";
    return 1;
  }

  std::cout << "Complex emit recipe executed successfully (matches: " << total_rule_matches << ").\n";
  std::cout.flush();
  std::cerr.flush();
  return 0;
}
