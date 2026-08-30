#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

#include "dxp/ExportTypes.hpp"
#include "dxp/PatchOptions.hpp"
#include "dxp/sm5/Recipe.hpp"
#include "tests/helper/TestHelper.hpp"

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 2) {
    std::cerr << "Usage: sm5_recipe_condition_missing_operand <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> input_bytes;
  if (!ReadFile(args[1], input_bytes)) {
    std::cerr << "Failed to read input file: " << args[1] << "\n";
    return 1;
  }

  // Both comparison operands reference never-set variables. Missing operands must
  // make the comparison FALSE (missing state/variable is treated as false), so the
  // gated step must NOT run. Regression: monostate==monostate previously compared
  // as equal, so this gate would have passed and added the sampler.
  const char* recipe_text = R"YAML(version: 1
steps:
  - kind: add_resource
    name: gated_add_s11
    condition:
      eq:
        lhs: never_set_a
        rhs: never_set_b
    samplers:
      - handle: gated_s11
        register_index: 11
        mode: comparison
)YAML";

  // --- Case 1: operands missing -> gate must be false, step skipped. ---
  {
    auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, "inline-sm5-missing-condition-test");
    if (!parse_result) {
      std::cerr << "Failed to parse inline SM5 recipe: " << parse_result.error() << "\n";
      return 1;
    }

    const auto patch_result = parse_result.value().Execute(input_bytes);
    if (!patch_result) {
      std::cerr << "Failed to patch SM5 shader: " << patch_result.error() << "\n";
      return 1;
    }

    const auto& report = patch_result.value();
    if (report.new_bindings.contains("gated_s11")) {
      std::cerr << "FAIL: gated add_resource ran even though both condition operands were missing.\n";
      return 1;
    }
    std::cout << "OK: missing comparison operands gate the step off (no binding created).\n";
  }

  // --- Case 2: operands resolve to equal values -> gate must be true, step runs. ---
  {
    auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, "inline-sm5-present-condition-test");
    if (!parse_result) {
      std::cerr << "Failed to parse inline SM5 recipe: " << parse_result.error() << "\n";
      return 1;
    }

    constexpr int64_t kEqualEnvValue = 7;
    dxp::PatchOptions options;
    options.SetEnv("never_set_a", kEqualEnvValue);
    options.SetEnv("never_set_b", kEqualEnvValue);
    parse_result.value().SetEnv(options);

    const auto patch_result = parse_result.value().Execute(input_bytes);
    if (!patch_result) {
      std::cerr << "Failed to patch SM5 shader: " << patch_result.error() << "\n";
      return 1;
    }

    const auto& report = patch_result.value();
    auto binding_it = report.new_bindings.find("gated_s11");
    constexpr uint32_t kExpectedBindPoint = 11u;
    if (binding_it == report.new_bindings.end() || binding_it->second.binding_class != dxp::BindingClass::Sampler || binding_it->second.register_index != kExpectedBindPoint) {
      std::cerr << "FAIL: gated add_resource did not run when both operands resolved to equal values.\n";
      return 1;
    }
    std::cout << "OK: resolved equal operands pass the gate (s11 binding created).\n";
  }

  std::cout << "SM5 condition missing-operand semantics verified.\n";
  return 0;
}
