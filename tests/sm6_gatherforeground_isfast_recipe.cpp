#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#include "dxp/sm6/Recipe.hpp"
#include "tests/helper/TestHelper.hpp"

namespace {

std::string BuildDefaultPatchedOutputPath(const std::string& input_path) {
  return DefaultArtifactOutputPath(input_path, ".recipe.isfast.patched.cso");
}

}  // namespace

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 2 && argc != 3) {
    std::cerr << "Usage: sm6_gatherforeground_isfast_recipe <input.cso> [output.cso]\n"
              << "If [output.cso] is omitted, the test writes next to the test binary\n"
              << "in the per-config build directory (DXP_TEST_OUTPUT_DIR).\n";
    return 1;
  }

  const std::string output_path = argc == 3 ? std::string(args[2]) : BuildDefaultPatchedOutputPath(args[1]);

  const ScopedCoInitialize coinit;

  // Read shader bytes using public API (no ShaderProgram)
  std::vector<uint8_t> input_bytes;
  if (!ReadFile(args[1], input_bytes)) {
    std::cerr << "Failed to read input shader: " << args[1] << "\n";
    return 1;
  }

  // Execute recipe using declarative YAML with add_resource step
  const char* yaml = R"(
steps:
  - kind: add_resource
    name: add_resources
    textures:
      - handle: fast_noise
        kind: Texture2DArray
        space: 50
        element_type: F32
        vector_width: 2
    cbuffers:
      - handle: frame_constants
        space: 0
        size: 16
        type: ISFastFrameConstants
        fields:
          - name: FrameIndex
            type: U32
            width: 1
            offset: 0
)";

  auto parse_result = dxp::sm6::Recipe::ParseFromText(yaml);
  if (!parse_result) {
    std::cerr << "Failed to parse recipe: " << parse_result.error() << "\n";
    return 1;
  }

  auto result = parse_result.value().Execute(input_bytes);
  if (!result) {
    std::cerr << "Recipe execution failed: " << result.error() << "\n";
    return 1;
  }

  // Verify serialization produced valid output
  if (result.value().output_bytes.empty()) {
    std::cerr << "Serialization produced empty output.\n";
    return 1;
  }

  // Verify new_bindings contains the added resources
  bool found_fast_noise = false;
  bool found_frame_constants = false;

  for (const auto& [handle, binding] : result.value().new_bindings) {
    if (handle == "fast_noise") {
      found_fast_noise = true;
    }
    if (handle == "frame_constants") {
      found_frame_constants = true;
    }
  }

  if (!found_fast_noise) {
    std::cerr << "Expected 'fast_noise' in new_bindings.\n";
    return 1;
  }

  if (!found_frame_constants) {
    std::cerr << "Expected 'frame_constants' in new_bindings.\n";
    return 1;
  }

  // Write output if requested
  if (argc == 3) {
    if (!WriteFile(output_path, result.value().output_bytes.data(), result.value().output_bytes.size())) {
      std::cerr << "Failed to write output: " << output_path << "\n";
      return 1;
    }
  }

  std::cout << "Recipe executed successfully, added fast_noise and frame_constants.\n";
  return 0;
}
