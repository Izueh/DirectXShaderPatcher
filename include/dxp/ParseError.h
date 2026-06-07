#pragma once

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace dxp {

/// @brief Structured parse error with location information from glaze.
struct ParseError {
  /// YAML line number (1-based), 0 if not available.
  std::uint32_t line = 0;
  /// YAML column number (1-based), 0 if not available.
  std::uint32_t column = 0;
  /// JSON path segments leading to the error (e.g. {"steps", "0", "if"}).
  std::vector<std::string> path;
  /// Human-readable error message.
  std::string message;

  /// @brief Returns true if this is a valid, populated error.
  [[nodiscard]] bool has_error() const noexcept {
    return !message.empty();
  }

  /// @brief Format as a human-readable diagnostic string.
  /// @param sourceName Logical source name (e.g. file path or "recipe").
  /// @return Formatted string like "recipe.yml:12:5: path.to.field: message"
  [[nodiscard]] std::string format(const std::string &sourceName) const {
    if (!has_error()) {
      return sourceName;
    }

    std::string result = sourceName;
    if (line > 0 || column > 0) {
      result += ":";
      if (line > 0) result += std::to_string(line);
      if (column > 0) result += ":" + std::to_string(column);
    }
    if (!path.empty()) {
      result += ":";
      for (size_t i = 0; i < path.size(); ++i) {
        if (i > 0) result += ".";
        result += path[i];
      }
    }
    result += ": " + message;
    return result;
  }
};

/// @brief Stream operator for ParseError (outputs the message).
inline std::ostream &operator<<(std::ostream &os, const ParseError &err) {
  os << err.message;
  return os;
}

} // namespace dxp
