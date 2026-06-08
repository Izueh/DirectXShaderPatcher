#pragma once

#include "Recipe.h"

#include <string>

namespace dxp::sm5 {

/// @brief Holds the result of parsing an SM5 recipe document.
struct RecipeParseResult {
  Recipe Recipe;
  /// Non-empty on failure. Contains glaze YAML parse errors (with line/column)
  /// or post-parse validation messages.
  std::string Error;
};

/// @brief Parses a declarative SM5 recipe from YAML text.
///
/// Schema v1 requires ordered `steps`, unique step names, and unique rule
/// names within the shared recipe namespace. Pattern probes should be
/// authored as `apply_rules` rules with `match.rewrite_mode: none`.
///
/// @param recipeText YAML recipe contents.
/// @param result Receives the parsed recipe or parse error.
/// @param sourceName Logical source name used in diagnostics.
/// @return `true` on success.
bool ParseRecipeText(const std::string &recipeText,
                     RecipeParseResult &result,
                     const std::string &sourceName = "recipe");

/// @brief Parses a declarative SM5 recipe from a file path.
///
/// Equivalent to loading the file contents and calling `ParseRecipeText`.
///
/// @param recipePath Path to the YAML recipe file.
/// @param result Receives the parsed recipe or parse error.
/// @return `true` on success.
bool ParseRecipeFile(const std::string &recipePath, RecipeParseResult &result);

} // namespace dxp::sm5