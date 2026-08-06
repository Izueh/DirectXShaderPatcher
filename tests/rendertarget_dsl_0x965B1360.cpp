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
    std::cerr << "Usage: rendertarget_dsl_0x965B1360 <input.cso>\n";
    return 1;
  }

  const ScopedCoInitialize coinit;

  // Read shader bytes using public API (no ShaderProgram)
  std::vector<uint8_t> input_bytes;
  if (!ReadFile(args[1], input_bytes)) {
    std::cerr << "Failed to read input shader: " << args[1] << "\n";
    return 1;
  }

  // Recipe that matches StoreOutput calls and verifies we can capture them
  const char* yaml = R"(
steps:
  - kind: apply_rule
    name: match_red
    required: true
    rewrite_mode: none
    rule:
        match:
          - opcode: StoreOutput
            capture: red_store
            operands:
              - index: 0
              - index: 1
                kind: constant
                constant_int_values: [0]
              - index: 2
                kind: constant
                constant_int_values: [0]
              - index: 3
                kind: constant
                constant_int_values: [0]
              - index: 4
    match_mode: first
  - kind: apply_rule
    name: match_green
    required: true
    rewrite_mode: none
    rule:
        match:
          - opcode: StoreOutput
            capture: green_store
            operands:
              - index: 0
              - index: 1
                kind: constant
                constant_int_values: [0]
              - index: 2
                kind: constant
                constant_int_values: [0]
              - index: 3
                kind: constant
                constant_int_values: [1]
              - index: 4
    match_mode: first
  - kind: apply_rule
    name: match_blue
    required: true
    rewrite_mode: none
    rule:
        match:
          - opcode: StoreOutput
            capture: blue_store
            operands:
              - index: 0
              - index: 1
                kind: constant
                constant_int_values: [0]
              - index: 2
                kind: constant
                constant_int_values: [0]
              - index: 3
                kind: constant
                constant_int_values: [2]
              - index: 4
    match_mode: first
)";

  auto parse_result = dxp::sm6::Recipe::ParseFromText(yaml);
  if (!parse_result) {
    std::cerr << "Failed to parse recipe: " << parse_result.error() << "\n";
    return 1;
  }

  auto result = parse_result.value().Execute(input_bytes);
  if (!result) {
    std::cerr << "Recipe execution failed: " << result.error() << "\n";
    return 1;
  }

  // Verify all three steps matched
  unsigned total_matches = 0;
  for (const auto& step : result.value().steps) {
    if (const auto* apply_res = std::get_if<dxp::ApplyRuleResults>(&step.results)) {
      if (apply_res->match_count == 0) {
        std::cerr << "Expected step '" << step.name << "' to match at least one instruction.\n";
        return 1;
      }
      total_matches += apply_res->match_count;
    }
  }

  if (total_matches != 3) {
    std::cerr << "Expected exactly 3 matches (one per channel), got " << total_matches << ".\n";
    return 1;
  }

  std::cout << "Matched render target stores for SV_Target0 RGB via recipe.\n";
  return 0;
}
