#include "TestSupport.h"
#include "dxp/sm5/Patch.h"
#include "dxp/sm5/Recipe.h"

#include <iostream>
#include <vector>

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: sm5_container_roundtrip <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> inputBytes;
  if (!ReadFile(argv[1], inputBytes)) {
    std::cerr << "Failed to read file: " << argv[1] << "\n";
    return 1;
  }

  const dxp::sm5::Recipe recipe;
  const auto patchResult = dxp::sm5::PatchContainer(inputBytes, recipe);
  if (!patchResult.Success) {
    std::cerr << "SM5 patch round-trip failed: " << patchResult.Error << "\n";
    return 1;
  }

  if (patchResult.OutputBytes.empty()) {
    std::cerr << "Round-trip output was unexpectedly empty.\n";
    return 1;
  }

  std::cout << "SM5 round-trip succeeded. Input bytes: " << inputBytes.size()
            << ", output bytes: " << patchResult.OutputBytes.size() << "\n";
  return 0;
}
