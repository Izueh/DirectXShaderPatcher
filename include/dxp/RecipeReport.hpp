#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "dxp/ExportTypes.hpp"
#include "dxp/StepResults.hpp"

namespace dxp {

/// @brief Result of executing a recipe (shared across SM5/SM6).
struct RecipeReport {
  std::vector<StepReport> steps;                                     ///< Reports for each step executed during the recipe.
  std::unordered_map<std::string, ResourceBinding> new_bindings;     ///< New resource bindings introduced during patching.
  std::unordered_map<std::string, ResourceUsage> resource_usage;     ///< Typed exports: resources found during pattern matching.
  std::unordered_map<std::string, ImmediateValue> immediate_values;  ///< Typed exports: immediate values found during pattern matching.
  PatchContainerReport output_container;                             ///< Report of the serialized output container.
  std::vector<uint8_t> output_bytes;                                 ///< Raw output bytes (DXBC/DXIL container).
  bool modified = false;                                             ///< True when a step actually rewrote the program (vs. a pass-through of the input bytes).
};

}  // namespace dxp
