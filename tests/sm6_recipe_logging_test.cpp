#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#include "dxp/Logging.hpp"
#include "dxp/PatchOptions.hpp"
#include "dxp/sm6/Recipe.hpp"
#include "tests/helper/TestHelper.hpp"

namespace {

const char* kRecipeText = R"YAML(
steps:
  - kind: apply_rule
    name: identity_pass
    required: false
    rewrite_mode: none
    rule:
        prune: true
        match:
          - opcode: TextureLoad
            capture: texture_load
            operands:
              - index: 1
                capture: texture_handle
              - index: 3
                capture: coord_x
        emit: []
    match_mode: first
)YAML";

struct Capture {
  std::vector<std::string> messages;
  void Clear() { messages.clear(); }
};

}  // namespace

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 2) {
    std::cerr << "Usage: sm6_recipe_logging_test <input.cs_6_6.cso>\n";
    return 1;
  }

  const ScopedCoInitialize coinit;

  std::vector<uint8_t> input_bytes;
  if (!ReadFile(args[1], input_bytes)) {
    std::cerr << "Failed to read input shader: " << args[1] << "\n";
    return 1;
  }

  auto parse_result = dxp::sm6::Recipe::ParseFromText(kRecipeText, "inline-sm6-logging-test");
  if (!parse_result) {
    std::cerr << "Failed to parse inline SM6 recipe: " << parse_result.error() << "\n";
    return 1;
  }
  const dxp::sm6::Recipe& recipe = parse_result.value();
  Capture capture;

  auto make_options = [&](dxp::LogLevel level) {
    dxp::PatchOptions options;
    options.log_level = level;
    options.logger = [&](dxp::LogLevel, const std::string& message) { capture.messages.push_back(message); };
    return options;
  };

  // Test 1 — Info level: recipe-level summary only.
  {
    capture.Clear();
    auto result = recipe.Execute(input_bytes, make_options(dxp::LogLevel::Info));
    if (!result) {
      std::cerr << "Test 1 execution failed: " << result.error() << "\n";
      return 1;
    }
    if (capture.messages.empty()) {
      std::cerr << "Test 1: expected log messages at Info level, got none\n";
      return 1;
    }
    bool saw_summary = false;
    for (const auto& message : capture.messages) {
      if (message.find("recipe succeeded: 1 steps") != std::string::npos) saw_summary = true;
      if (message.find("[apply_rule]") != std::string::npos) {
        std::cerr << "Test 1: per-step line leaked at Info level: " << message << "\n";
        return 1;
      }
      if (message.find("recipe execution started") != std::string::npos) {
        std::cerr << "Test 1: lifecycle line leaked at Info level: " << message << "\n";
        return 1;
      }
      if (message.find(": starting") != std::string::npos) {
        std::cerr << "Test 1: Trace line leaked at Info level: " << message << "\n";
        return 1;
      }
    }
    if (!saw_summary) {
      std::cerr << "Test 1: missing recipe summary at Info\n";
      return 1;
    }
    const std::string& summary = capture.messages.back();
    if (summary.find("input hash 0x") == std::string::npos || summary.find("output hash 0x") == std::string::npos) {
      std::cerr << "Test 1: summary must carry input+output hashes: " << summary << "\n";
      return 1;
    }
  }

  // Test 2 — Debug gating: lifecycle lines only at Debug level and above.
  {
    capture.Clear();
    auto info_result = recipe.Execute(input_bytes, make_options(dxp::LogLevel::Info));
    if (!info_result) {
      std::cerr << "Test 2a execution failed: " << info_result.error() << "\n";
      return 1;
    }
    for (const auto& message : capture.messages) {
      if (message.find("recipe execution started") != std::string::npos) {
        std::cerr << "Test 2: Debug message leaked at Info level: " << message << "\n";
        return 1;
      }
    }

    capture.Clear();
    auto debug_result = recipe.Execute(input_bytes, make_options(dxp::LogLevel::Debug));
    if (!debug_result) {
      std::cerr << "Test 2b execution failed: " << debug_result.error() << "\n";
      return 1;
    }
    bool saw_lifecycle = false;
    for (const auto& message : capture.messages) {
      if (message.find("recipe execution started") != std::string::npos) saw_lifecycle = true;
      if (message.find("[apply_rule]") != std::string::npos) {
        std::cerr << "Test 2: per-step line leaked at Debug level: " << message << "\n";
        return 1;
      }
    }
    if (!saw_lifecycle) {
      std::cerr << "Test 2: expected Debug lifecycle line, got none\n";
      return 1;
    }
  }

  // Test 3 — Trace gating: per-step lines + pretty-printed results.
  {
    capture.Clear();
    auto trace_result = recipe.Execute(input_bytes, make_options(dxp::LogLevel::Trace));
    if (!trace_result) {
      std::cerr << "Test 3 execution failed: " << trace_result.error() << "\n";
      return 1;
    }
    bool saw_start = false;
    bool saw_completed = false;
    bool saw_outcome = false;
    bool saw_results_json = false;
    for (const auto& message : capture.messages) {
      if (message.find("[apply_rule] identity_pass: starting") != std::string::npos) saw_start = true;
      if (message.find("[apply_rule] identity_pass: completed") != std::string::npos) saw_completed = true;
      if (message.find("[apply_rule] identity_pass: matched") != std::string::npos) saw_outcome = true;
      if (message.find(" results:") != std::string::npos && message.find("\"match_count\"") != std::string::npos) {
        saw_results_json = true;
      }
    }
    if (!saw_start || !saw_completed || !saw_outcome || !saw_results_json) {
      std::cerr << "Test 3: missing Trace lines (start=" << saw_start << ", completed=" << saw_completed
                << ", outcome=" << saw_outcome << ", results_json=" << saw_results_json << ")\n";
      return 1;
    }
  }

  std::cout << "sm6_recipe_logging_test passed (logging taxonomy).\n";
  std::cout.flush();
  return 0;
}
