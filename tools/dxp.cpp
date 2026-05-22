#include "../include/DirectXShaderPatcher.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

static void PrintUsage() {
  std::cerr
  << "Usage: dxp patch <input.cso> <recipe.recipe.yml> <output.cso> [--trace]\n"
  << "       dxp patch-sm5 <input.cso> <recipe.recipe.yml> <output.cso> [--trace]\n"
      << "       dxp validate <recipe.recipe.yml>\n"
  << "       dxp validate-sm5 <recipe.recipe.yml>\n"
      << "Recipe files are YAML documents.\n";
}

static bool ReadBinaryFile(const std::string &path, std::vector<uint8_t> &data) {
  std::ifstream file(path, std::ios::binary);
  if (!file)
    return false;

  file.seekg(0, std::ios::end);
  const std::streamoff size = file.tellg();
  if (size < 0)
    return false;
  file.seekg(0, std::ios::beg);

  data.resize(static_cast<size_t>(size));
  if (size > 0)
    file.read(reinterpret_cast<char *>(data.data()), size);

  return !!file;
}

static bool WriteBinaryFile(const std::string &path,
                            const std::vector<uint8_t> &data) {
  const std::filesystem::path outputPath(path);
  const std::filesystem::path parentPath = outputPath.parent_path();
  if (!parentPath.empty()) {
    std::error_code error;
    if (!std::filesystem::create_directories(parentPath, error) && error)
      return false;
  }

  std::ofstream file(path, std::ios::binary);
  if (!file)
    return false;

  if (!data.empty()) {
    file.write(reinterpret_cast<const char *>(data.data()),
               static_cast<std::streamsize>(data.size()));
  }

  return !!file;
}

static int RunValidateDxilCommand(const char *recipePath) {
  DxilRecipeParseResult parseResult;
  if (!ParseDxilRecipeFile(recipePath, parseResult)) {
    std::cerr << "Recipe validation failed: " << parseResult.error << "\n";
    return 1;
  }

  std::cout << "Recipe is valid: " << recipePath << " ("
            << parseResult.recipe.GetSteps().size() << " step(s))\n";
  return 0;
}

static int RunValidateSm5Command(const char *recipePath) {
  dxp::sm5::RecipeParseResult parseResult;
  if (!dxp::sm5::ParseRecipeFile(recipePath, parseResult)) {
    std::cerr << "SM5 recipe validation failed: " << parseResult.Error << "\n";
    return 1;
  }

  std::cout << "SM5 recipe is valid: " << recipePath << " ("
            << parseResult.Recipe.GetSteps().size() << " step(s))\n";
  return 0;
}

static int RunPatchDxilCommand(const char *inputPath,
                               const char *recipePath,
                               const char *outputPath,
                               bool traceEnabled) {
  std::vector<uint8_t> inputShader;
  if (!ReadBinaryFile(inputPath, inputShader)) {
    std::cerr << "Failed to read input shader: " << inputPath << "\n";
    return 1;
  }

  DxilRecipeParseResult parseResult;
  if (!ParseDxilRecipeFile(recipePath, parseResult)) {
    std::cerr << "Failed to parse recipe file: " << parseResult.error << "\n";
    return 1;
  }

  parseResult.patchOptions.recipeExecutionOptions.traceEnabled = traceEnabled;

  DxilRecipeContext recipeContext;
  std::vector<uint8_t> outputShader;
  if (!PatchDxilContainerInMemory(parseResult.recipe,
                                  inputShader,
                                  outputShader,
                                  parseResult.patchOptions,
                                  &recipeContext)) {
    std::cerr << "PatchDxilContainerInMemory failed.";
    if (!recipeContext.lastError.empty())
      std::cerr << " " << recipeContext.lastError;
    std::cerr << "\n";
    for (const std::string &diagnostic : recipeContext.diagnostics)
      std::cerr << diagnostic << "\n";
    return 1;
  }

  if (!WriteBinaryFile(outputPath, outputShader)) {
    std::cerr << "Failed to write output shader: " << outputPath << "\n";
    return 1;
  }

  std::cout << "Patched shader written to: " << outputPath << "\n";
  return 0;
}

static int RunPatchSm5Command(const char *inputPath,
                              const char *recipePath,
                              const char *outputPath,
                              bool traceEnabled) {
  std::vector<uint8_t> inputShader;
  if (!ReadBinaryFile(inputPath, inputShader)) {
    std::cerr << "Failed to read input shader: " << inputPath << "\n";
    return 1;
  }

  dxp::sm5::RecipeParseResult parseResult;
  if (!dxp::sm5::ParseRecipeFile(recipePath, parseResult)) {
    std::cerr << "Failed to parse SM5 recipe file: " << parseResult.Error << "\n";
    return 1;
  }

  dxp::sm5::RecipeContext recipeContext;
  recipeContext.TraceEnabled = traceEnabled;

  const auto patchResult =
      dxp::sm5::PatchContainerInMemory(inputShader, parseResult.Recipe, recipeContext);
  if (!patchResult.Success) {
    std::cerr << "PatchContainerInMemory failed";
    if (!patchResult.Error.empty()) {
      std::cerr << ": " << patchResult.Error;
    }
    std::cerr << "\n";

    if (!patchResult.RecipeContext.LastError.empty() &&
        patchResult.RecipeContext.LastError != patchResult.Error) {
      std::cerr << patchResult.RecipeContext.LastError << "\n";
    }

    for (const std::string &diagnostic : patchResult.RecipeContext.Diagnostics)
      std::cerr << diagnostic << "\n";
    return 1;
  }

  if (!WriteBinaryFile(outputPath, patchResult.OutputBytes)) {
    std::cerr << "Failed to write output shader: " << outputPath << "\n";
    return 1;
  }

  std::cout << "Patched SM5 shader written to: " << outputPath << "\n";
  return 0;
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    PrintUsage();
    return 1;
  }

  const std::string command = argv[1];
  if (command == "validate") {
    if (argc != 3) {
      PrintUsage();
      return 1;
    }

    const int exitCode = RunValidateDxilCommand(argv[2]);
    if (exitCode == 0) {
      std::cout.flush();
      std::cerr.flush();
      std::_Exit(0);
    }
    return exitCode;
  }

  if (command == "validate-sm5") {
    if (argc != 3) {
      PrintUsage();
      return 1;
    }

    const int exitCode = RunValidateSm5Command(argv[2]);
    if (exitCode == 0) {
      std::cout.flush();
      std::cerr.flush();
      std::_Exit(0);
    }
    return exitCode;
  }

  if (command != "patch" && command != "patch-sm5") {
    std::cerr << "Unknown command: " << command << "\n";
    PrintUsage();
    return 1;
  }

  if (argc != 5 && argc != 6) {
    PrintUsage();
    return 1;
  }

  const bool traceEnabled = argc == 6 && std::string(argv[5]) == "--trace";
  if (argc == 6 && !traceEnabled) {
    std::cerr << "Unknown option: " << argv[5] << "\n";
    return 1;
  }

  const int exitCode =
      command == "patch"
          ? RunPatchDxilCommand(argv[2], argv[3], argv[4], traceEnabled)
          : RunPatchSm5Command(argv[2], argv[3], argv[4], traceEnabled);
  if (exitCode == 0) {
    std::cout.flush();
    std::cerr.flush();
    std::_Exit(0);
  }
  return exitCode;
}