#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#include "dxp/Logging.hpp"
#include "dxp/PatchOptions.hpp"
#include "dxp/sm5/Recipe.hpp"
#include "tests/helper/TestHelper.hpp"

namespace {

const char* kRecipeText = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: rewrite_with_runtime_vars
    condition:
      eq:
        lhs: enable_noise_patch
        rhs: true
    rule:
      match:
        - opcode: mul
          operands:
            - capture: dst
            - capture: src
      emit:
        - opcode: mov
          operands:
            - capture: dst
            - type: immediate32
              immediates_u32: [frame_seed]
)YAML";

dxp::sm5::Recipe LoadRecipe() {
  auto parse_result = dxp::sm5::Recipe::ParseFromText(kRecipeText, "inline-sm5-logging-test");
  if (!parse_result) {
    std::cerr << "Failed to parse inline SM5 recipe: " << parse_result.error() << "\n";
    std::exit(1);
  }
  return std::move(parse_result.value());
}

struct Capture {
  std::vector<std::string> messages;
  void Clear() { messages.clear(); }
};

/// @brief True when the message contains a hash in all-caps hex (no a-f).
bool ContainsUpperHexHash(const std::string& message, const char* prefix) {
  const size_t pos = message.find(prefix);
  if (pos == std::string::npos) return false;
  for (size_t p = pos + std::string(prefix).size(); p < message.size(); ++p) {
    const char c = message[p];
    if (c == ',' || c == ';' || c == ')') break;
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) return false;
  }
  return true;
}

}  // namespace

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 2) {
    std::cerr << "Usage: sm5_recipe_logging_test <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> input_bytes;
  if (!ReadFile(args[1], input_bytes)) {
    std::cerr << "Failed to read input shader: " << args[1] << "\n";
    return 1;
  }

  const dxp::sm5::Recipe recipe = LoadRecipe();
  Capture capture;

  auto make_options = [&](dxp::LogLevel level) {
    dxp::PatchOptions options;
    options.SetEnv("enable_noise_patch", true);
    options.SetEnv("frame_seed", 0x3F800000u);
    options.log_level = level;
    options.logger = [&](dxp::LogLevel, const std::string& message) { capture.messages.push_back(message); };
    return options;
  };

  // Test 1 — Info level: recipe-level summary only, with uppercase hashes.
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
    if (!ContainsUpperHexHash(summary, "input hash 0x") || !ContainsUpperHexHash(summary, "output hash 0x")) {
      std::cerr << "Test 1: summary must carry uppercase input+output hashes: " << summary << "\n";
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
      if (message.find(": starting") != std::string::npos) {
        std::cerr << "Test 2: Trace message leaked at Debug level: " << message << "\n";
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
      if (message.find("[apply_rule] rewrite_with_runtime_vars: starting") != std::string::npos) saw_start = true;
      if (message.find("[apply_rule] rewrite_with_runtime_vars: completed") != std::string::npos) saw_completed = true;
      if (message.find("[apply_rule] rewrite_with_runtime_vars: matched") != std::string::npos) saw_outcome = true;
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

  // Test 4 — error contract: a corrupt container yields a specific message
  // through Execute (the library writes nothing to global streams).
  {
    const std::vector<uint8_t> corrupt(16, 0x00);  // too small to be a DXBC container
    auto result = recipe.Execute(corrupt, make_options(dxp::LogLevel::Warning));
    if (result) {
      std::cerr << "Test 4: corrupt input unexpectedly succeeded\n";
      return 1;
    }
    const std::string& error = result.error();
    if (error.find("container too small") == std::string::npos) {
      std::cerr << "Test 4: expected a specific 'container too small' error, got: " << error << "\n";
      return 1;
    }
    // The failure summary must also reach the sink.
    bool saw_failed = false;
    for (const auto& message : capture.messages) {
      if (message.find("recipe failed:") != std::string::npos) saw_failed = true;
    }
    if (!saw_failed) {
      std::cerr << "Test 4: expected 'recipe failed' message on the sink\n";
      return 1;
    }
  }

  std::cout << "sm5_recipe_logging_test passed (logging taxonomy + error contract).\n";
  std::cout.flush();
  return 0;
}
