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
/// Schema version `1` requires ordered `steps`, requires unique step names,
/// and requires unique rule names within the shared recipe name namespace.
/// Scalar check steps (`check_shader_version`, `check_opcode_count`,
/// `check_resource_count`) are supported directly, and pattern probes should
/// be authored as `apply_rules` rules with `match.rewrite_mode: none`.
///
/// On failure, `result.Error` contains glaze YAML parse diagnostics with
/// line/column information (e.g. "recipe:5:22: unknown_key") or
/// post-parse validation messages.
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
/// This is equivalent to loading the file contents and calling
/// `ParseRecipeText`.
///
/// On failure, `result.Error` contains glaze YAML parse diagnostics with
/// line/column information (e.g. "recipe:5:22: unknown_key") or
/// post-parse validation messages.
///
/// @param recipePath Path to the YAML recipe file.
/// @param result Receives the parsed recipe or parse error.
/// @return `true` on success.
bool ParseRecipeFile(const std::string &recipePath, RecipeParseResult &result);

} // namespace dxp::sm5