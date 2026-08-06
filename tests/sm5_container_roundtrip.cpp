#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

#include "dxp/sm5/Recipe.hpp"
#include "tests/helper/TestHelper.hpp"

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 2) {
    std::cerr << "Usage: sm5_container_roundtrip <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> input_bytes;
  if (!ReadFile(args[1], input_bytes)) {
    std::cerr << "Failed to read file: " << args[1] << "\n";
    return 1;
  }

  const char* recipe_text = R"YAML(version: 1
steps:
  - kind: check_shader_version
    name: version_check
    major: 5
    minor: 0
)YAML";

  auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, "roundtrip");
  if (!parse_result) {
    std::cerr << "Failed to parse inline recipe: " << parse_result.error() << "\n";
    return 1;
  }

  const auto patch_result = parse_result.value().Execute(input_bytes);
  if (!patch_result) {
    std::cerr << "SM5 patch round-trip failed: " << patch_result.error() << "\n";
    return 1;
  }

  if (patch_result.value().output_bytes.empty()) {
    std::cerr << "Round-trip output was unexpectedly empty.\n";
    return 1;
  }

  std::cout << "SM5 round-trip succeeded. Input bytes: " << input_bytes.size()
            << ", output bytes: " << patch_result.value().output_bytes.size() << "\n";
  return 0;
}
