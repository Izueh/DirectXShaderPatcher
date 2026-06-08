#pragma once

#include <any>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../PatchReport.h"
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
/// TODO: Rename `inputs` → `variables` to match SM5 redesign.
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
  std::vector<dxp::PatchRuleReport> ruleReports;
  std::vector<dxp::PatchSideEffect> sideEffects;
};

/// @brief Describes a generic DXIL step guard based on recipe context state.
struct DxilRecipeStepCondition {
  std::string state;
  std::vector<DxilRecipeStepCondition> all;
  std::vector<DxilRecipeStepCondition> any;
  bool negate = false;

  static DxilRecipeStepCondition FromState(std::string stateValue,
                                           bool negateValue = false) {
    DxilRecipeStepCondition condition;
    condition.state = std::move(stateValue);
    condition.negate = negateValue;
    return condition;
  }

  static DxilRecipeStepCondition
  AllOf(std::vector<DxilRecipeStepCondition> conditions,
        bool negateValue = false) {
    DxilRecipeStepCondition condition;
    condition.all = std::move(conditions);
    condition.negate = negateValue;
    return condition;
  }

  static DxilRecipeStepCondition
  AnyOf(std::vector<DxilRecipeStepCondition> conditions,
        bool negateValue = false) {
    DxilRecipeStepCondition condition;
    condition.any = std::move(conditions);
    condition.negate = negateValue;
    return condition;
  }

  bool IsSet() const {
    return !state.empty() || !all.empty() || !any.empty();
  }
};

/// @brief Carries mutable state across DXIL recipe execution.
/// TODO: Merge `inputs` → `variables`, remove `SetInput()`/`FindInput()`,
///       make `SetState()` internal-only (same pattern as SM5 redesign).
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
using DxilRecipeStepPredicate =
  std::function<bool(DxilRecipeContext &)>;

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
  bool required = true;
  DxilRecipeStepCondition ifCondition;
  DxilRecipeStepExecutor execute;
  DxilRecipeStepPredicate predicate;

  DxilRecipeStep &Require(bool isRequired) & {
    required = isRequired;
    return *this;
  }

  DxilRecipeStep &&Require(bool isRequired) && {
    required = isRequired;
    return std::move(*this);
  }

  DxilRecipeStep &When(DxilRecipeStepCondition condition) & {
    ifCondition = std::move(condition);
    return *this;
  }

  DxilRecipeStep &&When(DxilRecipeStepCondition condition) && {
    ifCondition = std::move(condition);
    return std::move(*this);
  }

  DxilRecipeStep &When(DxilRecipeStepPredicate stepPredicate) & {
    predicate = std::move(stepPredicate);
    return *this;
  }

  DxilRecipeStep &&When(DxilRecipeStepPredicate stepPredicate) && {
    predicate = std::move(stepPredicate);
    return std::move(*this);
  }
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
                                 std::vector<DxilCallPattern> patterns,
                                 std::string setState = {});

/// @brief Creates a step that refreshes resource metadata.
DxilRecipeStep MakeRefreshResourcesStep(std::string name = "refresh_resources");

/// @brief Creates a step that prunes dead code.
DxilRecipeStep MakePruneDeadCodeStep(std::string name = "prune_dead_code");

/// @brief Executes a DXIL recipe with a simple trace toggle.
bool ExecuteDxilRecipe(const DxilRecipe &recipe, llvm::Module &module,
                       hlsl::DxilModule &dxilModule,
                       DxilRecipeContext *outContext = nullptr,
                       bool traceEnabled = false,
                       dxp::PatchReport *outReport = nullptr);

/// @brief Executes a DXIL recipe with explicit execution options.
bool ExecuteDxilRecipe(const DxilRecipe &recipe, llvm::Module &module,
                       hlsl::DxilModule &dxilModule,
                       const DxilRecipeExecutionOptions &options,
                       DxilRecipeContext *outContext = nullptr,
                       dxp::PatchReport *outReport = nullptr);