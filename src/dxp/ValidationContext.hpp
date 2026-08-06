#pragma once

#include <string>
#include <unordered_set>

namespace dxp {

/// @brief Shared validation context for recipe validation.
struct ValidationContext {
  std::unordered_set<std::string> names;
  std::unordered_set<std::string> handles;
  std::unordered_set<std::string> instruction_captures;
  std::unordered_set<std::string> operand_captures;
  std::unordered_set<std::string> index_captures;
};

}  // namespace dxp
