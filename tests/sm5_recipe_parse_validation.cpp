#include <filesystem>
#include <iostream>
#include <string>

#include "tests/helper/TestHelper.hpp"

#include "d3d11TokenizedProgramFormat.hpp"
#include "dxp/sm5/Model.hpp"
#include "dxp/sm5/Recipe.hpp"

namespace {

bool Contains(const std::string& text, const std::string& needle) {
  return text.find(needle) != std::string::npos;
}

dxp::sm5::RecipeParseResult ParseFixture(const std::filesystem::path& relative_path) {
  const std::filesystem::path recipe_path = RepoRootPath() / relative_path;
  return dxp::sm5::Recipe::ParseFromFile(recipe_path.string());
}

}  // namespace

int main() {
  // Verify OpcodeUsesTestBoolean for known opcodes
  {
    const auto discard_opcode = dxp::sm5::Opcode{D3D10_SB_OPCODE_DISCARD};
    if (!dxp::sm5::OpcodeUsesTestBoolean(discard_opcode)) {
      std::cerr << "Expected DISCARD to use test_boolean.\n";
      return 1;
    }

    const auto add_opcode = dxp::sm5::Opcode{D3D10_SB_OPCODE_ADD};
    if (dxp::sm5::OpcodeUsesTestBoolean(add_opcode)) {
      std::cerr << "Expected ADD not to use test_boolean.\n";
      return 1;
    }
  }

  {
    auto parse_result = ParseFixture("tests/recipes/sm5_parse_validation_portable_v1.yml");
    if (!parse_result) {
      std::cerr << "Expected portable schema form to parse: " << parse_result.error() << "\n";
      return 1;
    }
  }

  {
    auto parse_result = ParseFixture("tests/recipes/sm5_parse_validation_resource_uav_decl.yml");
    if (!parse_result) {
      std::cerr << "Expected raw/structured resource and UAV declarations to parse: " << parse_result.error()
                << "\n";
      return 1;
    }
  }

  {
    auto parse_result = ParseFixture("tests/recipes/sm5_parse_validation_dcl_opcode_coverage.yml");
    if (!parse_result) {
      std::cerr << "Expected expanded DCL opcode names to parse: " << parse_result.error() << "\n";
      return 1;
    }
  }

  {
    auto parse_result = ParseFixture("tests/recipes/sm5_parse_validation_valid_interpolation.yml");
    if (!parse_result) {
      std::cerr << "Expected interpolation on dcl_input_ps to parse: " << parse_result.error() << "\n";
      return 1;
    }
  }

  {
    auto parse_result = ParseFixture("tests/recipes/sm5_parse_validation_step_kinds.yml");
    if (!parse_result) {
      std::cerr << "Expected SM5 step kinds to parse: " << parse_result.error() << "\n";
      return 1;
    }
  }

  {
    auto parse_result = ParseFixture("tests/recipes/sm5_parse_validation_add_temp_handle.yml");
    if (!parse_result) {
      std::cerr << "Expected add_temp handles to satisfy temp handle "
                   "validation: "
                << parse_result.error() << "\n";
      return 1;
    }
  }

  {
    auto parse_result = ParseFixture("tests/recipes/sm5_parse_validation_add_temp_handles_handle.yml");
    if (!parse_result) {
      std::cerr << "Expected add_temp handles list to satisfy temp "
                   "handle validation: "
                << parse_result.error() << "\n";
      return 1;
    }
  }

  {
    auto parse_result = ParseFixture("tests/recipes/sm5_parse_validation_emit_immediate_shorthand.yml");
    if (!parse_result) {
      std::cerr << "Expected SM5 emit immediate shorthand to parse: " << parse_result.error() << "\n";
      return 1;
    }
  }

  {
    auto parse_result = ParseFixture("tests/recipes/sm5_parse_validation_modes.yml");
    if (!parse_result) {
      std::cerr << "Expected SM5 before/after indexed-anchor rewrite "
                   "modes to parse: "
                << parse_result.error() << "\n";
      return 1;
    }
  }

  {
    auto parse_result = ParseFixture("tests/recipes/sm5_parse_validation_required_match_and_state.yml");
    if (!parse_result) {
      std::cerr << "Expected SM5 required_match parsing and name-driven "
                   "rule-state publication schema to parse: "
                << parse_result.error() << "\n";
      return 1;
    }
  }

  {
    auto parse_result = ParseFixture("tests/recipes/sm5_parse_validation_predicate_ops.yml");
    if (!parse_result) {
      std::cerr << "Expected SM5 predicate operators (eq/ne/gt/gte/lt/lte, and/or) to parse: "
                << parse_result.error() << "\n";
      return 1;
    }
  }

  {
    // Conflicting comparison operator validation now happens at execution time
    // Parse should succeed; execution will fail with the validation error
    auto parse_result = ParseFixture("tests/recipes/sm5_parse_validation_invalid_predicate_selector.yml");
    if (!parse_result) {
      std::cerr << "Conflicting comparison operators should now pass parse-time validation (moved to execution-time).\n";
      return 1;
    }
  }

  {
    // Interpolation validation moved to execution time
    auto parse_result = ParseFixture("tests/recipes/sm5_parse_validation_invalid_interpolation.yml");
    if (!parse_result) {
      std::cerr << "Interpolation validation should now pass parse-time validation (moved to execution-time).\n";
      return 1;
    }
  }

  {
    auto parse_result = ParseFixture("tests/recipes/sm5_parse_validation_valid_check_step.yml");
    if (!parse_result) {
      std::cerr << "Expected equivalent step-based check schema to parse: " << parse_result.error() << "\n";
      return 1;
    }
  }

  {
    // check_opcode_count with opcodes array should parse
    auto parse_result = ParseFixture("tests/recipes/sm5_parse_validation_check_opcode_count.yml");
    if (!parse_result) {
      std::cerr << "Expected check_opcode_count with opcodes array to parse: " << parse_result.error() << "\n";
      return 1;
    }
  }

  {
    // check_resource_count should parse without expected_resources
    auto parse_result = ParseFixture("tests/recipes/sm5_parse_validation_check_resource_count.yml");
    if (!parse_result) {
      std::cerr << "Expected check_resource_count to parse: " << parse_result.error() << "\n";
      return 1;
    }
  }

  {
    // Empty opcodes validation moved to execution time
    auto parse_result = ParseFixture("tests/recipes/sm5_parse_validation_invalid_check_opcode_count.yml");
    if (!parse_result) {
      std::cerr << "Empty opcodes validation should now pass parse-time validation (moved to execution-time).\n";
      return 1;
    }
  }

  {
    auto parse_result = ParseFixture(
        "tests/recipes/"
        "sm5_parse_validation_invalid_equivalent_check_step.yml");
    if (parse_result) {
      std::cerr << "Expected equivalent check-step validation to fail "
                   "parsing.\n";
      return 1;
    }

    if (!Contains(parse_result.error(), "minor version is required")) {
      std::cerr << "Expected equivalent check-step validation error.\n";
      return 1;
    }
  }

  {
    // Duplicate name validation moved to execution time
    auto parse_result = ParseFixture(
        "tests/recipes/"
        "sm5_parse_validation_invalid_duplicate_name_cross_kind.yml");
    if (!parse_result) {
      std::cerr << "Duplicate names should now pass parse-time validation (moved to execution-time).\n";
      return 1;
    }
  }

  {
    // Rule name validation moved to execution time
    auto parse_result = ParseFixture(
        "tests/recipes/"
        "sm5_parse_validation_invalid_missing_rule_name.yml");
    if (!parse_result) {
      std::cerr << "Rule names should now pass parse-time validation (moved to execution-time).\n";
      return 1;
    }
  }

  {
    // Handle reference validation moved to execution time
    auto parse_result = ParseFixture(
        "tests/recipes/"
        "sm5_parse_validation_invalid_from_handle_reference.yml");
    if (!parse_result) {
      std::cerr << "Handle references should now pass parse-time validation (moved to execution-time).\n";
      return 1;
    }
  }

  {
    // Index immediate values must be numeric — non-numeric strings fail YAML deserialization
    auto parse_result = ParseFixture(
        "tests/recipes/"
        "sm5_parse_validation_invalid_index_immediate_string.yml");
    if (parse_result) {
      std::cerr << "Expected string index immediate value to fail parsing.\n";
      return 1;
    }

    if (parse_result.error().empty()) {
      std::cerr << "Expected parse error for invalid immediate_lo value.\n";
      return 1;
    }
  }

  {
    // Add_resource name validation moved to execution time
    auto parse_result = ParseFixture(
        "tests/recipes/"
        "sm5_parse_validation_invalid_add_temp_missing_handle.yml");
    if (!parse_result) {
      std::cerr << "Add_resource name should now pass parse-time validation (moved to execution-time).\n";
      return 1;
    }
  }

  {
    // Emit indices/immediate shorthand is data transformation (both result in index_patterns)
    auto parse_result = ParseFixture(
        "tests/recipes/"
        "sm5_parse_validation_invalid_emit_indices_and_immediate_"
        "shorthand.yml");
    if (parse_result) {
      std::cerr << "Expected mixed emit index declaration styles to fail parsing.\n";
      return 1;
    }

    if (!Contains(parse_result.error(),
                  "SM5 emit operands may use explicit indices or "
                  "immediate shorthand arrays")) {
      std::cerr << "Expected emit shorthand exclusivity validation error.\n";
      return 1;
    }
  }

  {
    // Match-side immediate shorthand now supported (both result in index_patterns)
    const char* recipe_text = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: match_immediate_shorthand
    rule:
      match:
        - opcode: mul
          operands:
          - capture: dst
          - immediates_u32: [0]
      emit:
        - opcode: mov
          operands:
            - capture: dst
            - type: temp
              indices:
                - representation: relative
)YAML";
    auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, "inline-sm5-match-immediate-shorthand-test");
    if (!parse_result) {
      std::cerr << "Expected match-side shorthand immediate usage to parse successfully.\n";
      return 1;
    }
  }

  {
    // match_capture must reference a known operand capture; an instruction
    // capture name is rejected at validate time (not at YAML parse/compile).
    auto parse_result = ParseFixture(
        "tests/recipes/"
        "sm5_parse_validation_invalid_match_capture_operand_name.yml");
    if (!parse_result) {
      std::cerr << "Expected match_capture fixture to parse; validation happens separately.\n";
      return 1;
    }
    auto validate_result = dxp::sm5::ValidateRecipe(parse_result.value());
    if (validate_result) {
      std::cerr << "Expected operand match_capture referencing an instruction capture to fail validation.\n";
      return 1;
    }
  }

  {
    // Index match_capture must reference a known index capture; an operand
    // capture name is rejected at validate time.
    auto parse_result = ParseFixture(
        "tests/recipes/"
        "sm5_parse_validation_invalid_match_capture_index_name.yml");
    if (!parse_result) {
      std::cerr << "Expected index match_capture fixture to parse; validation happens separately.\n";
      return 1;
    }
    auto validate_result = dxp::sm5::ValidateRecipe(parse_result.value());
    if (validate_result) {
      std::cerr << "Expected index match_capture referencing an operand capture to fail validation.\n";
      return 1;
    }
  }

  {
    auto parse_result = ParseFixture("tests/recipes/sm5_parse_validation_invalid_operand_components_selection_mode.yml");
    if (parse_result) {
      std::cerr << "Expected invalid operand components.selection_mode to fail parsing.\n";
      return 1;
    }

    if (!Contains(parse_result.error(), "unexpected_enum")) {
      std::cerr << "Expected operand components.selection_mode validation error.\n";
      return 1;
    }
  }

  {
    // Condition comparison operands are typed: a literal may appear on either side.
    const char* recipe_text = R"YAML(version: 1
steps:
  - kind: check_opcode_count
    name: count_ops
    opcodes: [mov]
  - kind: apply_rule
    name: literal_lhs_guard
    rewrite_mode: none
    condition:
      eq:
        lhs: 0
        rhs: count_ops.mov
    rule:
      match:
        - opcode: mov
)YAML";
    auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, "inline-sm5-typed-condition-lhs-test");
    if (!parse_result) {
      std::cerr << "Expected typed condition lhs (literal) to parse successfully: " << parse_result.error() << "\n";
      return 1;
    }
    auto validate_result = dxp::sm5::ValidateRecipe(parse_result.value());
    if (!validate_result) {
      std::cerr << "Expected typed condition lhs recipe to validate: " << validate_result.error() << "\n";
      return 1;
    }
  }

  std::cout << "SM5 parser covers all non-reserved opcode names, accepts "
               "test-boolean opcode aliases, and rejects invalid schema "
               "combinations.\n";
  return 0;
}
