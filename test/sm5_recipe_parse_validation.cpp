#include "dxp/sm5/RecipeParse.h"

#include "d3d11TokenizedProgramFormat.hpp"
#include "dxp/sm5/Model.h"

#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

static bool Contains(const std::string &text, const std::string &needle) {
  return text.find(needle) != std::string::npos;
}

static std::filesystem::path RepoRootPath() {
  return std::filesystem::path(__FILE__).parent_path().parent_path();
}

static bool ParseFixture(const std::filesystem::path &relativePath,
                         dxp::sm5::RecipeParseResult &parseResult) {
  const std::filesystem::path recipePath = RepoRootPath() / relativePath;
  return dxp::sm5::ParseRecipeFile(recipePath.string(), parseResult);
}

static bool IsReservedOpcode(uint32_t opcodeValue) {
  switch (static_cast<D3D10_SB_OPCODE_TYPE>(opcodeValue)) {
  case D3D10_SB_OPCODE_RESERVED0:
  case D3D10_1_SB_OPCODE_RESERVED1:
  case D3D11_SB_OPCODE_RESERVED0:
  case D3D11_1_SB_OPCODE_RESERVED0:
  case D3DWDDM1_3_SB_OPCODE_RESERVED0:
    return true;
  default:
    return false;
  }
}

}

