#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#include "dxp/Logging.hpp"
#include "dxp/PatchOptions.hpp"
#include "dxp/sm5/Recipe.hpp"
#include "dxp/StepResults.hpp"
#include "tests/helper/TestHelper.hpp"

namespace {

const char* kTolerantRecipe = R"YAML(version: 1
steps:
  - kind: add_resource
    name: add_mixed
    required: false
    textures:
      - handle: bad_tex
        register_index: 999
      - handle: auto_tex
  - kind: apply_rule
    name: probe_mul
    required: true
    rewrite_mode: none
    rule:
      match:
        - opcode: mul
)YAML";

const char* kStrictRecipe = R"YAML(version: 1
steps:
  - kind: add_resource
    name: add_bad
    textures:
      - handle: bad_tex
        register_index: 999
)YAML";

}  // namespace

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 2) {
    std::cerr << "Usage: sm5_recipe_add_resource_tolerance_test <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> input_bytes;
  if (!ReadFile(args[1], input_bytes)) {
    std::cerr << "Failed to read input shader: " << args[1] << "\n";
    return 1;
  }

  // Tolerant step: 999 overflows the texture bind-point maximum, is skipped
  // with a Warning; auto_tex is still added; the following rule still runs.
  {
    auto parse_result = dxp::sm5::Recipe::ParseFromText(kTolerantRecipe, "inline-sm5-tolerance-test");
    if (!parse_result) {
      std::cerr << "Failed to parse tolerant recipe: " << parse_result.error() << "\n";
      return 1;
    }
    const dxp::sm5::Recipe& recipe = parse_result.value();

    std::vector<std::string> messages;
    dxp::PatchOptions options;
    options.log_level = dxp::LogLevel::Warning;
    options.logger = [&](dxp::LogLevel, const std::string& message) { messages.push_back(message); };

    auto result = recipe.Execute(input_bytes, options);
    if (!result) {
      std::cerr << "Tolerant recipe unexpectedly failed: " << result.error() << "\n";
      return 1;
    }
    const auto& report = result.value();

    if (report.steps.size() != 2) {
      std::cerr << "Tolerant: expected both steps to run, got " << report.steps.size() << "\n";
      return 1;
    }
    const auto* add_res = std::get_if<dxp::AddResourceResults>(&report.steps[0].results);
    if (add_res == nullptr || add_res->textures_added != 1) {
      std::cerr << "Tolerant: expected exactly 1 texture added (auto_tex), got "
                << (add_res != nullptr ? add_res->textures_added : 0) << "\n";
      return 1;
    }
    if (report.steps[1].name != "probe_mul" || !report.steps[1].success) {
      std::cerr << "Tolerant: probe_mul must have run and matched\n";
      return 1;
    }

    bool saw_warning = false;
    for (const auto& message : messages) {
      if (message.find("add_resource: register_index 999 exceeds maximum 127 for 'texture'") != std::string::npos && message.find("skipped (required: false)") != std::string::npos) {
        saw_warning = true;
      }
    }
    if (!saw_warning) {
      std::cerr << "Tolerant: expected a Warning for the skipped declaration\n";
      return 1;
    }
  }

  // Strict step (default required: true): the same failure is a hard error.
  {
    auto parse_result = dxp::sm5::Recipe::ParseFromText(kStrictRecipe, "inline-sm5-strict-test");
    if (!parse_result) {
      std::cerr << "Failed to parse strict recipe: " << parse_result.error() << "\n";
      return 1;
    }
    const dxp::sm5::Recipe& recipe = parse_result.value();

    auto result = recipe.Execute(input_bytes);
    if (result) {
      std::cerr << "Strict: over-limit register_index unexpectedly succeeded\n";
      return 1;
    }
    if (result.error().find("exceeds maximum") == std::string::npos) {
      std::cerr << "Strict: unexpected error message: " << result.error() << "\n";
      return 1;
    }
  }

  std::cout << "sm5_recipe_add_resource_tolerance_test passed (warn+skip vs hard error).\n";
  std::cout.flush();
  return 0;
}
