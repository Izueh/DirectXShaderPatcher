// Stop-fast semantics test for dxp::sm5::Recipe.
//
// `required` is a stop-fast mechanism, not an error mechanism: a required step
// that finds no match ends the recipe run, but Execute succeeds with an
// unmodified pass-through output. Non-required no-matches publish state=false
// (so dependent steps skip) and execution continues.

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

const char* kStopFastRecipe = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: must_match
    required: true
    rewrite_mode: none
    rule:
      match:
        - opcode: dcl_thread_group
  - kind: apply_rule
    name: would_match
    required: true
    rewrite_mode: none
    rule:
      match:
        - opcode: mul
)YAML";

const char* kContinueRecipe = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: soft_probe
    required: false
    rewrite_mode: none
    rule:
      match:
        - opcode: dcl_thread_group
  - kind: apply_rule
    name: would_match
    required: true
    rewrite_mode: none
    rule:
      match:
        - opcode: mul
)YAML";

struct Capture {
  std::vector<std::string> messages;
  void Clear() { messages.clear(); }
};

}  // namespace

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 2) {
    std::cerr << "Usage: sm5_recipe_required_no_match_test <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> input_bytes;
  if (!ReadFile(args[1], input_bytes)) {
    std::cerr << "Failed to read input shader: " << args[1] << "\n";
    return 1;
  }

  // dcl_thread_group is compute-only; a pixel shader can never contain it, so
  // these probes deterministically fail to match.
  {
    auto parse_result = dxp::sm5::Recipe::ParseFromText(kStopFastRecipe, "inline-sm5-stop-fast-test");
    if (!parse_result) {
      std::cerr << "Failed to parse stop-fast recipe: " << parse_result.error() << "\n";
      return 1;
    }
    const dxp::sm5::Recipe& recipe = parse_result.value();

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

    // Only the first (required, no-match) step ran; the matching rule never ran.
    if (report.steps.size() != 1 || report.steps[0].name != "must_match" || report.steps[0].success) {
      std::cerr << "Stop-fast: expected exactly one failed 'must_match' step, got "
                << report.steps.size() << " step(s)\n";
      return 1;
    }
    if (report.modified || report.output_bytes != input_bytes) {
      std::cerr << "Stop-fast: output must be an unmodified pass-through\n";
      return 1;
    }

    bool saw_no_match = false;
    bool saw_stopping = false;
    bool saw_summary_note = false;
    for (const auto& message : capture.messages) {
      if (message.find("[apply_rule] must_match: no match — nothing applied") != std::string::npos) saw_no_match = true;
      if (message.find("[apply_rule] must_match: stopping early (required)") != std::string::npos) saw_stopping = true;
      if (message.find("recipe succeeded") != std::string::npos && message.find("stopped early at 'must_match' — required") != std::string::npos) {
        saw_summary_note = true;
      }
    }
    if (!saw_no_match || !saw_stopping || !saw_summary_note) {
      std::cerr << "Stop-fast: missing Info lines (no_match=" << saw_no_match
                << ", stopping=" << saw_stopping << ", summary_note=" << saw_summary_note << ")\n";
      return 1;
    }
  }

  // Non-required no-match: state=false, execution continues, later steps run.
  {
    auto parse_result = dxp::sm5::Recipe::ParseFromText(kContinueRecipe, "inline-sm5-continue-test");
    if (!parse_result) {
      std::cerr << "Failed to parse continue recipe: " << parse_result.error() << "\n";
      return 1;
    }
    const dxp::sm5::Recipe& recipe = parse_result.value();

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
    if (report.steps[0].name != "soft_probe" || report.steps[0].success) {
      std::cerr << "Continue: soft_probe must be reported as failed (no match)\n";
      return 1;
    }
    if (report.steps[1].name != "would_match" || !report.steps[1].success) {
      std::cerr << "Continue: would_match must have run and matched\n";
      return 1;
    }
  }

  std::cout << "sm5_recipe_required_no_match_test passed (stop-fast + continue).\n";
  std::cout.flush();
  return 0;
}
