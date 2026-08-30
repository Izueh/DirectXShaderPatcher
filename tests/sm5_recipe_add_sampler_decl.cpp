#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <variant>
#include <vector>

#include "dxp/ExportTypes.hpp"
#include "dxp/sm5/Recipe.hpp"
#include "dxp/StepResults.hpp"
#include "tests/helper/TestHelper.hpp"

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 2) {
    std::cerr << "Usage: sm5_recipe_add_sampler_decl <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> input_bytes;
  if (!ReadFile(args[1], input_bytes)) {
    std::cerr << "Failed to read input file: " << args[1] << "\n";
    return 1;
  }

  const char* recipe_text = R"YAML(version: 1
steps:
  - kind: check_opcode_count
    name: check_before
    opcodes: [dcl_sampler]
  - kind: add_resource
    name: add_s11
    samplers:
      - handle: add_s11
        register_index: 11
        mode: comparison
  - kind: check_opcode_count
    name: check_after
    opcodes: [dcl_sampler]
)YAML";

  auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, "inline-sm5-sampler-decl-test");
  if (!parse_result) {
    std::cerr << "Failed to parse inline SM5 sampler recipe: " << parse_result.error() << "\n";
    return 1;
  }

  const auto patch_result = parse_result.value().Execute(input_bytes);
  if (!patch_result) {
    std::cerr << "Failed to patch SM5 shader with sampler declaration recipe: " << patch_result.error() << "\n";
    return 1;
  }

  const auto& report = patch_result.value();

  if (report.steps.empty()) {
    std::cerr << "Expected sampler-add step to report one side effect.\n";
    return 1;
  }

  auto binding_it = report.new_bindings.find("add_s11");
  constexpr uint32_t kExpectedBindPoint = 11u;
  if (binding_it == report.new_bindings.end() || binding_it->second.binding_class != dxp::BindingClass::Sampler || binding_it->second.register_index != kExpectedBindPoint || binding_it->second.space != 0u) {
    std::cerr << "Expected sampler-add side effect to describe added s11.\n";
    return 1;
  }

  const auto* before_results = std::get_if<dxp::CheckOpcodeCountResults>(&report.steps[0].results);
  const auto* after_results = std::get_if<dxp::CheckOpcodeCountResults>(&report.steps[2].results);
  if ((before_results == nullptr) || (after_results == nullptr)) {
    std::cerr << "Expected check_opcode_count results.\n";
    return 1;
  }

  auto before = before_results->opcode_counts.find("dcl_sampler");
  const int before_dcl_sampler = before != before_results->opcode_counts.end() ? before->second : 0;
  auto after = after_results->opcode_counts.find("dcl_sampler");
  const int after_dcl_sampler = after != after_results->opcode_counts.end() ? after->second : 0;

  if (after_dcl_sampler != before_dcl_sampler + 1) {
    std::cerr << "Expected one additional DCL_SAMPLER declaration.\n";
    return 1;
  }

  std::cout << "SM5 recipe added comparison sampler declaration s11.\n";
  return 0;
}
