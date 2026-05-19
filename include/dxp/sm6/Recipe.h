#pragma once

#include <any>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Transforms.h"

enum class DxilRecipeRuleApplicationMode {
  Once,
  UntilNoMatch,
};

struct DxilRecipeExecutionOptions {
  bool traceEnabled = false;
  std::unordered_map<std::string, std::any> inputs;
  std::unordered_map<std::string, std::any> initialState;
};

struct DxilRecipeStepResult {
  bool success = true;
  bool changed = false;
  unsigned matchCount = 0;
  bool invalidatedAnalyses = false;
};

struct DxilRecipeContext {
  llvm::Module *module = nullptr;
  hlsl::DxilModule *dxilModule = nullptr;
  llvm::Function *entryFunction = nullptr;
  bool traceEnabled = false;
  unsigned totalRuleMatches = 0;
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

using DxilRecipeStepExecutor =
    std::function<DxilRecipeStepResult(DxilRecipeContext &)>;

DxilRecipeStepResult MakeRecipeStepSuccess(bool changed = false,
                                           unsigned matchCount = 0,
                                           bool invalidatedAnalyses = false);
DxilRecipeStepResult MakeRecipeStepFailure(DxilRecipeContext &context,
                                           std::string message);

struct DxilRecipeStep {
  std::string name;
  DxilRecipeStepExecutor execute;
};

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

DxilRecipeStep MakeCustomRecipeStep(std::string name,
                                    DxilRecipeStepExecutor execute);
DxilRecipeStep MakeAddTextureStep(std::string id, TextureResourceDesc desc);
DxilRecipeStep MakeAddTextureUAVStep(std::string id, TextureResourceDesc desc);
DxilRecipeStep MakeAddCBufferStep(std::string id, CBufferDesc desc);
DxilRecipeStep MakeAddSamplerStep(std::string id, SamplerDesc desc);
DxilRecipeStep MakeApplyRewriteRulesStep(
    std::string name, std::vector<DxilRewriteRule> rules,
    DxilRecipeRuleApplicationMode mode = DxilRecipeRuleApplicationMode::Once,
    bool required = true);
DxilRecipeStep MakeRefreshResourcesStep(std::string name = "refresh_resources");
DxilRecipeStep MakePruneDeadCodeStep(std::string name = "prune_dead_code");
DxilRecipeStep MakeVerifyModuleStep(std::string name = "verify_module");
DxilRecipeStep MakeExpectTextureStep(std::string id,
                                     std::string name = "expect_texture");
DxilRecipeStep
MakeExpectTextureUAVStep(std::string id,
                         std::string name = "expect_texture_uav");
DxilRecipeStep MakeExpectCBufferStep(std::string id,
                                     std::string name = "expect_cbuffer");
bool ExecuteDxilRecipe(const DxilRecipe &recipe, llvm::Module &module,
                       hlsl::DxilModule &dxilModule,
                       DxilRecipeContext *outContext = nullptr,
                       bool traceEnabled = false);
bool ExecuteDxilRecipe(const DxilRecipe &recipe, llvm::Module &module,
                       hlsl::DxilModule &dxilModule,
                       const DxilRecipeExecutionOptions &options,
                       DxilRecipeContext *outContext = nullptr);