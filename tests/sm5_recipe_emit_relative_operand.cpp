#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <system_error>
#include <variant>
#include <vector>

#include "dxp/sm5/Recipe.hpp"
#include "dxp/StepResults.hpp"
#include "tests/helper/TestHelper.hpp"

namespace {

bool WriteFile(const std::string& path, const std::vector<uint8_t>& bytes) {
  const std::filesystem::path output_path(path);
  const std::filesystem::path parent_path = output_path.parent_path();
  if (!parent_path.empty()) {
    std::error_code error;
    if (!std::filesystem::create_directories(parent_path, error) && error) {
      return false;
    }
  }

  std::ofstream out(path, std::ios::binary);
  if (!out) {
    return false;
  }

  if (!bytes.empty()) {
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  }

  return static_cast<bool>(out);
}

std::string DefaultOutPath(const std::string& in_path) {
  return DefaultArtifactOutputPath(in_path, ".sm5.emit_relative.patched.cso");
}

}  // namespace

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 2 && argc != 3) {
    std::cerr << "Usage: sm5_recipe_emit_relative_operand <input.cs_5_0.cso> [output.cso]\n";
    return 1;
  }

  const std::string input_path = args[1];
  const std::string output_path = argc == 3 ? args[2] : DefaultOutPath(input_path);

  std::vector<uint8_t> input_bytes;
  if (!ReadFile(input_path, input_bytes)) {
    std::cerr << "Failed to read input file: " << input_path << "\n";
    return 1;
  }

  // Recipe that matches any mov instruction and emits a new mov with a dynamically
  // indexed constant_buffer operand using relative + relative_operand.
  // Constant buffers are 2D: register index (cb0) + offset index (r0.x).
  // The compiler optimized cb0[r0.x + 0] to cb0[r0.x] (representation=Relative, not Immediate32PlusRelative).
  const char* recipe_text = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: emit_relative_operand_test
    rule:
        match:
          - opcode: mov
            operands:
            - type: temp
              capture: dst
        emit:
          - opcode: mov
            operands:
              - type: temp
                capture: dst
              - type: constant_buffer
                components:
                  selection_mode: swizzle
                  value: xyzw
                  num_components: four
                indices:
                  - representation: immediate32
                    immediate_lo: 0
                  - representation: relative
                    relative_operand:
                      type: temp
                      components:
                        selection_mode: select
                        value: x
                        num_components: four
                      indices:
                        - representation: immediate32
                          immediate_lo: 0
)YAML";

  auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, "inline-sm5-emit-relative-operand-test");
  if (!parse_result) {
    std::cerr << "Failed to parse inline emit relative operand recipe: " << parse_result.error() << "\n";
    return 1;
  }

  const auto patch_result = parse_result.value().Execute(input_bytes);
  if (!patch_result) {
    std::cerr << "PatchContainer failed: " << patch_result.error() << "\n";
    return 1;
  }

  if (patch_result.value().output_bytes.empty()) {
    std::cerr << "Patched SM5 output was unexpectedly empty.\n";
    return 1;
  }

  // Verify at least one rule was applied.
  bool any_applied = false;
  for (const auto& step : patch_result.value().steps) {
    if (const auto* apply_res = std::get_if<dxp::ApplyRuleResults>(&step.results)) {
      if (apply_res->applied_count > 0) {
        any_applied = true;
        break;
      }
    }
  }

  if (!any_applied) {
    std::cerr << "Expected at least one rule to be applied.\n";
    return 1;
  }

  if (!WriteFile(output_path, patch_result.value().output_bytes)) {
    std::cerr << "Failed to write output file: " << output_path << "\n";
    return 1;
  }

  std::cout << "Patched SM5 shader written to: " << output_path << "\n";
  std::cout << "SM5 emit relative_operand test passed.\n";
  return 0;
}
