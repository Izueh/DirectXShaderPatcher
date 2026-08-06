#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#include "dxp/Logging.hpp"
#include "dxp/PatchOptions.hpp"
#include "dxp/sm6/Recipe.hpp"
#include "dxp/StepResults.hpp"
#include "tests/helper/TestHelper.hpp"

namespace {

// 0x56C468C3.cs_6_6 is SM6.6; expecting SM5.1 deterministically mismatches.
const char* kStopFastRecipe = R"YAML(
steps:
  - kind: check_shader_version
    name: version_gate
    major: 5
    minor: 1
  - kind: apply_rule
    name: would_match
    required: true
    rewrite_mode: none
    rule:
      match:
        - opcode: TextureLoad
)YAML";

const char* kContinueRecipe = R"YAML(
steps:
  - kind: check_shader_version
    name: version_gate
    required: false
    major: 5
    minor: 1
  - kind: apply_rule
    name: would_match
    required: true
    rewrite_mode: none
    rule:
      match:
        - opcode: TextureLoad
)YAML";

struct Capture {
  std::vector<std::string> messages;
  void Clear() { messages.clear(); }
};

}  // namespace

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 2) {
    std::cerr << "Usage: sm6_recipe_required_no_match_test <input.cs_6_6.cso>\n";
    return 1;
  }

  const ScopedCoInitialize coinit;

  std::vector<uint8_t> input_bytes;
  if (!ReadFile(args[1], input_bytes)) {
    std::cerr << "Failed to read input shader: " << args[1] << "\n";
    return 1;
  }

  {
    auto parse_result = dxp::sm6::Recipe::ParseFromText(kStopFastRecipe, "inline-sm6-stop-fast-test");
    if (!parse_result) {
      std::cerr << "Failed to parse stop-fast recipe: " << parse_result.error() << "\n";
      return 1;
    }
    const dxp::sm6::Recipe& recipe = parse_result.value();

    Capture capture;
    dxp::PatchOptions options;
    options.log_level = dxp::LogLevel::Trace;
    options.logger = [&](dxp::LogLevel, const std::string& message) { capture.messages.push_back(message); };

    auto result = recipe.Execute(input_bytes, options);
    if (!result) {
      std::cerr << "Stop-fast recipe unexpectedly failed: " << result.error() << "\n";
      return 1;
    }
    const auto& report = result.value();

    // Only the version gate ran (mismatch, required -> stop-fast); the rule
    // that would match never ran.
    if (report.steps.size() != 1 || report.steps[0].name != "version_gate" || report.steps[0].success) {
      std::cerr << "Stop-fast: expected exactly one failed 'version_gate' step, got "
                << report.steps.size() << " step(s)\n";
      return 1;
    }
    if (report.modified || report.output_bytes != input_bytes) {
      std::cerr << "Stop-fast: output must be an unmodified pass-through\n";
      return 1;
    }

    bool saw_no_match = false;
    bool saw_stopping = false;
    for (const auto& message : capture.messages) {
      if (message.find("[check_shader_version] version_gate: expected SM5.1, got SM6.6 — no match") != std::string::npos) {
        saw_no_match = true;
      }
      if (message.find("[check_shader_version] version_gate: stopping early (required)") != std::string::npos) {
        saw_stopping = true;
      }
    }
    if (!saw_no_match || !saw_stopping) {
      std::cerr << "Stop-fast: missing Info lines (no_match=" << saw_no_match
                << ", stopping=" << saw_stopping << ")\n";
      return 1;
    }
  }

  // Non-required version mismatch: state=false, execution continues.
  {
    auto parse_result = dxp::sm6::Recipe::ParseFromText(kContinueRecipe, "inline-sm6-continue-test");
    if (!parse_result) {
      std::cerr << "Failed to parse continue recipe: " << parse_result.error() << "\n";
      return 1;
    }
    const dxp::sm6::Recipe& recipe = parse_result.value();

    auto result = recipe.Execute(input_bytes);
    if (!result) {
      std::cerr << "Continue recipe unexpectedly failed: " << result.error() << "\n";
      return 1;
    }
    const auto& report = result.value();
    if (report.steps.size() != 2) {
      std::cerr << "Continue: expected both steps to run, got " << report.steps.size() << "\n";
      return 1;
    }
    if (report.steps[0].name != "version_gate" || report.steps[0].success) {
      std::cerr << "Continue: version_gate must be reported as failed (no match)\n";
      return 1;
    }
    if (report.steps[1].name != "would_match" || !report.steps[1].success) {
      std::cerr << "Continue: would_match must have run and matched\n";
      return 1;
    }
  }

  std::cout << "sm6_recipe_required_no_match_test passed (stop-fast + continue).\n";
  std::cout.flush();
  return 0;
}
