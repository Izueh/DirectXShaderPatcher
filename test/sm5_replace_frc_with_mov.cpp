#include "TestSupport.h"
#include "dxp/sm5/Patch.h"
#include "dxp/sm5/RecipeParse.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace {

static bool WriteFile(const std::string &path,
                      const std::vector<uint8_t> &bytes) {
  const std::filesystem::path outputPath(path);
  const std::filesystem::path parentPath = outputPath.parent_path();
  if (!parentPath.empty()) {
    std::error_code error;
    if (!std::filesystem::create_directories(parentPath, error) && error) {
      return false;
    }
  }

  std::ofstream out(path, std::ios::binary);
  if (!out) {
    return false;
  }

  if (!bytes.empty()) {
    out.write(reinterpret_cast<const char *>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
  }

  return static_cast<bool>(out);
}

static std::string DefaultOutPath(const std::string &inPath) {
  return DefaultArtifactOutputPath(inPath, ".sm5.mov.patched.cso");
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2 && argc != 3) {
    std::cerr
        << "Usage: sm5_replace_frc_with_mov <input.ps_5_0.cso> [output.cso]\n";
    return 1;
  }

  const std::string inputPath = argv[1];
  const std::string outputPath =
      argc == 3 ? argv[2] : DefaultOutPath(inputPath);

  std::vector<uint8_t> inputBytes;
  if (!ReadFile(inputPath, inputBytes)) {
    std::cerr << "Failed to read input file: " << inputPath << "\n";
    return 1;
  }

  const std::filesystem::path recipePath =
      RepoRootPath() / "test" / "sm5_frc_to_mov.recipe.yml";
  dxp::sm5::RecipeParseResult parseResult;
  if (!dxp::sm5::ParseRecipeFile(recipePath.string(), parseResult)) {
    std::cerr << "Failed to parse SM5 recipe file: " << parseResult.Error
              << "\n";
    return 1;
  }

  const auto patchResult =
      dxp::sm5::PatchContainer(inputBytes, parseResult.Recipe);
  if (!patchResult.Success) {
    std::cerr << "Failed to patch SM5 shader: " << patchResult.Error << "\n";
    return 1;
  }

  if (patchResult.OutputBytes.empty()) {
    std::cerr << "Patched SM5 output was unexpectedly empty.\n";
    return 1;
  }

  if (!WriteFile(outputPath, patchResult.OutputBytes)) {
    std::cerr << "Failed to write output file: " << outputPath << "\n";
    return 1;
  }

  std::cout << "Patched SM5 shader written to: " << outputPath << "\n";
  std::cout << "SM5 recipe patch succeeded. Total rule matches: "
            << patchResult.RecipeContext.TotalRuleMatches << "\n";
  return 0;
}
