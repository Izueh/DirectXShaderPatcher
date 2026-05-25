#pragma once

#include "Recipe.h"

#include <string>

#include "llvm/ADT/StringRef.h"

namespace dxp::sm5 {

/// @brief Holds the result of parsing an SM5 recipe document.
struct RecipeParseResult {
  Recipe Recipe;
  std::string Error;
};

/// @brief Parses a declarative SM5 recipe from YAML text.
/// @param recipeText YAML recipe contents.
/// @param result Receives the parsed recipe or parse error.
/// @param sourceName Logical source name used in diagnostics.
/// @return `true` on success.
bool ParseRecipeText(llvm::StringRef recipeText, RecipeParseResult &result,
                     llvm::StringRef sourceName = "recipe");

/// @brief Parses a declarative SM5 recipe from a file path.
/// @param recipePath Path to the YAML recipe file.
/// @param result Receives the parsed recipe or parse error.
/// @return `true` on success.
bool ParseRecipeFile(const std::string &recipePath, RecipeParseResult &result);

} // namespace dxp::sm5