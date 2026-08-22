#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

#include "dxp/sm5/Recipe.hpp"
#include "tests/helper/StackTraceHelper.hpp"
#include "tests/helper/TestHelper.hpp"

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
#ifndef NDEBUG
  InstallCrashHandler();
#endif

  if (argc != 2) {
    std::cerr << "Usage: sm5_reverse_bind_test <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> input_bytes;
  if (!ReadFile(args[1], input_bytes)) {
    std::cerr << "Failed to read input file: " << args[1] << "\n";
    return 1;
  }

  // Forward auto-bind recipe — no reverse_bind, should take lowest free slot.
  const char* forward_recipe = R"YAML(version: 1
steps:
  - kind: add_resource
    name: add_fwd
    textures:
      - handle: my_texture
)YAML";

  // Reverse auto-bind recipe — reverse_bind: true, should take highest free slot.
  const char* reverse_recipe = R"YAML(version: 1
steps:
  - kind: add_resource
    name: add_rev
    textures:
      - handle: my_texture
        reverse_bind: true
)YAML";

  auto forward_parse = dxp::sm5::Recipe::ParseFromText(forward_recipe, "forward-test");
  if (!forward_parse) {
    std::cerr << "Failed to parse forward recipe: " << forward_parse.error() << "\n";
    return 1;
  }

  auto reverse_parse = dxp::sm5::Recipe::ParseFromText(reverse_recipe, "reverse-test");
  if (!reverse_parse) {
    std::cerr << "Failed to parse reverse recipe: " << reverse_parse.error() << "\n";
    return 1;
  }

  const auto forward_result = forward_parse.value().Execute(input_bytes);
  if (!forward_result) {
    std::cerr << "Failed to execute forward recipe: " << forward_result.error() << "\n";
    return 1;
  }

  const auto reverse_result = reverse_parse.value().Execute(input_bytes);
  if (!reverse_result) {
    std::cerr << "Failed to execute reverse recipe: " << reverse_result.error() << "\n";
    return 1;
  }

  const auto& fwd_report = forward_result.value();
  const auto& rev_report = reverse_result.value();

  auto fwd_it = fwd_report.new_bindings.find("my_texture");
  auto rev_it = rev_report.new_bindings.find("my_texture");

  if (fwd_it == fwd_report.new_bindings.end()) {
    std::cerr << "Forward recipe did not produce a 'my_texture' binding.\n";
    return 1;
  }
  if (rev_it == rev_report.new_bindings.end()) {
    std::cerr << "Reverse recipe did not produce a 'my_texture' binding.\n";
    return 1;
  }

  const uint32_t fwd_index = fwd_it->second.register_index;
  const uint32_t rev_index = rev_it->second.register_index;

  std::cout << "Forward auto-bind: t" << fwd_index << "\n";
  std::cout << "Reverse auto-bind: t" << rev_index << "\n";

  if (fwd_index == rev_index) {
    std::cerr << "ERROR: Forward and reverse auto-bind returned the same register index ("
              << fwd_index << "). Expected them to differ.\n";
    return 1;
  }

  if (fwd_index >= rev_index) {
    std::cerr << "ERROR: Forward index (" << fwd_index
              << ") should be less than reverse index (" << rev_index << ").\n";
    return 1;
  }

  std::cout << "SM5 reverse_bind test passed: forward=t" << fwd_index
            << ", reverse=t" << rev_index << "\n";
  return 0;
}
