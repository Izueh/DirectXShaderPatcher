#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace dxp {

/// @brief Shared validation context for recipe validation.
struct ValidationContext {
  std::unordered_set<std::string> names;
  std::unordered_set<std::string> handles;
  std::unordered_set<std::string> instruction_captures;
  std::unordered_set<std::string> operand_captures;
  std::unordered_set<std::string> index_captures;
  std::unordered_set<std::string> blob_names;
  std::unordered_set<std::string> template_names;
  std::unordered_set<std::string> template_temp_names;
  std::unordered_set<std::string> template_param_names;
  /// @brief Declared params per template (name → param names); registered at
  /// declare_template Validate; enforced at the invoking apply_rule step's
  /// Validate (all declared params must be provided by the entry).
  std::unordered_map<std::string, std::vector<std::string>> template_params;
  /// @brief Templates' required captures (name → capture names referenced by
  /// the template's emits). Registered at declare_template Validate time;
  /// enforced at the invoking apply_rule step's Validate (each required
  /// capture must be producible by a previous or current match step).
  std::unordered_map<std::string, std::unordered_set<std::string>> template_required_captures;
};

}  // namespace dxp
