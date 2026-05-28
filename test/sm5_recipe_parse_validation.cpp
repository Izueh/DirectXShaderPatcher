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

} // namespace

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
    if (!ParseFixture("test/recipes/sm5_parse_validation_rewrite_modes.yml",
                      parseResult)) {
      std::cerr << "Expected SM5 Before/After rewrite modes to parse: "
                << parseResult.Error << "\n";
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
        if (ParseFixture(
          "test/recipes/sm5_parse_validation_invalid_legacy_top_level.yml",
            parseResult)) {
      std::cerr << "Expected legacy top-level usage to fail parsing.\n";
      return 1;
    }

    if (!Contains(parseResult.Error,
                  "top-level prefilters are deprecated and unsupported in "
                  "schema version 1; use prefilter steps")) {
      std::cerr << "Expected top-level prefilter rejection error.\n";
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
                  "SM5 step mode is only valid for apply_rules or prefilter "
                  "steps")) {
      std::cerr << "Expected step mode validation error.\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeParseResult parseResult;
    if (ParseFixture("test/recipes/"
                     "sm5_parse_validation_invalid_bind_handle_reference.yml",
                     parseResult)) {
      std::cerr << "Expected unknown bind_handle reference to fail parsing.\n";
      return 1;
    }

    if (!Contains(parseResult.Error,
                  "unknown resource declaration handle 'missing_texture'")) {
      std::cerr << "Expected strict bind_handle validation error for unknown "
                   "handle.\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeParseResult parseResult;
        if (ParseFixture(
          "test/recipes/sm5_parse_validation_invalid_legacy_selector.yml",
            parseResult)) {
      std::cerr << "Expected legacy selector usage to fail parsing.\n";
      return 1;
    }

    if (!Contains(parseResult.Error,
                  "SM5 operands require components.kind/components.value "
                  "instead of mask/swizzle/select")) {
      std::cerr << "Expected component selector validation error.\n";
      return 1;
    }
  }

  std::cout << "SM5 parser covers all non-reserved opcode names, accepts "
               "test-boolean opcode aliases, and rejects deprecated fields.\n";
  return 0;
}
