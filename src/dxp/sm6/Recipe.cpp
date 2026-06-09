#include "../../../include/dxp/sm6/Recipe.h"

#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../../../include/dxp/sm6/Patch.h"
#include "../../../include/dxp/sm6/Resources.h"
#include "../../../include/dxp/sm6/Transforms.h"

#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"

#include "dxc/DXIL/DxilConstants.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilResourceBase.h"

using llvm::Module;

namespace {

static void AppendRecipeDiagnostic(DxilRecipeContext &context,
                                   const std::string &message) {
  context.diagnostics.push_back(message);
  if (context.traceEnabled)
    std::cerr << message << "\n";
}

static DxilRecipeStepResult FailRecipeStep(DxilRecipeContext &context,
                                           const std::string &message) {
  context.lastError = message;
  AppendRecipeDiagnostic(context, message);
  DxilRecipeStepResult result;
  result.success = false;
  return result;
}

static dxp::PatchSideEffect MakeAddedBindingSideEffect(
    dxp::PatchResourceKind resourceKind, std::string handle,
    unsigned bindPoint, unsigned space, std::string description) {
  dxp::PatchSideEffect sideEffect;
  sideEffect.Kind = dxp::PatchSideEffectKind::ResourceAdded;
  sideEffect.ResourceKind = resourceKind;
  sideEffect.Handle = std::move(handle);
  sideEffect.BindPoint = bindPoint;
  sideEffect.Space = space;
  sideEffect.Changed = true;
  sideEffect.Description = std::move(description);
  return sideEffect;
}

static void AppendBindingExports(dxp::PatchReport &report,
                                 const std::vector<dxp::PatchSideEffect> &sideEffects) {
  for (const dxp::PatchSideEffect &sideEffect : sideEffects) {
    if (sideEffect.Kind != dxp::PatchSideEffectKind::ResourceAdded ||
        sideEffect.Handle.empty()) {
      continue;
    }

    dxp::PatchBindingValue binding;
    binding.Handle = sideEffect.Handle;
    binding.ResourceKind = sideEffect.ResourceKind;
    binding.BindPoint = sideEffect.BindPoint;
    binding.Space = sideEffect.Space;
    report.NewBindings[sideEffect.Handle] = std::move(binding);
  }
}

static const hlsl::DxilResourceBase *
FindResourceByBinding(const hlsl::DxilModule &dxilModule,
                      hlsl::DXIL::ResourceClass resourceClass,
                      unsigned bindPoint, unsigned space) {
  auto matches = [bindPoint, space](const auto &resource) {
    return resource->GetLowerBound() == bindPoint &&
           resource->GetSpaceID() == space;
  };

  switch (resourceClass) {
  case hlsl::DXIL::ResourceClass::SRV:
    for (const auto &resource : dxilModule.GetSRVs()) {
      if (matches(resource))
        return resource.get();
    }
    break;
  case hlsl::DXIL::ResourceClass::UAV:
    for (const auto &resource : dxilModule.GetUAVs()) {
      if (matches(resource))
        return resource.get();
    }
    break;
  case hlsl::DXIL::ResourceClass::CBuffer:
    for (const auto &resource : dxilModule.GetCBuffers()) {
      if (matches(resource))
        return resource.get();
    }
    break;
  case hlsl::DXIL::ResourceClass::Sampler:
    for (const auto &resource : dxilModule.GetSamplers()) {
      if (matches(resource))
        return resource.get();
    }
    break;
  default:
    break;
  }

  return nullptr;
}

static DxilRecipeStepResult
ApplyRecipeRewriteRules(DxilRecipeContext &context,
                        const std::vector<DxilRewriteRule> &rules,
                        DxilRecipeRuleApplicationMode mode, bool required,
                        const std::string &stepName) {
  if (context.module == nullptr || context.dxilModule == nullptr) {
    return FailRecipeStep(
        context, stepName + ": recipe context is missing module state");
  }

  context.entryFunction = context.dxilModule->GetEntryFunction();
  if (context.entryFunction == nullptr) {
    return FailRecipeStep(context,
                          stepName + ": failed to locate DXIL entry function");
  }

  unsigned totalMatches = 0;
  unsigned totalMutations = 0;
  std::vector<DxilRuleApplicationReport> ruleReports;
  if (mode == DxilRecipeRuleApplicationMode::MatchAll) {
    if (!ApplyDxilRewriteRulesMatchAll(*context.entryFunction, *context.module,
                                       *context.dxilModule, rules,
                                       &totalMatches, &totalMutations,
                                       &ruleReports)) {
      return FailRecipeStep(context,
                            stepName + ": rewrite rule application failed");
    }
  } else {
    if (!ApplyDxilRewriteRulesOnce(*context.entryFunction, *context.module,
                                   *context.dxilModule, rules,
                                   mode == DxilRecipeRuleApplicationMode::Last,
                                   &totalMatches, &totalMutations,
                                   &ruleReports)) {
      return FailRecipeStep(context,
                            stepName + ": rewrite rule application failed");
    }
  }

  if (required && totalMatches == 0) {
    return FailRecipeStep(context,
                          stepName + ": no rewrite matches were applied");
  }

  DxilRecipeStepResult result;
  result.changed = totalMutations != 0;
  result.matchCount = totalMatches;
  result.invalidatedAnalyses = totalMutations != 0;
  result.ruleReports.reserve(ruleReports.size());
  for (const DxilRuleApplicationReport &ruleReport : ruleReports) {
    dxp::PatchRuleReport reportEntry;
    reportEntry.Name = ruleReport.name;
    reportEntry.MatchCount = ruleReport.matchCount;
    reportEntry.AppliedCount = ruleReport.appliedCount;
    reportEntry.Changed = ruleReport.mutatedCount != 0;
    result.ruleReports.push_back(std::move(reportEntry));
  }
  return result;
}

static unsigned CountPrefilterMatches(llvm::Function &entryFunction,
                                      hlsl::DxilModule &dxilModule,
                                      const DxilCallPattern &pattern) {
  std::vector<DxilMatchResult> matches;
  return CollectDxilCallMatches(entryFunction, pattern, matches, &dxilModule);
}

static DxilRecipeStepResult
EvaluateRecipePrefilter(DxilRecipeContext &context,
                        const std::vector<DxilCallPattern> &patterns,
                        const std::string &stepName,
                        const std::string &stateKey) {
  if (context.dxilModule == nullptr) {
    return FailRecipeStep(context,
                          stepName + ": recipe context is missing DXIL module");
  }

  context.entryFunction = context.dxilModule->GetEntryFunction();
  if (context.entryFunction == nullptr) {
    return FailRecipeStep(context,
                          stepName + ": failed to locate DXIL entry function");
  }

  unsigned matchedPatterns = 0;
  for (const DxilCallPattern &pattern : patterns) {
    if (CountPrefilterMatches(*context.entryFunction, *context.dxilModule,
                              pattern) != 0) {
      ++matchedPatterns;
    }
  }

  if (!stateKey.empty()) {
    context.SetState<bool>(stateKey, matchedPatterns != 0);
  }
  return MakeRecipeStepSuccess(false, matchedPatterns);
}

static bool EvaluateStepCondition(const DxilRecipeStepCondition &condition,
                                  const DxilRecipeContext &context) {
  if (!condition.IsSet()) {
    return true;
  }

  bool value = false;

  if (!condition.state.empty()) {
    const bool *boolValue = context.FindState<bool>(condition.state);
    if (boolValue != nullptr) {
      value = *boolValue;
    } else {
      const uint32_t *u32Value = context.FindState<uint32_t>(condition.state);
      if (u32Value != nullptr) {
        value = *u32Value != 0;
      } else {
        const int32_t *i32Value = context.FindState<int32_t>(condition.state);
        if (i32Value != nullptr) {
          value = *i32Value != 0;
        } else {
          const std::string *stringValue =
              context.FindState<std::string>(condition.state);
          if (stringValue != nullptr) {
            value = !stringValue->empty();
          }
        }
      }
    }
  } else if (!condition.all.empty()) {
    value = true;
    for (const DxilRecipeStepCondition &child : condition.all) {
      if (!EvaluateStepCondition(child, context)) {
        value = false;
        break;
      }
    }
  } else if (!condition.any.empty()) {
    value = false;
    for (const DxilRecipeStepCondition &child : condition.any) {
      if (EvaluateStepCondition(child, context)) {
        value = true;
        break;
      }
    }
  }

  return condition.negate ? !value : value;
}

static bool ShouldExecuteStep(const DxilRecipeStep &step,
                              DxilRecipeContext &context) {
  if (!EvaluateStepCondition(step.ifCondition, context)) {
    return false;
  }
  if (step.predicate && !step.predicate(context)) {
    return false;
  }
  return true;
}

}

