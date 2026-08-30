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
    std::cerr << "Usage: sm6_blue_noise_emit <input.cso>\n";
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
    name: blue_noise_scalar_slice
    rewrite_mode: replace
    rule:
        prune: true
        match:
          - opcode: TextureLoad
            capture: texture_load
            operands:
              - index: 1
                kind: resource
                resource_class: SRV
                resource_kind: Texture2D
                register_index: 7
                space: 0
              - index: 3
                kind: call
                capture: coord_x
              - index: 4
                kind: call
                instruction:
                  opcode: add
                  operands:
                    - index: 0
                      kind: call
                      instruction:
                        opcode: mul
                        operands:
                          - index: 0
                            kind: call
                            instruction:
                              opcode: and
                          - index: 1
                    - index: 1
                      kind: call
                      capture: coord_y
        emit:
          - opcode: CBufferLoadLegacy
            result_component_type: I32
            name: frame_load
            operands:
              - index: 1
                kind: resource
                handle: frame_constants
              - index: 2
                kind: constant
                constant_int_values: [0]
          - aggregate: frame_load
            extract_index: 0
            name: frame_index
          - opcode: urem
            result_component_type: I32
            name: slice_index
            operands:
              - index: 0
                kind: call
                capture: frame_index
              - index: 1
                kind: constant
                constant_int_values: [32]
          - opcode: TextureLoad
            result_component_type: F32
            name: noise_load
            replace_captured: texture_load
            operands:
              - index: 1
                kind: resource
                handle: fast_noise
              - index: 2
                kind: constant
                constant_int_values: [0]
              - index: 3
                kind: call
                capture: coord_x
              - index: 4
                kind: call
                capture: coord_y
              - index: 5
                kind: call
                capture: slice_index
              - index: 6
              - index: 7
              - index: 8
    match_mode: match_all
    required: false
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
  std::cout << "Blue noise emit recipe executed successfully (matches: " << total_rule_matches << ").\n";
  std::cout.flush();
  std::cerr.flush();
  return 0;
}
