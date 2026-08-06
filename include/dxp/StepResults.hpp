#pragma once

#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace dxp {

/// @brief Results from ApplyRuleStep.
struct ApplyRuleResults {
  uint32_t match_count = 0;
  uint32_t applied_count = 0;
};

/// @brief Results from AddResourceStep.
struct AddResourceResults {
  uint32_t textures_added = 0;
  uint32_t raw_resources_added = 0;
  uint32_t structured_resources_added = 0;
  uint32_t cbuffers_added = 0;
  uint32_t samplers_added = 0;
  uint32_t uavs_added = 0;
  uint32_t inputs_added = 0;
  uint32_t outputs_added = 0;
  uint32_t temps_added = 0;
};

/// @brief Results from CheckShaderVersionStep.
struct CheckShaderVersionResults {
  uint32_t major_version = 0;
  uint32_t minor_version = 0;
};

/// @brief Results from CheckOpcodeCountStep.
struct CheckOpcodeCountResults {
  std::unordered_map<std::string, int32_t> opcode_counts;       ///< General opcode counts.
  std::unordered_map<std::string, int32_t> dxil_opcode_counts;  ///< DXIL opcode counts (SM6 only).
  std::unordered_map<std::string, int32_t> llvm_opcode_counts;  ///< LLVM opcode counts (SM6 only).
};

/// @brief Results from CheckResourceCountStep.
struct CheckResourceCountResults {
  int32_t textures = 0;       ///< SRV count.
  int32_t samplers = 0;       ///< Sampler count.
  int32_t cbuffers = 0;       ///< CBuffer count.
  int32_t uavs = 0;           ///< UAV count.
  int32_t thread_groups = 0;  ///< Thread group count (always 0 for SM6).
  int32_t total = 0;          ///< Total resource count.
};

/// @brief Variant of all per-step Results types.
using ResultsVariant = std::variant<
    ApplyRuleResults,
    AddResourceResults,
    CheckShaderVersionResults,
    CheckOpcodeCountResults,
    CheckResourceCountResults>;

/// @brief Summarizes one executed recipe step.
struct StepReport {
  std::string name;
  ResultsVariant results;
  bool success = false;
};

}  // namespace dxp
