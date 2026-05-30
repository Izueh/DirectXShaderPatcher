#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

#include "Recipe.h"

/// @brief Owns the loaded LLVM and DXIL state for a shader container.
struct DxilLoadedShaderState {
  std::vector<uint8_t> inputBytes;
  llvm::LLVMContext context;
  std::unique_ptr<llvm::LLVMContext> reflectionContext;
  std::unique_ptr<llvm::Module> module;
  hlsl::DxilModule *dxilModule = nullptr;

  ~DxilLoadedShaderState();
};

/// @brief Controls how DXIL container patching is performed.
struct DxilContainerPatchOptions {
  bool restoreReflection = true;
  DxilRecipeExecutionOptions recipeExecutionOptions;
};

/// @brief Holds the result of parsing a DXIL recipe document.
struct DxilRecipeParseResult {
  DxilRecipe recipe;
  DxilContainerPatchOptions patchOptions;
  std::string error;
};

/// @brief Parses a DXIL recipe from YAML text.
bool ParseDxilRecipeText(llvm::StringRef recipeText,
                         DxilRecipeParseResult &result,
                         llvm::StringRef sourceName = "recipe");

/// @brief Parses a DXIL recipe from a file path.
bool ParseDxilRecipeFile(const std::string &recipePath,
                         DxilRecipeParseResult &result);
