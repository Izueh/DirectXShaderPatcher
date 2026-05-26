#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

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
  /// Handle used by the recipe and reported back to the caller.
  std::string Handle;
  /// Final shader-facing resource class that must be bound.
  PatchResourceKind ResourceKind = PatchResourceKind::Unknown;
  /// Final bind register within the reported register space.
  uint32_t BindPoint = 0;
  /// Final register space for the binding.
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

/// @brief Caller-facing patch report surface.
/// Additional execution details can be added here without changing the
/// mutable recipe-context contract.
struct PatchReport {
  /// Per-step execution summary, including rule reports and step-local
  /// binding side effects when a caller needs execution detail.
  std::vector<PatchStepReport> Steps;
  /// Final shader-facing bindings introduced by the patch, keyed by recipe
  /// handle. This is the normalized output contract a host should read when it
  /// needs to know what newly required resources must be bound.
  std::unordered_map<std::string, PatchBindingValue> NewBindings;
  /// Summary of the final serialized output container.
  PatchContainerReport OutputContainer;
};

} // namespace dxp