#pragma once

#include "Recipe.h"
#include "../ParseError.h"

#include <string>

namespace dxp::sm5 {

/// @brief Holds the result of parsing an SM5 recipe document.
struct RecipeParseResult {
  Recipe Recipe;
  ParseError Error;
};

/// @brief Parses a declarative SM5 recipe from YAML text.
///
/// Schema version `1` requires ordered `steps`, requires unique step names,
/// and requires unique rule names within the shared recipe name namespace.
/// Scalar check steps (`check_shader_version`, `check_opcode_count`,
/// `check_resource_count`) are supported directly, and pattern probes should
/// be authored as `apply_rules` rules with `match.rewrite_mode: none`.
/// @param recipeText YAML recipe contents.
/// @param result Receives the parsed recipe or parse error.
/// @param sourceName Logical source name used in diagnostics.
/// @return `true` on success.
///
/// Error details include line/column/path from glaze YAML parsing,
/// or plain message from post-parse validation.
bool ParseRecipeText(const std::string &recipeText,
                     RecipeParseResult &result,
                     const std::string &sourceName = "recipe");

/// @brief Parses a declarative SM5 recipe from a file path.
///
/// This is equivalent to loading the file contents and calling
/// `ParseRecipeText`.
/// @param recipePath Path to the YAML recipe file.
/// @param result Receives the parsed recipe or parse error.
/// @return `true` on success.
///
/// On failure, result.Error contains structured location info
/// (line, column, path) for YAML parse errors, or a plain message
/// for post-parse validation failures.
bool ParseRecipeFile(const std::string &recipePath, RecipeParseResult &result);

} // namespace dxp::sm5