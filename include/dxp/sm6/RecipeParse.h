#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

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
  /// Non-empty on failure. Contains glaze YAML parse errors (with line/column)
  /// or post-parse validation messages.
  std::string error;
};

/// @brief Parses a DXIL recipe from YAML text.
///
/// @param recipeText YAML recipe contents.
/// @param result Receives the parsed recipe or parse error.
/// @param sourceName Logical source name used in diagnostics.
/// @return `true` on success.
bool ParseDxilRecipeText(const std::string &recipeText,
                         DxilRecipeParseResult &result,
                         const std::string &sourceName = "recipe");

/// @brief Parses a DXIL recipe from a file path.
///
/// Equivalent to loading the file contents and calling `ParseDxilRecipeText`.
///
/// @param recipePath Path to the YAML recipe file.
/// @param result Receives the parsed recipe or parse error.
/// @return `true` on success.
bool ParseDxilRecipeFile(const std::string &recipePath,
                         DxilRecipeParseResult &result);
