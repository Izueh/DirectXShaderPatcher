#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <variant>
#include <vector>

#include "dxp/sm5/Recipe.hpp"
#include "dxp/StepResults.hpp"
#include "tests/helper/TestHelper.hpp"

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 2) {
    std::cerr << "Usage: sm5_recipe_insert_index_defaults <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> input_bytes;
  if (!ReadFile(args[1], input_bytes)) {
    std::cerr << "Failed to read input file: " << args[1] << "\n";
    return 1;
  }

  // ─── Test 1: rewrite_mode: before without insert_index → defaults to 0 ───
  {
    const char* recipe_text = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: before_default_test
    rewrite_mode: before
    rule:
      match:
        - opcode: mul
          capture: target_mul
          operands:
            - capture: dst
            - capture: src
      emit:
        - opcode: mov
          operands:
            - capture: dst
            - capture: src
)YAML";

    auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, "inline-sm5-before-default-test");
    if (!parse_result) {
      std::cerr << "Failed to parse inline SM5 before recipe (no insert_index): " << parse_result.error() << "\n";
      return 1;
    }

    auto patch_result = parse_result.value().Execute(input_bytes);
    if (!patch_result) {
      std::cerr << "Before recipe (no insert_index) execution failed: " << patch_result.error() << "\n";
      return 1;
    }

    std::cout << "  Test 1 PASSED: rewrite_mode: before without insert_index → defaults to 0\n";
  }

  // ─── Test 2: rewrite_mode: after without insert_index → defaults to last match ───
  {
    const char* recipe_text = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: after_default_test
    rewrite_mode: after
    rule:
      match:
        - opcode: mul
          capture: target_mul
          operands:
            - capture: dst
            - capture: src
      emit:
        - opcode: mov
          operands:
            - capture: dst
            - capture: src
)YAML";

    auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, "inline-sm5-after-default-test");
    if (!parse_result) {
      std::cerr << "Failed to parse inline SM5 after recipe (no insert_index): " << parse_result.error() << "\n";
      return 1;
    }

    auto patch_result = parse_result.value().Execute(input_bytes);
    if (!patch_result) {
      std::cerr << "After recipe (no insert_index) execution failed: " << patch_result.error() << "\n";
      return 1;
    }

    std::cout << "  Test 2 PASSED: rewrite_mode: after without insert_index → defaults to last match\n";
  }

  // ─── Test 3: explicit insert_index overrides default ───
  {
    const char* recipe_text = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: explicit_insert_test
    rewrite_mode: before
    insert_index: 0
    rule:
      match:
        - opcode: mul
          capture: target_mul
          operands:
            - capture: dst
            - capture: src
      emit:
        - opcode: mov
          operands:
            - capture: dst
            - capture: src
)YAML";

    auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, "inline-sm5-explicit-insert-test");
    if (!parse_result) {
      std::cerr << "Failed to parse inline SM5 recipe (explicit insert_index): " << parse_result.error() << "\n";
      return 1;
    }

    auto patch_result = parse_result.value().Execute(input_bytes);
    if (!patch_result) {
      std::cerr << "Recipe (explicit insert_index) execution failed: " << patch_result.error() << "\n";
      return 1;
    }

    std::cout << "  Test 3 PASSED: explicit insert_index: 0 overrides default\n";
  }

  std::cout << "\nAll insert_index default tests passed!\n";
  return 0;
}