DxilRecipeStepResult MakeRecipeStepSuccess(bool changed, unsigned matchCount,
                                           bool invalidatedAnalyses,
                                           bool stopRecipe) {
  DxilRecipeStepResult result;
  result.success = true;
  result.changed = changed;
  result.matchCount = matchCount;
  result.invalidatedAnalyses = invalidatedAnalyses;
  result.stopRecipe = stopRecipe;
  return result;
}

DxilRecipeStepResult MakeRecipeStepFailure(DxilRecipeContext &context,
                                           std::string message) {
  return FailRecipeStep(context, message);
}

DxilRecipeStep MakeCustomRecipeStep(std::string name,
                                    DxilRecipeStepExecutor execute) {
  return DxilRecipeStep{std::move(name), true, {}, std::move(execute), {}};
}

DxilRecipeStep MakeAddTextureStep(std::string id, TextureResourceDesc desc) {
  return DxilRecipeStep{
      "add_texture:" + id,
      true,
      {},
      [id = std::move(id), desc](DxilRecipeContext &context) {
        if (context.module == nullptr || context.dxilModule == nullptr) {
          return FailRecipeStep(
              context, "add_texture: recipe context is missing module state");
        }

        TextureResourceDesc resolvedDesc = desc;
        if (resolvedDesc.binding.IsAutoBinding()) {
          resolvedDesc.binding.SetBindPoint(
              FindNextAvailableBinding(context.dxilModule->GetSRVs(),
                                       resolvedDesc.binding.GetSpace(), 0));
        }

        if (!AddTextureSRV(*context.module, *context.dxilModule,
                           resolvedDesc)) {
          return FailRecipeStep(
              context,
              "add_texture: failed to add texture resource '" + id + "'");
        }

        context.textures[id] = resolvedDesc;
        DxilRecipeStepResult result;
        result.changed = true;
        result.invalidatedAnalyses = true;
        result.resourceBindingsChanged = true;
        result.sideEffects.push_back(MakeAddedBindingSideEffect(
            dxp::PatchResourceKind::Texture, id,
            resolvedDesc.binding.GetBindPoint(), resolvedDesc.binding.GetSpace(),
            "added DXIL texture binding"));
        return result;
      },
      {}};
}

    DxilRecipeStep MakeAddTextureUAVStep(std::string id, TextureResourceDesc desc) {
  return DxilRecipeStep{
      "add_texture_uav:" + id,
      true,
      {},
      [id = std::move(id), desc](DxilRecipeContext &context) {
        if (context.module == nullptr || context.dxilModule == nullptr) {
          return FailRecipeStep(
              context,
              "add_texture_uav: recipe context is missing module state");
        }

        TextureResourceDesc resolvedDesc = desc;
        if (resolvedDesc.binding.IsAutoBinding()) {
          resolvedDesc.binding.SetBindPoint(
              FindNextAvailableBinding(context.dxilModule->GetUAVs(),
                                       resolvedDesc.binding.GetSpace(), 0));
        }

        if (!AddTextureUAV(*context.module, *context.dxilModule,
                           resolvedDesc)) {
          return FailRecipeStep(context,
                                "add_texture_uav: failed to add texture UAV '" +
                                    id + "'");
        }

        context.uavs[id] = resolvedDesc;
        DxilRecipeStepResult result;
        result.changed = true;
        result.invalidatedAnalyses = true;
        result.resourceBindingsChanged = true;
        result.sideEffects.push_back(MakeAddedBindingSideEffect(
            dxp::PatchResourceKind::TextureUav, id,
            resolvedDesc.binding.GetBindPoint(), resolvedDesc.binding.GetSpace(),
            "added DXIL texture UAV binding"));
        return result;
      },
      {}};
}

    DxilRecipeStep MakeAddCBufferStep(std::string id, CBufferDesc desc) {
  std::shared_ptr<CBufferSchema> schemaCopy;
  if (desc.schema != nullptr)
    schemaCopy = std::make_shared<CBufferSchema>(*desc.schema);

  return DxilRecipeStep{
      "add_cbuffer:" + id,
      true,
      {},
      [id = std::move(id), desc, schemaCopy](DxilRecipeContext &context) {
        if (context.module == nullptr || context.dxilModule == nullptr) {
          return FailRecipeStep(
              context, "add_cbuffer: recipe context is missing module state");
        }

        CBufferDesc resolvedDesc = desc;
        if (schemaCopy)
          resolvedDesc.schema = schemaCopy.get();
        if (resolvedDesc.binding.IsAutoBinding()) {
          resolvedDesc.binding.SetBindPoint(
              FindNextAvailableBinding(context.dxilModule->GetCBuffers(),
                                       resolvedDesc.binding.GetSpace(), 0));
        }

        if (!AddCBuffer(*context.module, *context.dxilModule, resolvedDesc)) {
          return FailRecipeStep(
              context, "add_cbuffer: failed to add cbuffer '" + id + "'");
        }

        context.cbuffers[id] = resolvedDesc;
        DxilRecipeStepResult result;
        result.changed = true;
        result.invalidatedAnalyses = true;
        result.resourceBindingsChanged = true;
        result.sideEffects.push_back(MakeAddedBindingSideEffect(
            dxp::PatchResourceKind::CBuffer, id,
            resolvedDesc.binding.GetBindPoint(), resolvedDesc.binding.GetSpace(),
            "added DXIL cbuffer binding"));
        return result;
      },
      {}};
}

    DxilRecipeStep MakeAddSamplerStep(std::string id, SamplerDesc desc) {
  return DxilRecipeStep{
      "add_sampler:" + id,
      true,
      {},
      [id = std::move(id), desc](DxilRecipeContext &context) {
        if (context.module == nullptr || context.dxilModule == nullptr) {
          return FailRecipeStep(
              context, "add_sampler: recipe context is missing module state");
        }

        SamplerDesc resolvedDesc = desc;
        if (resolvedDesc.binding.IsAutoBinding()) {
          resolvedDesc.binding.SetBindPoint(
              FindNextAvailableBinding(context.dxilModule->GetSamplers(),
                                       resolvedDesc.binding.GetSpace(), 0));
        }

        if (!AddSampler(*context.module, *context.dxilModule, resolvedDesc)) {
          return FailRecipeStep(
              context, "add_sampler: failed to add sampler '" + id + "'");
        }

        context.samplers[id] = resolvedDesc;
        DxilRecipeStepResult result;
        result.changed = true;
        result.invalidatedAnalyses = true;
        result.resourceBindingsChanged = true;
        result.sideEffects.push_back(MakeAddedBindingSideEffect(
            dxp::PatchResourceKind::Sampler, id,
            resolvedDesc.binding.GetBindPoint(), resolvedDesc.binding.GetSpace(),
            "added DXIL sampler binding"));
        return result;
      },
      {}};
}

