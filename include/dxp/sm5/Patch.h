#pragma once

#include "Recipe.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace dxp::sm5 {

struct PatchResult {
  bool Success = false;
  std::vector<uint8_t> OutputBytes;
  std::string Error;
  RecipeContext RecipeContext;
};

PatchResult PatchContainerInMemory(const std::vector<uint8_t> &inputContainer,
                                   const Recipe &recipe,
                                   const RecipeContext &context = {});

PatchResult PatchContainerInMemory(const Recipe &recipe,
                                   const uint8_t *inputData,
                                   size_t inputSize,
                                   const RecipeContext &context = {});

} // namespace dxp::sm5
