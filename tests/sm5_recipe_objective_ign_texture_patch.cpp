#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#include "dxp/sm5/Recipe.hpp"
#include "tests/helper/TestHelper.hpp"

namespace {

bool WriteFile(const std::string& path, const std::vector<uint8_t>& bytes) {
  const std::filesystem::path output_path(path);
  const std::filesystem::path parent_path = output_path.parent_path();
  if (!std::filesystem::exists(parent_path)) {
    std::filesystem::create_directories(parent_path);
  }
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    std::cerr << "Failed to open output file: " << path << "\n";
    return false;
  }
  out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  return static_cast<bool>(out);
}

}  // namespace

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 3 && argc != 4) {
    std::cerr << "Usage: sm5_recipe_objective_ign_texture_patch <input.ps_5_0.cso> "
                 "<recipe.yml> [output.cso]\n";
    return 1;
  }

  std::vector<uint8_t> input_bytes;
  if (!ReadFile(args[1], input_bytes)) {
    std::cerr << "Failed to read input file: " << args[1] << "\n";
    return 1;
  }

  auto parse_result = dxp::sm5::Recipe::ParseFromFile(args[2]);
  if (!parse_result) {
    std::cerr << "Failed to parse SM5 recipe file: " << parse_result.error() << "\n";
    return 1;
  }

  const auto patch_result = parse_result.value().Execute(input_bytes);
  if (!patch_result) {
    std::cerr << "Failed to patch SM5 shader: " << patch_result.error() << "\n";
    return 1;
  }

  const auto& report = patch_result.value();

  // Verify recipe report structure
  if (report.output_container.format != "DXBC") {
    std::cerr << "Expected SM5 patch report to identify DXBC output format.\n";
    return 1;
  }

  if (report.steps.empty()) {
    std::cerr << "Expected SM5 patch report to record at least one step.\n";
    return 1;
  }

  if (report.output_container.total_size_in_bytes != report.output_bytes.size()) {
    std::cerr << "Expected SM5 patch report to expose final DXBC container size.\n";
    return 1;
  }

  constexpr size_t kHexHashLength = 32;
  if (report.output_container.hash_hex.size() != kHexHashLength) {
    std::cerr << "Expected SM5 patch report to expose a 32-character DXBC hash.\n";
    return 1;
  }

  if (report.output_container.chunks.empty()) {
    std::cerr << "Expected SM5 patch report to enumerate DXBC chunks.\n";
    return 1;
  }

  bool found_shader_chunk = false;
  for (const auto& chunk : report.output_container.chunks) {
    if (chunk.size_in_bytes == 0) {
      std::cerr << "Expected SM5 patch report chunk sizes to be populated.\n";
      return 1;
    }
    if (chunk.id == "SHDR" || chunk.id == "SHEX") {
      found_shader_chunk = true;
    }
  }
  if (!found_shader_chunk) {
    std::cerr << "Expected SM5 patch report to include the shader chunk.\n";
    return 1;
  }

  if (argc == 4) {
    if (!WriteFile(args[3], report.output_bytes)) {
      return 1;
    }
  }

  std::cout << "SM5 recipe objective ign texture patch test passed.\n";
  return 0;
}