DxilRecipeStep MakeApplyRewriteRulesStep(std::string name,
                                         std::vector<DxilRewriteRule> rules,
                                         DxilRecipeRuleApplicationMode mode,
                     bool required) {
  return DxilRecipeStep{name, required, {},
                        [rules = std::move(rules), mode, required,
                         name](DxilRecipeContext &context) {
                          return ApplyRecipeRewriteRules(context, rules, mode,
                                                         required, name);
                        },
                        {}};
}

DxilRecipeStep MakePrefilterStep(std::string name,
                                 std::vector<DxilCallPattern> patterns,
                                 std::string setState) {
  return DxilRecipeStep{
      name,
      true,
      {},
      [patterns = std::move(patterns), name,
       setState = std::move(setState)](DxilRecipeContext &context) {
        const std::string &stateKey = setState.empty() ? name : setState;
        return EvaluateRecipePrefilter(context, patterns, name, stateKey);
      },
      {}};
}

    DxilRecipeStep MakeRefreshResourcesStep(std::string name) {
  return DxilRecipeStep{
      name, true, {}, [name](DxilRecipeContext &context) {
        if (context.dxilModule == nullptr) {
          return FailRecipeStep(
              context, name + ": recipe context is missing DXIL module");
        }

        RefreshDxilModule(*context.dxilModule, context.traceEnabled);
        DxilRecipeStepResult result;
        result.changed = true;
        result.resourcesRefreshed = true;
        return result;
      }, {}};
}

    DxilRecipeStep MakePruneDeadCodeStep(std::string name) {
  return DxilRecipeStep{
      name, true, {}, [name](DxilRecipeContext &context) {
        if (context.dxilModule == nullptr) {
          return FailRecipeStep(
              context, name + ": recipe context is missing DXIL module");
        }

        context.entryFunction = context.dxilModule->GetEntryFunction();
        if (context.entryFunction == nullptr) {
          return FailRecipeStep(
              context, name + ": failed to locate DXIL entry function");
        }

        PruneFunctionDeadCode(*context.entryFunction);

        {
          hlsl::OP *op = context.dxilModule->GetOP();
          if (op)
            op->RefreshCache();
        }

        return MakeRecipeStepSuccess(true, 0, true);
      }, {}};
}