int main() {
  for (uint32_t opcodeValue = 0; opcodeValue < D3D10_SB_NUM_OPCODES;
       ++opcodeValue) {
    if (IsReservedOpcode(opcodeValue)) {
      continue;
    }

    const dxp::sm5::Opcode opcode{opcodeValue};
    const char *opcodeName = dxp::sm5::GetOpcodeName(opcode);
    if (std::strcmp(opcodeName, "unknown") == 0) {
      std::cerr << "Expected opcode value " << opcodeValue
                << " to have a canonical SM5 name.\n";
      return 1;
    }

    dxp::sm5::Opcode parsedOpcode;
    if (!dxp::sm5::ParseOpcode(opcodeName, parsedOpcode) ||
        parsedOpcode != opcode) {
      std::cerr << "Expected SM5 opcode name '" << opcodeName
                << "' to round-trip through ParseOpcode.\n";
      return 1;
    }
  }

  {
    dxp::sm5::Opcode opcode;
    int32_t implicitTestBoolean = -1;
    if (!dxp::sm5::ParseOpcodeWithImplicitTestBoolean("discard_z", opcode,
                                                      implicitTestBoolean) ||
        opcode != dxp::sm5::Opcode{D3D10_SB_OPCODE_DISCARD} ||
        implicitTestBoolean != D3D10_SB_INSTRUCTION_TEST_ZERO ||
        !dxp::sm5::OpcodeUsesTestBoolean(opcode)) {
      std::cerr << "Expected discard_z to resolve to discard with zero test_boolean.\n";
      return 1;
    }

    if (!dxp::sm5::ParseOpcodeWithImplicitTestBoolean("retc_nz", opcode,
                                                      implicitTestBoolean) ||
        opcode != dxp::sm5::Opcode{D3D10_SB_OPCODE_RETC} ||
        implicitTestBoolean != D3D10_SB_INSTRUCTION_TEST_NONZERO ||
        !dxp::sm5::OpcodeUsesTestBoolean(opcode)) {
      std::cerr << "Expected retc_nz to resolve to retc with nonzero test_boolean.\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeParseResult parseResult;
    if (!ParseFixture("test/recipes/sm5_parse_validation_portable_v1.yml",
                      parseResult)) {
      std::cerr << "Expected portable schema form to parse: "
                << parseResult.Error << "\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeParseResult parseResult;
        if (!ParseFixture(
          "test/recipes/sm5_parse_validation_resource_uav_decl.yml",
            parseResult)) {
      std::cerr
          << "Expected raw/structured resource and UAV declarations to parse: "
          << parseResult.Error << "\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeParseResult parseResult;
        if (!ParseFixture(
          "test/recipes/sm5_parse_validation_dcl_opcode_coverage.yml",
            parseResult)) {
      std::cerr << "Expected expanded DCL opcode names to parse: "
                << parseResult.Error << "\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeParseResult parseResult;
        if (!ParseFixture(
          "test/recipes/sm5_parse_validation_valid_interpolation_mode.yml",
            parseResult)) {
      std::cerr << "Expected interpolation_mode on dcl_input_ps to parse: "
                << parseResult.Error << "\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeParseResult parseResult;
    if (!ParseFixture("test/recipes/sm5_parse_validation_step_kinds.yml",
                      parseResult)) {
      std::cerr << "Expected SM5 step kinds to parse: " << parseResult.Error
                << "\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeParseResult parseResult;
    if (!ParseFixture(
            "test/recipes/sm5_parse_validation_add_temp_from_handle.yml",
            parseResult)) {
      std::cerr << "Expected add_temp handles to satisfy temp from_handle "
                   "validation: "
                << parseResult.Error << "\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeParseResult parseResult;
    if (!ParseFixture(
            "test/recipes/sm5_parse_validation_add_temp_handles_from_handle.yml",
            parseResult)) {
      std::cerr << "Expected add_temp handles list to satisfy temp "
                   "from_handle validation: "
                << parseResult.Error << "\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeParseResult parseResult;
    if (!ParseFixture(
            "test/recipes/sm5_parse_validation_emit_immediate_shorthand.yml",
            parseResult)) {
      std::cerr << "Expected SM5 emit immediate shorthand to parse: "
                << parseResult.Error << "\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeParseResult parseResult;
    if (!ParseFixture("test/recipes/sm5_parse_validation_rewrite_modes.yml",
                      parseResult)) {
      std::cerr << "Expected SM5 before/after indexed-anchor rewrite "
                   "modes to parse: "
                << parseResult.Error << "\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeParseResult parseResult;
    if (!ParseFixture(
            "test/recipes/sm5_parse_validation_required_match_and_state.yml",
            parseResult)) {
      std::cerr << "Expected SM5 required_match parsing and name-driven "
                   "rule-state publication schema to parse: "
                << parseResult.Error << "\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeParseResult parseResult;
    if (!ParseFixture("test/recipes/sm5_parse_validation_predicate_ops.yml",
                      parseResult)) {
      std::cerr << "Expected SM5 predicate operators (eq/ne/gt/gte/lt/lte, and/or) to parse: "
                << parseResult.Error << "\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeParseResult parseResult;
    if (ParseFixture(
            "test/recipes/sm5_parse_validation_invalid_predicate_selector.yml",
            parseResult)) {
      std::cerr << "Expected predicate comparison with both state and input "
                   "selectors to fail parsing.\n";
      return 1;
    }

    if (!Contains(parseResult.Error,
                  "SM5 step if comparison requires exactly one of state or "
                  "input")) {
      std::cerr << "Expected predicate selector exclusivity validation "
                   "error.\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeParseResult parseResult;
        if (ParseFixture(
          "test/recipes/sm5_parse_validation_invalid_interpolation_mode.yml",
            parseResult)) {
      std::cerr << "Expected interpolation_mode on non-dcl_input_ps opcode to "
                   "fail parsing.\n";
      return 1;
    }

    if (!Contains(parseResult.Error, "interpolation_mode is only valid for "
                                     "dcl_input_ps and dcl_input_ps_siv")) {
      std::cerr << "Expected interpolation_mode opcode validation error.\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeParseResult parseResult;
    if (!ParseFixture(
            "test/recipes/sm5_parse_validation_valid_check_step.yml",
            parseResult)) {
      std::cerr << "Expected equivalent step-based check schema to parse: "
                << parseResult.Error << "\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeParseResult parseResult;
    if (ParseFixture(
            "test/recipes/sm5_parse_validation_invalid_top_level_rewrite_rules.yml",
            parseResult)) {
      std::cerr << "Expected top-level rewrite_rules to fail parsing.\n";
      return 1;
    }

    if (!Contains(parseResult.Error,
                  "schema version 1 requires steps and does not allow top-level rewrite_rules")) {
      std::cerr << "Expected top-level rewrite_rules validation error.\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeParseResult parseResult;
    if (ParseFixture(
            "test/recipes/"
            "sm5_parse_validation_invalid_equivalent_check_step.yml",
            parseResult)) {
      std::cerr << "Expected equivalent check-step validation to fail "
                   "parsing.\n";
      return 1;
    }

    if (!Contains(parseResult.Error,
                  "SM5 check_shader_version steps require major and minor")) {
      std::cerr << "Expected equivalent check-step validation error.\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeParseResult parseResult;
    if (ParseFixture("test/recipes/sm5_parse_validation_invalid_step_mode.yml",
                     parseResult)) {
      std::cerr << "Expected mode on non-apply_rules step to fail parsing.\n";
      return 1;
    }

    if (!Contains(parseResult.Error,
                  "SM5 step mode is only valid for apply_rules steps")) {
      std::cerr << "Expected step mode validation error.\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeParseResult parseResult;
    if (ParseFixture(
            "test/recipes/"
            "sm5_parse_validation_invalid_duplicate_name_cross_kind.yml",
            parseResult)) {
      std::cerr << "Expected duplicate names across steps/rules to fail "
                   "parsing.\n";
      return 1;
    }

    if (!Contains(parseResult.Error,
                  "duplicate SM5 name 'shared_name' reused by rule "
                  "(already used by step)")) {
      std::cerr << "Expected cross-kind duplicate name validation error.\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeParseResult parseResult;
    if (ParseFixture(
            "test/recipes/"
            "sm5_parse_validation_invalid_missing_rule_name.yml",
            parseResult)) {
      std::cerr << "Expected missing explicit rule names to fail parsing.\n";
      return 1;
    }

    if (!Contains(parseResult.Error,
                  "SM5 rule names are required and must be unique")) {
      std::cerr << "Expected explicit rule-name requirement validation "
                   "error.\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeParseResult parseResult;
    if (ParseFixture("test/recipes/"
                     "sm5_parse_validation_invalid_from_handle_reference.yml",
                     parseResult)) {
      std::cerr << "Expected unknown from_handle reference to fail parsing.\n";
      return 1;
    }

    if (!Contains(parseResult.Error,
                  "unknown resource declaration handle 'missing_texture'")) {
      std::cerr << "Expected strict from_handle validation error for unknown "
                   "handle.\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeParseResult parseResult;
    if (ParseFixture("test/recipes/"
                     "sm5_parse_validation_invalid_index_immediate_string.yml",
                     parseResult)) {
      std::cerr << "Expected string index immediate value to fail parsing.\n";
      return 1;
    }

    if (!Contains(parseResult.Error,
                  "index immediate_lo only accepts integer literals")) {
      std::cerr << "Expected integer-only index immediate validation error.\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeParseResult parseResult;
    if (ParseFixture(
            "test/recipes/"
            "sm5_parse_validation_invalid_add_temp_missing_handle.yml",
            parseResult)) {
      std::cerr << "Expected add_temp without handle to fail parsing.\n";
      return 1;
    }

    if (!Contains(parseResult.Error, "add_temp steps require handles")) {
      std::cerr << "Expected add_temp handle validation error.\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeParseResult parseResult;
    if (ParseFixture(
            "test/recipes/"
            "sm5_parse_validation_invalid_add_temp_handle_and_handles.yml",
            parseResult)) {
      std::cerr << "Expected add_temp with both handle and handles to fail "
                   "parsing.\n";
      return 1;
    }

    if (!Contains(parseResult.Error,
                  "add_temp steps no longer support handle; use handles")) {
      std::cerr << "Expected add_temp handle/handles exclusivity "
                   "validation error.\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeParseResult parseResult;
    if (ParseFixture(
            "test/recipes/"
            "sm5_parse_validation_invalid_emit_indices_and_immediate_"
            "shorthand.yml",
            parseResult)) {
      std::cerr << "Expected mixed emit index declaration styles to fail "
                   "parsing.\n";
      return 1;
    }

    if (!Contains(parseResult.Error,
                  "SM5 emit operands may use explicit indices or "
                  "immediate shorthand arrays "
                  "(immediates_u32/immediates_u64/immediates_i32/"
                  "immediates_i64/immediates_f32/immediates_f64), but not "
                  "both")) {
      std::cerr << "Expected emit shorthand exclusivity validation error.\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeParseResult parseResult;
    if (ParseFixture("test/recipes/"
                     "sm5_parse_validation_invalid_match_immediate_"
                     "shorthand.yml",
                     parseResult)) {
      std::cerr << "Expected match-side shorthand immediate usage to fail "
                   "parsing.\n";
      return 1;
    }

    if (!Contains(parseResult.Error,
                  "SM5 immediates_u32/immediates_u64/immediates_i32/"
                  "immediates_i64/immediates_f32/immediates_f64 are only "
                  "valid on emit operands")) {
      std::cerr << "Expected emit-only shorthand validation error.\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeParseResult parseResult;
    if (ParseFixture(
            "test/recipes/"
            "sm5_parse_validation_invalid_operand_match_capture_kind.yml",
            parseResult)) {
      std::cerr << "Expected operand match_capture kind mismatch to fail parsing.\n";
      return 1;
    }

    if (!Contains(parseResult.Error,
                  "operand match_capture 'op_tok' expects operand capture but "
                  "found instruction capture")) {
      std::cerr << "Expected operand match_capture kind validation error.\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeParseResult parseResult;
    if (ParseFixture(
            "test/recipes/"
            "sm5_parse_validation_invalid_index_match_capture_kind.yml",
            parseResult)) {
      std::cerr << "Expected index match_capture kind mismatch to fail parsing.\n";
      return 1;
    }

    if (!Contains(parseResult.Error,
                  "index match_capture 'dst' expects index capture but found "
                  "operand capture")) {
      std::cerr << "Expected index match_capture kind validation error.\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeParseResult parseResult;
    if (ParseFixture(
            "test/recipes/sm5_parse_validation_invalid_operand_components_kind.yml",
            parseResult)) {
      std::cerr << "Expected invalid operand components.kind to fail parsing.\n";
      return 1;
    }

    if (!Contains(parseResult.Error, "unsupported operand components.kind")) {
      std::cerr << "Expected operand components.kind validation error.\n";
      return 1;
    }
  }

  std::cout << "SM5 parser covers all non-reserved opcode names, accepts "
               "test-boolean opcode aliases, and rejects invalid schema "
               "combinations.\n";
  return 0;
}
