#include "../DXIL Assembler/DxilAssemblerLib.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

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
  std::ofstream file(path, std::ios::binary);
  if (!file)
    return false;

  if (!data.empty()) {
    file.write(reinterpret_cast<const char *>(data.data()),
               static_cast<std::streamsize>(data.size()));
  }

  return !!file;
}

static std::string Trim(std::string value) {
  const size_t first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return std::string();

  const size_t last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 4 && argc != 5) {
    std::cerr << "Usage: dxil_patch_tool <input.cso> <recipe.recipe.yml> <output.cso> [--trace]\n"
              << "Recipe files are YAML documents.\n";
    return 1;
  }

  const bool traceEnabled = argc == 5 && std::string(argv[4]) == "--trace";
  if (argc == 5 && !traceEnabled) {
    std::cerr << "Unknown option: " << argv[4] << "\n";
    return 1;
  }

  std::vector<uint8_t> inputShader;
  if (!ReadBinaryFile(argv[1], inputShader)) {
    std::cerr << "Failed to read input shader: " << argv[1] << "\n";
    return 1;
  }

  DxilRecipeParseResult parseResult;
  if (!ParseDxilRecipeFile(argv[2], parseResult)) {
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

  if (!WriteBinaryFile(argv[3], outputShader)) {
    std::cerr << "Failed to write output shader: " << argv[3] << "\n";
    return 1;
  }

  std::cout << "Patched shader written to: " << argv[3] << "\n";
  std::cout.flush();
  std::cerr.flush();
  std::_Exit(0);
}