bool ExecuteDxilRecipe(const DxilRecipe &recipe, Module &module,
                       hlsl::DxilModule &dxilModule,
                       DxilRecipeContext *outContext, bool traceEnabled,
                       dxp::PatchReport *outReport) {
  DxilRecipeExecutionOptions options;
  options.traceEnabled = traceEnabled;
  return ExecuteDxilRecipe(recipe, module, dxilModule, options, outContext,
                           outReport);
}

bool ExecuteDxilRecipe(const DxilRecipe &recipe, Module &module,
                       hlsl::DxilModule &dxilModule,
                       const DxilRecipeExecutionOptions &options,
                       DxilRecipeContext *outContext,
                       dxp::PatchReport *outReport) {
  DxilRecipeContext context;
  context.module = &module;
  context.dxilModule = &dxilModule;
  context.entryFunction = dxilModule.GetEntryFunction();
  context.traceEnabled = options.traceEnabled;
  context.inputs = options.inputs;
  context.state = options.initialState;

  if (outReport != nullptr)
    outReport->Steps.clear();
  if (outReport != nullptr)
    outReport->NewBindings.clear();

  for (const DxilRecipeStep &step : recipe.GetSteps()) {
    if (!ShouldExecuteStep(step, context)) {
      if (outReport != nullptr) {
        dxp::PatchStepReport stepReport;
        stepReport.Name = step.name;
        stepReport.Executed = false;
        stepReport.Skipped = true;
        stepReport.Success = true;
        stepReport.Required = step.required;
        outReport->Steps.push_back(std::move(stepReport));
      }
      continue;
    }

    if (!step.execute) {
      context.lastError = "recipe step has no executor: " + step.name;
      if (outContext != nullptr)
        *outContext = context;
      return false;
    }

    const DxilRecipeStepResult result = step.execute(context);
    if (outReport != nullptr) {
      dxp::PatchStepReport stepReport;
      stepReport.Name = step.name;
      stepReport.Executed = true;
      stepReport.Skipped = false;
      stepReport.Success = result.success;
      stepReport.Changed = result.changed;
      stepReport.StopRecipe = result.stopRecipe;
      stepReport.Required = step.required;
      stepReport.MatchCount = result.matchCount;
      stepReport.Rules = result.ruleReports;
      stepReport.SideEffects = result.sideEffects;
      for (auto &sideEffect : stepReport.SideEffects) {
        if (sideEffect.StepName.empty())
          sideEffect.StepName = step.name;
      }
      if (!result.success)
        stepReport.Error = context.lastError;
      AppendBindingExports(*outReport, stepReport.SideEffects);
      outReport->Steps.push_back(std::move(stepReport));
    }
    context.totalRuleMatches += result.matchCount;
    context.moduleModified = context.moduleModified || result.changed;
    context.resourceBindingsChanged =
        context.resourceBindingsChanged || result.resourceBindingsChanged;
    context.resourcesRefreshed =
        context.resourcesRefreshed || result.resourcesRefreshed;
    context.moduleVerified = context.moduleVerified || result.moduleVerified;
    context.entryFunction = dxilModule.GetEntryFunction();
    if (!result.success && step.required) {
      if (outContext != nullptr)
        *outContext = context;
      return false;
    }
    if (result.stopRecipe)
      break;
  }

  if (outContext != nullptr)
    *outContext = context;
  return true;
}