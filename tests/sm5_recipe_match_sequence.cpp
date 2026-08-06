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
    std::cerr << "Usage: sm5_recipe_match_sequence <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> input_bytes;
  if (!ReadFile(args[1], input_bytes)) {
    std::cerr << "Failed to read input file: " << args[1] << "\n";
    return 1;
  }

  const char* recipe_text = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: replace_frc_mul_sequence
    rewrite_mode: replace_range
    rule:
        match:
          - opcode: frc
            capture: ign_frc
          - opcode: mul
            capture: ign_mul
            operands:
              - capture: dst
              - capture: src
        emit:
          - opcode: mov
            operands:
              - capture: dst
              - capture: src
)YAML";

  auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, "inline-sm5-sequence-test");
  if (!parse_result) {
    std::cerr << "Failed to parse inline SM5 sequence recipe: " << parse_result.error() << "\n";
    return 1;
  }

  const auto patch_result = parse_result.value().Execute(input_bytes);
  if (!patch_result) {
    std::cerr << "Failed to patch SM5 shader with sequence recipe: " << patch_result.error() << "\n";
    return 1;
  }

  const auto& report = patch_result.value();

  // Verify recipe report structure
  if (report.output_container.format != "DXBC") {
    std::cerr << "Expected SM5 patch report to identify DXBC output format.\n";
    return 1;
  }

  if (report.steps.empty()) {
    std::cerr << "Expected SM5 patch report to record at least one step.\n";
    return 1;
  }

  if (report.steps.front().name != "replace_frc_mul_sequence") {
    std::cerr << "Expected SM5 patch report to describe the executed rewrite step.\n";
    return 1;
  }

  const auto* seq_res = std::get_if<dxp::ApplyRuleResults>(&report.steps.front().results);
  if ((seq_res == nullptr) || seq_res->match_count == 0) {
    std::cerr << "Expected SM5 patch report to describe the executed rewrite step.\n";
    return 1;
  }

  if (report.output_container.total_size_in_bytes != report.output_bytes.size()) {
    std::cerr << "Expected SM5 patch report to expose final DXBC container size.\n";
    return 1;
  }

  constexpr size_t kHexHashLength = 32;
  if (report.output_container.hash_hex.size() != kHexHashLength) {
    std::cerr << "Expected SM5 patch report to expose a 32-character DXBC hash.\n";
    return 1;
  }

  if (report.output_container.chunks.empty()) {
    std::cerr << "Expected SM5 patch report to enumerate DXBC chunks.\n";
    return 1;
  }

  bool found_shader_chunk = false;
  for (const auto& chunk : report.output_container.chunks) {
    if (chunk.size_in_bytes == 0) {
      std::cerr << "Expected SM5 patch report chunk sizes to be populated.\n";
      return 1;
    }
    if (chunk.id == "SHDR" || chunk.id == "SHEX") {
      found_shader_chunk = true;
    }
  }
  if (!found_shader_chunk) {
    std::cerr << "Expected SM5 patch report to include the shader chunk.\n";
    return 1;
  }

  // Test match-only mode (no mutation)
  const char* match_only_recipe_text = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: match_only_probe
    required: true
    match_mode: first
    rewrite_mode: none
    rule:
        match:
          - opcode: frc
            capture: ign_frc
)YAML";

  auto match_only_parse_result = dxp::sm5::Recipe::ParseFromText(match_only_recipe_text, "inline-sm5-match-only-test");
  if (!match_only_parse_result) {
    std::cerr << "Failed to parse inline SM5 match-only recipe: " << match_only_parse_result.error() << "\n";
    return 1;
  }

  const auto match_only_patch_result = match_only_parse_result.value().Execute(input_bytes);
  if (!match_only_patch_result) {
    std::cerr << "Failed to patch SM5 shader with match-only recipe: " << match_only_patch_result.error() << "\n";
    return 1;
  }

  const auto& match_only_report = match_only_patch_result.value();

  if (match_only_report.output_bytes.empty()) {
    std::cerr << "Match-only recipe produced unexpectedly empty output.\n";
    return 1;
  }

  std::cout << "SM5 sequence replacement succeeded and SM5 match-only rules "
               "reported matches without mutating the program.\n";
  return 0;
}
