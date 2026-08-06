#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <vector>

#include "dxp/sm5/Recipe.hpp"
#include "tests/helper/TestHelper.hpp"

namespace {

static bool WriteFile(const std::string& path, const std::vector<uint8_t>& bytes) {
  const std::filesystem::path output_path(path);
  const std::filesystem::path parent_path = output_path.parent_path();
  if (!parent_path.empty()) {
    std::error_code error;
    if (!std::filesystem::create_directories(parent_path, error) && error) {
      return false;
    }
  }

  std::ofstream out(path, std::ios::binary);
  if (!out) {
    return false;
  }

  if (!bytes.empty()) {
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  }

  return static_cast<bool>(out);
}

static std::string DefaultOutPath(const std::string& in_path) {
  return DefaultArtifactOutputPath(in_path, ".sm5.mov.patched.cso");
}

}  // namespace

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 2 && argc != 3) {
    std::cerr << "Usage: sm5_replace_frc_with_mov <input.ps_5_0.cso> [output.cso]\n";
    return 1;
  }

  const std::string input_path = args[1];
  const std::string output_path = argc == 3 ? args[2] : DefaultOutPath(input_path);

  std::vector<uint8_t> input_bytes;
  if (!ReadFile(input_path, input_bytes)) {
    std::cerr << "Failed to read input file: " << input_path << "\n";
    return 1;
  }

  const std::filesystem::path recipe_path = RepoRootPath() / "test" / "recipes" / "sm5_frc_to_mov.recipe.yml";
  auto parse_result = dxp::sm5::Recipe::ParseFromFile(recipe_path.string());
  if (!parse_result) {
    std::cerr << "Failed to parse SM5 recipe file: " << parse_result.error() << "\n";
    return 1;
  }

  const auto patch_result = parse_result.value().Execute(input_bytes);
  if (!patch_result) {
    std::cerr << "Failed to patch SM5 shader: " << patch_result.error() << "\n";
    return 1;
  }

  if (patch_result.value().output_bytes.empty()) {
    std::cerr << "Patched SM5 output was unexpectedly empty.\n";
    return 1;
  }

  if (!WriteFile(output_path, patch_result.value().output_bytes)) {
    std::cerr << "Failed to write output file: " << output_path << "\n";
    return 1;
  }

  std::cout << "Patched SM5 shader written to: " << output_path << "\n";
  std::cout << "SM5 recipe patch succeeded. Total rule matches: "
            << "\n";
  return 0;
}
