#pragma once

#include "Recipe.h"

#include <string>

#include "llvm/ADT/StringRef.h"

namespace dxp::sm5 {

/// Parse a declarative SM5 recipe from YAML text.
struct RecipeParseResult {
  Recipe Recipe;
  std::string Error;
};

bool ParseRecipeText(llvm::StringRef recipeText,
                     RecipeParseResult &result,
                     llvm::StringRef sourceName = "recipe");

bool ParseRecipeFile(const std::string &recipePath,
                     RecipeParseResult &result);

} // namespace dxp::sm5