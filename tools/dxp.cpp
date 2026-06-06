#include "dxp/sm5/Patch.h"
#include "dxp/sm5/Recipe.h"
#include "dxp/sm5/RecipeParse.h"
#include "dxp/sm6/Patch.h"
#include "dxp/sm6/Recipe.h"
#include "dxp/sm6/RecipeParse.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

namespace {

static void PrintUsage() {
  std::cerr << "Usage:\n"
            << "  dxp sm5 patch <recipe.recipe.yml> <input.cso> [output.cso] [--trace]\n"
            << "  dxp sm5 validate <recipe.recipe.yml>\n"
            << "  dxp sm6 patch <recipe.recipe.yml> <input.cso> [output.cso] [--trace]\n"
            << "  dxp sm6 validate <recipe.recipe.yml>\n"
            << "Recipe files are YAML documents.\n"
            << "If output is omitted, defaults to <input>.patched.<ext>.\n";
}

static bool ReadBinaryFile(const std::string &path,
                           std::vector<uint8_t> &data) {
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

static int RunValidateSm6Command(const char *recipePath) {
  DxilRecipeParseResult parseResult;
  if (!ParseDxilRecipeFile(recipePath, parseResult)) {
    std::cerr << "SM6 recipe validation failed: " << parseResult.error << "\n";
    return 1;
  }

  std::cout << "SM6 recipe is valid: " << recipePath << " ("
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

static int RunPatchSm6Command(const char *inputPath, const char *recipePath,
                              const char *outputPath, bool traceEnabled) {
  std::vector<uint8_t> inputShader;
  if (!ReadBinaryFile(inputPath, inputShader)) {
    std::cerr << "Failed to read input shader: " << inputPath << "\n";
    return 1;
  }

  DxilRecipeParseResult parseResult;
  if (!ParseDxilRecipeFile(recipePath, parseResult)) {
    std::cerr << "Failed to parse SM6 recipe file: " << parseResult.error << "\n";
    return 1;
  }

  parseResult.patchOptions.recipeExecutionOptions.traceEnabled = traceEnabled;

  DxilRecipeContext recipeContext;
  std::vector<uint8_t> outputShader;
  if (!PatchDxilContainer(parseResult.recipe, inputShader, outputShader,
                          parseResult.patchOptions, &recipeContext)) {
    std::cerr << "SM6 patch operation failed.";
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

  std::cout << "Patched SM6 shader written to: " << outputPath << "\n";
  return 0;
}

static int RunPatchSm5Command(const char *inputPath, const char *recipePath,
                              const char *outputPath, bool traceEnabled) {
  std::vector<uint8_t> inputShader;
  if (!ReadBinaryFile(inputPath, inputShader)) {
    std::cerr << "Failed to read input shader: " << inputPath << "\n";
    return 1;
  }

  dxp::sm5::RecipeParseResult parseResult;
  if (!dxp::sm5::ParseRecipeFile(recipePath, parseResult)) {
    std::cerr << "Failed to parse SM5 recipe file: " << parseResult.Error
              << "\n";
    return 1;
  }

  dxp::sm5::RecipeContext recipeContext;
  recipeContext.TraceEnabled = traceEnabled;

  const auto patchResult =
      dxp::sm5::PatchContainer(inputShader, parseResult.Recipe, recipeContext);
  if (!patchResult.Success) {
    std::cerr << "PatchContainer failed";
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

  const std::string first = argv[1];
  if (first != "sm5" && first != "sm6") {
    std::cerr << "Error: backend must be specified as 'sm5' or 'sm6'.\n";
    PrintUsage();
    return 1;
  }

  const std::string backend = first;
  const std::string cmd = (argc >= 3) ? argv[2] : std::string();

  int exitCode = 1;

  if (cmd == "validate") {
    if (argc != 4) {
      PrintUsage();
      return 1;
    }
    exitCode = (backend == "sm5") ? RunValidateSm5Command(argv[3])
                    : RunValidateSm6Command(argv[3]);
    if (exitCode == 0) {
      std::cout.flush();
      std::cerr.flush();
    }
    return exitCode;
  }

  if (cmd == "patch") {
    if (argc < 5 || argc > 7) {
      PrintUsage();
      return 1;
    }

    const char *recipePath = argv[3];
    const char *inputPath = argv[4];

    // Determine output path and trace flag
    const char *outputPath = nullptr;
    bool traceEnabled = false;

    if (argc == 5) {
      // dxp <sm5|sm6> patch <recipe> <input>
      outputPath = nullptr;
      traceEnabled = false;
    } else if (argc == 6) {
      if (std::string(argv[5]) == "--trace") {
        traceEnabled = true;
      } else {
        outputPath = argv[5];
      }
    } else {
      // argc == 7
      if (std::string(argv[6]) == "--trace") {
        traceEnabled = true;
        outputPath = argv[5];
      } else {
        std::cerr << "Unknown option: " << argv[6] << "\n";
        return 1;
      }
    }

    // Derive default output path if not provided
    std::string resolvedOutputPath;
    if (!outputPath) {
      const std::filesystem::path inputP(inputPath);
      resolvedOutputPath =
          inputP.stem().string() + ".patched" + inputP.extension().string();
      outputPath = resolvedOutputPath.c_str();
    }

    exitCode = (backend == "sm5")
             ? RunPatchSm5Command(inputPath, recipePath, outputPath,
                       traceEnabled)
             : RunPatchSm6Command(inputPath, recipePath, outputPath,
                       traceEnabled);

    if (exitCode == 0) {
      std::cout.flush();
      std::cerr.flush();
    }
    return exitCode;
  }

  std::cerr << "Unknown command: " << cmd << "\n";
  PrintUsage();
  return 1;
}