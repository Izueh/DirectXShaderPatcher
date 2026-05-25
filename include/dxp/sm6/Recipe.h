#pragma once

#include <any>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Transforms.h"

/// @brief Controls which DXIL match is rewritten when a rule matches more than
/// once.
enum class DxilRecipeRuleApplicationMode {
  First,
  Last,
  MatchAll,
};

/// @brief Supplies optional inputs and initial state for DXIL recipe
/// execution.
struct DxilRecipeExecutionOptions {
  bool traceEnabled = false;
  std::unordered_map<std::string, std::any> inputs;
  std::unordered_map<std::string, std::any> initialState;
};

/// @brief Reports the result of executing one DXIL recipe step.
struct DxilRecipeStepResult {
  bool success = true;
  bool changed = false;
  unsigned matchCount = 0;
  bool invalidatedAnalyses = false;
  bool stopRecipe = false;
  bool resourceBindingsChanged = false;
  bool resourcesRefreshed = false;
  bool moduleVerified = false;
};

/// @brief Carries mutable state across DXIL recipe execution.
struct DxilRecipeContext {
  llvm::Module *module = nullptr;
  hlsl::DxilModule *dxilModule = nullptr;
  llvm::Function *entryFunction = nullptr;
  bool traceEnabled = false;
  unsigned totalRuleMatches = 0;
  bool moduleModified = false;
  bool resourceBindingsChanged = false;
  bool resourcesRefreshed = false;
  bool moduleVerified = false;
  std::string lastError;
  std::vector<std::string> diagnostics;
  std::unordered_map<std::string, TextureResourceDesc> textures;
  std::unordered_map<std::string, TextureResourceDesc> uavs;
  std::unordered_map<std::string, CBufferDesc> cbuffers;
  std::unordered_map<std::string, SamplerDesc> samplers;
  std::unordered_map<std::string, std::any> inputs;
  std::unordered_map<std::string, std::any> state;

  template <typename TValue>
  void SetInput(const std::string &name, TValue value) {
    inputs[name] = std::any(std::move(value));
  }

  template <typename TValue> TValue *FindInput(const std::string &name) {
    auto it = inputs.find(name);
    if (it == inputs.end())
      return nullptr;
    return std::any_cast<TValue>(&it->second);
  }

  template <typename TValue>
  const TValue *FindInput(const std::string &name) const {
    auto it = inputs.find(name);
    if (it == inputs.end())
      return nullptr;
    return std::any_cast<TValue>(&it->second);
  }

  template <typename TValue>
  void SetState(const std::string &name, TValue value) {
    state[name] = std::any(std::move(value));
  }

  template <typename TValue> TValue *FindState(const std::string &name) {
    auto it = state.find(name);
    if (it == state.end())
      return nullptr;
    return std::any_cast<TValue>(&it->second);
  }

  template <typename TValue>
  const TValue *FindState(const std::string &name) const {
    auto it = state.find(name);
    if (it == state.end())
      return nullptr;
    return std::any_cast<TValue>(&it->second);
  }
};

/// @brief Callable signature for custom DXIL recipe steps.
using DxilRecipeStepExecutor =
    std::function<DxilRecipeStepResult(DxilRecipeContext &)>;

/// @brief Creates a successful DXIL step result.
DxilRecipeStepResult MakeRecipeStepSuccess(bool changed = false,
                                           unsigned matchCount = 0,
                                           bool invalidatedAnalyses = false,
                                           bool stopRecipe = false);

/// @brief Creates a failed DXIL step result and records the message in
/// context.
DxilRecipeStepResult MakeRecipeStepFailure(DxilRecipeContext &context,
                                           std::string message);

/// @brief Represents one executable DXIL recipe step.
struct DxilRecipeStep {
  std::string name;
  DxilRecipeStepExecutor execute;
};

/// @brief Owns the ordered sequence of DXIL recipe steps.
class DxilRecipe {
public:
  DxilRecipe &AddStep(DxilRecipeStep step) {
    steps_.push_back(std::move(step));
    return *this;
  }

  const std::vector<DxilRecipeStep> &GetSteps() const { return steps_; }

private:
  std::vector<DxilRecipeStep> steps_;
};

/// @brief Wraps a custom executor as a named DXIL recipe step.
DxilRecipeStep MakeCustomRecipeStep(std::string name,
                                    DxilRecipeStepExecutor execute);

/// @brief Creates a step that adds a texture resource.
DxilRecipeStep MakeAddTextureStep(std::string id, TextureResourceDesc desc);

/// @brief Creates a step that adds a texture UAV resource.
DxilRecipeStep MakeAddTextureUAVStep(std::string id, TextureResourceDesc desc);

/// @brief Creates a step that adds a constant buffer resource.
DxilRecipeStep MakeAddCBufferStep(std::string id, CBufferDesc desc);

/// @brief Creates a step that adds a sampler resource.
DxilRecipeStep MakeAddSamplerStep(std::string id, SamplerDesc desc);

/// @brief Creates a step that applies DXIL rewrite rules.
DxilRecipeStep MakeApplyRewriteRulesStep(
    std::string name, std::vector<DxilRewriteRule> rules,
    DxilRecipeRuleApplicationMode mode = DxilRecipeRuleApplicationMode::First,
    bool required = true);

/// @brief Creates a step that asserts one or more patterns are present.
DxilRecipeStep MakePrefilterStep(std::string name,
                                 std::vector<DxilCallPattern> patterns);

/// @brief Creates a step that refreshes resource metadata.
DxilRecipeStep MakeRefreshResourcesStep(std::string name = "refresh_resources");

/// @brief Creates a step that prunes dead code.
DxilRecipeStep MakePruneDeadCodeStep(std::string name = "prune_dead_code");

/// @brief Executes a DXIL recipe with a simple trace toggle.
bool ExecuteDxilRecipe(const DxilRecipe &recipe, llvm::Module &module,
                       hlsl::DxilModule &dxilModule,
                       DxilRecipeContext *outContext = nullptr,
                       bool traceEnabled = false);

/// @brief Executes a DXIL recipe with explicit execution options.
bool ExecuteDxilRecipe(const DxilRecipe &recipe, llvm::Module &module,
                       hlsl::DxilModule &dxilModule,
                       const DxilRecipeExecutionOptions &options,
                       DxilRecipeContext *outContext = nullptr);