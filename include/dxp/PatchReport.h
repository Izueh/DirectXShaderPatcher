#pragma once

#include <any>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "dxp/sm5/Types.h"

namespace dxp {

enum class PatchSideEffectKind {
  None,
  ResourceAdded,
};

enum class PatchResourceKind {
  Unknown,
  Input,
  Output,
  Texture,
  TextureUav,
  RawResource,
  StructuredResource,
  CBuffer,
  Sampler,
  Uav,
};

struct PatchBindingValue {
  std::string Handle;
  PatchResourceKind ResourceKind = PatchResourceKind::Unknown;
  uint32_t BindPoint = 0;
  uint32_t Space = 0;
};

/// @brief Summarizes one caller-visible side effect produced while patching.
/// This is patch-time provenance: it explains which step or rule introduced a
/// resource or other reported change. Callers that only need the final shader
/// contract should prefer PatchReport::NewBindings.
struct PatchSideEffect {
  PatchSideEffectKind Kind = PatchSideEffectKind::None;
  PatchResourceKind ResourceKind = PatchResourceKind::Unknown;
  std::string StepName;
  std::string RuleName;
  std::string Handle;
  uint32_t BindPoint = 0;
  uint32_t Space = 0;
  bool Changed = false;
  std::string Description;
};

/// @brief Summarizes one rewrite rule executed within a step.
struct PatchRuleReport {
  std::string Name;
  uint32_t MatchCount = 0;
  uint32_t AppliedCount = 0;
  bool Changed = false;
};

/// @brief Summarizes one executed recipe step.
struct PatchStepReport {
  std::string Name;
  bool Executed = false;
  bool Skipped = false;
  bool Success = true;
  bool Changed = false;
  bool StopRecipe = false;
  bool Required = true;
  uint32_t MatchCount = 0;
  std::string Error;
  std::vector<PatchRuleReport> Rules;
  std::vector<PatchSideEffect> SideEffects;
};

/// @brief Summarizes one serialized container chunk or part.
struct PatchChunkReport {
  std::string Id;
  uint32_t FourCC = 0;
  uint32_t OffsetInContainer = 0;
  uint32_t SizeInBytes = 0;
};

/// @brief Summarizes the final serialized output container.
struct PatchContainerReport {
  std::string Format;
  uint32_t TotalSizeInBytes = 0;
  std::string HashHex;
  std::vector<PatchChunkReport> Chunks;
};

/// @brief Wraps all exported data from a recipe execution.
///
/// Populated when the recipe defines exports (YAML `exports` or C++
/// `Recipe::AddExport`). Callers access captured operands, instructions,
/// index values, variables, and state through typed getters.
struct PatchExport {
  std::unordered_map<std::string, dxp::sm5::CapturedOperand> captured_operands;
  std::unordered_map<std::string, dxp::sm5::CapturedInstruction> captured_instructions;
  std::unordered_map<std::string, uint32_t> captured_index_values;
  std::unordered_map<std::string, std::any> variables;
  std::unordered_map<std::string, std::any> state;

  /// @brief Looks up a captured operand by name.
  const dxp::sm5::CapturedOperand *GetCapturedOperand(const std::string &name) const {
    const auto it = captured_operands.find(name);
    return it == captured_operands.end() ? nullptr : &it->second;
  }

  /// @brief Looks up a captured instruction by name.
  const dxp::sm5::CapturedInstruction *GetCapturedInstruction(const std::string &name) const {
    const auto it = captured_instructions.find(name);
    return it == captured_instructions.end() ? nullptr : &it->second;
  }

  /// @brief Looks up a captured index value by name.
  const uint32_t *GetCapturedOperandIndexValue(const std::string &name) const {
    const auto it = captured_index_values.find(name);
    return it == captured_index_values.end() ? nullptr : &it->second;
  }
};

/// @brief Caller-facing patch report surface.
/// Additional execution details can be added here without changing the
/// mutable recipe-context contract.
struct PatchReport {
  std::vector<PatchStepReport> Steps;
  std::unordered_map<std::string, PatchBindingValue> NewBindings;
  PatchContainerReport OutputContainer;
  PatchExport Exports;
};

} // namespace dxp