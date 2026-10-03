#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <format>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <vector>

#include "dxp/ExportTypes.hpp"
#include "dxp/PatchOptions.hpp"
#include "dxp/sm6/Recipe.hpp"
#include "tests/helper/TestHelper.hpp"

namespace {
// Numeric literals below are explicit regression inputs and expected shader values.
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
std::string RecipeText(const std::string& integer, const std::string& scalar) {
  return std::format(R"YAML(
env:
  register_number: 3
  noise_scale: 0.375
steps:
- kind: add_resource
  name: constants
  cbuffers:
  - handle: env_test_constants
    space: 50
    register_index: 31
    size: 1024
    fields:
    - name: first_register
      type: F32
      width: 4
      offset: 0
    - name: last_register
      type: F32
      width: 4
      offset: 1008
- kind: apply_rule
  name: consume_env_values
  required: true
  rewrite_mode: replace
  rule:
    match:
    - opcode: Frc
      capture: old_frc
      operands:
      - index: 1
        capture: input_value
    emit:
    - opcode: CBufferLoadLegacy
      name: env_register_load
      result_component_type: F32
      operands:
      - index: 1
        kind: resource
        handle: env_test_constants
      - index: 2
        kind: constant
        {}
    - name: env_register_x
      aggregate: env_register_load
      extract_index: 0
    - opcode: fadd
      name: input_plus_register
      result_component_type: F32
      operands:
      - index: 0
        capture: input_value
      - index: 1
        capture: env_register_x
    - opcode: fmul
      name: scaled_input
      result_component_type: F32
      operands:
      - index: 0
        capture: input_plus_register
      - index: 1
        kind: constant
        {}
    - opcode: Frc
      name: new_frc
      result_component_type: F32
      replace_captured: old_frc
      operands:
      - index: 1
        capture: scaled_input
)YAML",
                     integer, scalar);
}
}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc != 2) return 1;
    const ScopedCoInitialize coinit;
    std::vector<uint8_t> input;
    const std::span arguments(argv, static_cast<size_t>(argc));
    if (!ReadFile(arguments[1], input)) return 1;
    auto recipe = dxp::sm6::Recipe::ParseFromText(RecipeText("constant_int_values: [register_number]", "constant_float_values: [noise_scale]"));
    if (!recipe) {
      std::cerr << recipe.error();
      return 1;
    }

    const auto check = [&](dxp::PrimitiveValue integer, int expected_integer,
                           const std::string& expected_scalar, dxp::PrimitiveValue scalar) {
      dxp::PatchOptions options;
      options.SetEnv("register_number", integer);
      options.SetEnv("noise_scale", scalar);
      auto actual = recipe->Execute(input, options);
      auto literal = dxp::sm6::Recipe::ParseFromText(RecipeText(
          "constant_int_values: [" + std::to_string(expected_integer) + "]",
          "constant_float_values: [" + expected_scalar + "]"));
      if (!actual || !literal) {
        std::cerr << (actual ? literal.error() : actual.error());
        return false;
      }
      auto expected = literal->Execute(input);
      if (!expected || actual->output_bytes != expected->output_bytes) {
        std::cerr << "Env constants differed from explicitly typed literal output\n";
        return false;
      }
      return true;
    };
    // All PrimitiveValue integer alternatives must become i32, not i64;
    // both whole-number and fractional scale settings must become f32.
    if (!check(int32_t{3}, 3, "0.375", 0.375) || !check(uint32_t{8}, 8, "0.5", 0.5) || !check(int64_t{16}, 16, "0.75", 0.75) || !check(uint64_t{32}, 32, "2.0", int32_t{2}) || !check(8.0, 8, "2.0", uint64_t{2}) || !check(true, 1, "0.0", false)) return 1;

    auto yaml_default = recipe->Execute(input);
    auto default_literal = dxp::sm6::Recipe::ParseFromText(RecipeText("constant_int_values: [3]", "constant_float_values: [0.375]"));
    if (!yaml_default || !default_literal) return 1;
    auto expected_default = default_literal->Execute(input);
    if (!expected_default || yaml_default->output_bytes != expected_default->output_bytes) return 1;

    for (const dxp::PrimitiveValue bad : {dxp::PrimitiveValue{1.5}, dxp::PrimitiveValue{uint64_t{1} << 40},
                                          dxp::PrimitiveValue{std::numeric_limits<double>::infinity()}}) {
      dxp::PatchOptions options;
      options.SetEnv("register_number", bad);
      auto result = recipe->Execute(input, options);
      if (result || result.error().find("representable by i32") == std::string::npos) return 1;
    }
    auto missing = dxp::sm6::Recipe::ParseFromText(RecipeText("constant_int_values: [absent_variable]", "constant_float_values: [noise_scale]"));
    if (!missing) return 1;
    auto missing_result = missing->Execute(input);
    if (missing_result || missing_result.error().find("unknown environment constant 'absent_variable'") == std::string::npos) return 1;

    // Array references work in YAML match filters, and each Execute sees its own overrides.
    auto match_recipe = dxp::sm6::Recipe::ParseFromText(R"YAML(
env:
  opcode_number: 22
steps:
- kind: apply_rule
  name: match_env_opcode
  required: false
  rewrite_mode: none
  match_mode: match_all
  rule:
    match:
    - opcode: Frc
      operands:
      - index: 0
        kind: constant
        capture: matched_opcode
        export_as: opcode_export
        constant_int_values: [opcode_number]
)YAML");
    if (!match_recipe) {
      std::cerr << match_recipe.error();
      return 1;
    }
    auto matched = match_recipe->Execute(input);
    if (!matched || matched->immediate_values.at("opcode_export").raw_values != std::vector<uint64_t>{22}) return 1;
    dxp::PatchOptions unmatched_options;
    unmatched_options.SetEnv("opcode_number", int32_t{23});
    auto unmatched = match_recipe->Execute(input, unmatched_options);
    if (!unmatched || std::get<dxp::ApplyRuleResults>(unmatched->steps[0].results).match_count != 0) return 1;
    auto matched_again = match_recipe->Execute(input);
    if (!matched_again || std::get<dxp::ApplyRuleResults>(matched_again->steps[0].results).match_count == 0) return 1;
    dxp::PatchOptions invalid_match_options;
    invalid_match_options.SetEnv("opcode_number", 1.5);
    auto invalid_match = match_recipe->Execute(input, invalid_match_options);
    if (invalid_match || invalid_match.error().find("representable by i32") == std::string::npos) return 1;

    auto old_capture = dxp::sm6::Recipe::ParseFromText(RecipeText("capture: register_number", "constant_float_values: [noise_scale]"));
    if (!old_capture) return 1;
    auto old_result = old_capture->Execute(input);
    if (old_result || old_result.error().find("requires 'constant_int_values'") == std::string::npos) return 1;

    auto conflicting_type = dxp::sm6::Recipe::ParseFromText(RecipeText(
        "constant_int_values: [register_number]\n        component_type: I64", "constant_float_values: [noise_scale]"));
    if (!conflicting_type) return 1;
    auto conflicting_result = conflicting_type->Execute(input);
    if (conflicting_result || conflicting_result.error().find("component_type disagrees with operand type i32") == std::string::npos) return 1;
    auto wrong_family = dxp::sm6::Recipe::ParseFromText(RecipeText(
        "constant_float_values: [register_number]", "constant_float_values: [noise_scale]"));
    if (!wrong_family) return 1;
    auto wrong_family_result = wrong_family->Execute(input);
    if (wrong_family_result || wrong_family_result.error().find("incompatible with i32") == std::string::npos) return 1;

    // Captured cast operands keep their source type, rather than inheriting the
    // result type. Exercise both cast spelling forms used by recipe emitters.
    for (const char* cast_field : {"opcode", "cast_opcode"}) {
      std::string cast_text = RecipeText("constant_int_values: [register_number]", "constant_float_values: [noise_scale]");
      const auto insert = cast_text.find("    - opcode: fmul\n");
      cast_text.insert(insert, std::format(R"YAML(    - {}: fptosi
      name: quantized_input
      result_component_type: I32
      operands:
      - index: 0
        capture: input_plus_register
    - {}: sitofp
      name: cast_back_input
      result_component_type: F32
      operands:
      - index: 0
        capture: quantized_input
)YAML",
                                           cast_field, cast_field));
      const auto scalar_input = cast_text.find("capture: input_plus_register", cast_text.find("    - opcode: fmul\n"));
      cast_text.replace(scalar_input, std::string("capture: input_plus_register").size(), "capture: cast_back_input");
      auto cast_recipe = dxp::sm6::Recipe::ParseFromText(cast_text);
      if (!cast_recipe) {
        std::cerr << cast_recipe.error();
        return 1;
      }
      auto cast_result = cast_recipe->Execute(input);
      if (!cast_result) {
        std::cerr << cast_result.error();
        return 1;
      }

      auto constant_cast = cast_text;
      const auto source = constant_cast.find("capture: input_plus_register");
      constant_cast.replace(source, std::string("capture: input_plus_register").size(),
                            "kind: constant\n        component_type: F32\n        constant_float_values: [noise_scale]");
      auto constant_recipe = dxp::sm6::Recipe::ParseFromText(constant_cast);
      if (!constant_recipe) {
        std::cerr << constant_recipe.error();
        return 1;
      }
      auto constant_result = constant_recipe->Execute(input);
      if (!constant_result) {
        std::cerr << constant_result.error();
        return 1;
      }
      constant_cast.erase(constant_cast.find("        component_type: F32\n"), std::string("        component_type: F32\n").size());
      auto missing_type_recipe = dxp::sm6::Recipe::ParseFromText(constant_cast);
      if (!missing_type_recipe) return 1;
      auto missing_type_result = missing_type_recipe->Execute(input);
      if (missing_type_result || missing_type_result.error().find("cast constant source requires component_type") == std::string::npos) return 1;
      cast_text.replace(cast_text.find(": fptosi"), std::string(": fptosi").size(), ": uitofp");
      auto invalid_recipe = dxp::sm6::Recipe::ParseFromText(cast_text);
      if (!invalid_recipe) {
        std::cerr << invalid_recipe.error();
        return 1;
      }
      auto invalid_result = invalid_recipe->Execute(input);
      if (invalid_result || invalid_result.error().find("invalid cast source/result types") == std::string::npos) {
        std::cerr << cast_field << " invalid cast: " << (invalid_result ? "unexpected success" : invalid_result.error()) << '\n';
        return 1;
      }
    }
    std::cout << "SM6 numeric env constants and captured cast type/error cases passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::fputs(error.what(), stderr);
    std::fputc('\n', stderr);
    return 1;
  } catch (...) {
    std::fputs("Unexpected exception\n", stderr);
    return 1;
  }
}
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
