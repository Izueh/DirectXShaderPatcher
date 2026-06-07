#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

#include "Recipe.h"
#include "../ParseError.h"

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
  ::dxp::ParseError yaml_diagnostic;
};

/// @brief Parses a DXIL recipe from YAML text.
///
/// On failure, result.yaml_diagnostic contains structured location info
/// (line, column, path) for YAML parse errors, or a plain message
/// for post-parse validation failures.
bool ParseDxilRecipeText(const std::string &recipeText,
                         DxilRecipeParseResult &result,
                         const std::string &sourceName = "recipe");

/// @brief Parses a DXIL recipe from a file path.
///
/// This is equivalent to loading the file contents and calling
/// `ParseDxilRecipeText`.
///
/// On failure, result.yaml_diagnostic contains structured location info
/// (line, column, path) for YAML parse errors, or a plain message
/// for post-parse validation failures.
bool ParseDxilRecipeFile(const std::string &recipePath,
                         DxilRecipeParseResult &result);
