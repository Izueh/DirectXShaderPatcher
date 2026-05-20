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

// NOLINTBEGIN(llvm-prefer-static-over-anonymous-namespace)
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
  if (mode == DxilRecipeRuleApplicationMode::MatchAll) {
    if (!ApplyDxilRewriteRulesMatchAll(*context.entryFunction, *context.module,
                                       *context.dxilModule, rules,
                                       &totalMatches)) {
      return FailRecipeStep(
          context, stepName + ": rewrite rule application failed");
    }
  } else {
    if (!ApplyDxilRewriteRulesOnce(*context.entryFunction, *context.module,
                                   *context.dxilModule, rules,
                                   mode == DxilRecipeRuleApplicationMode::Last,
                                   &totalMatches)) {
      return FailRecipeStep(
          context, stepName + ": rewrite rule application failed");
    }
  }

  if (required && totalMatches == 0) {
    return FailRecipeStep(context,
                          stepName + ": no rewrite matches were applied");
  }

  DxilRecipeStepResult result;
  result.changed = totalMatches != 0;
  result.matchCount = totalMatches;
  result.invalidatedAnalyses = totalMatches != 0;
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
                        const std::string &stepName) {
  if (context.dxilModule == nullptr) {
    return FailRecipeStep(context,
                          stepName + ": recipe context is missing DXIL module");
  }

  context.entryFunction = context.dxilModule->GetEntryFunction();
  if (context.entryFunction == nullptr) {
    return FailRecipeStep(context,
                          stepName + ": failed to locate DXIL entry function");
  }

  for (const DxilCallPattern &pattern : patterns) {
    if (CountPrefilterMatches(*context.entryFunction, *context.dxilModule,
                              pattern) != 0) {
      return MakeRecipeStepSuccess();
    }
  }

  AppendRecipeDiagnostic(context, stepName +
                                      ": prefilter did not match; skipping remaining recipe steps");
  return MakeRecipeStepSuccess(false, 0, false, true);
}

} // namespace
// NOLINTEND(llvm-prefer-static-over-anonymous-namespace)

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
  return DxilRecipeStep{std::move(name), std::move(execute)};
}

DxilRecipeStep MakeAddTextureStep(std::string id, TextureResourceDesc desc) {
  return DxilRecipeStep{
      "add_texture:" + id,
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
        return result;
      }};
}

DxilRecipeStep MakeAddTextureUAVStep(std::string id, TextureResourceDesc desc) {
  return DxilRecipeStep{
      "add_texture_uav:" + id,
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
        return result;
      }};
}

DxilRecipeStep MakeAddCBufferStep(std::string id, CBufferDesc desc) {
  std::shared_ptr<CBufferSchema> schemaCopy;
  if (desc.schema != nullptr)
    schemaCopy = std::make_shared<CBufferSchema>(*desc.schema);

  return DxilRecipeStep{
      "add_cbuffer:" + id,
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
        return result;
      }};
}

DxilRecipeStep MakeAddSamplerStep(std::string id, SamplerDesc desc) {
  return DxilRecipeStep{
      "add_sampler:" + id,
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
        return result;
      }};
}

DxilRecipeStep MakeApplyRewriteRulesStep(std::string name,
                                         std::vector<DxilRewriteRule> rules,
                                         DxilRecipeRuleApplicationMode mode,
                                         bool required) {
  return DxilRecipeStep{name, [rules = std::move(rules), mode, required,
                               name](DxilRecipeContext &context) {
                          return ApplyRecipeRewriteRules(context, rules, mode,
                                                         required, name);
                        }};
}

DxilRecipeStep MakePrefilterStep(std::string name,
                                 std::vector<DxilCallPattern> patterns) {
  return DxilRecipeStep{name, [patterns = std::move(patterns),
                               name](DxilRecipeContext &context) {
                          return EvaluateRecipePrefilter(context, patterns,
                                                         name);
                        }};
}

DxilRecipeStep MakeRefreshResourcesStep(std::string name) {
  return DxilRecipeStep{
      name, [name](DxilRecipeContext &context) {
        if (context.dxilModule == nullptr) {
          return FailRecipeStep(
              context, name + ": recipe context is missing DXIL module");
        }

        RefreshDxilAfterResourceMutation(*context.dxilModule,
                                         context.traceEnabled);
        DxilRecipeStepResult result;
        result.changed = true;
        result.resourcesRefreshed = true;
        return result;
      }};
}

DxilRecipeStep MakePruneDeadCodeStep(std::string name) {
  return DxilRecipeStep{
      name, [name](DxilRecipeContext &context) {
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

        // Refresh OP cache after pruning — pruning may have deleted
        // DXIL op function call instructions, leaving stale pointers
        // in the cache that cause crashes during module destruction.
        {
          hlsl::OP *op = context.dxilModule->GetOP();
          if (op)
            op->RefreshCache();
        }

        return MakeRecipeStepSuccess(true, 0, true);
      }};
}

DxilRecipeStep MakeVerifyModuleStep(std::string name) {
  return DxilRecipeStep{
      name, [name](DxilRecipeContext &context) {
        if (context.module == nullptr) {
          return FailRecipeStep(context,
                                name + ": recipe context is missing module");
        }

        std::string verifyErrors;
        llvm::raw_string_ostream errorStream(verifyErrors);
        if (llvm::verifyModule(*context.module, &errorStream)) {
          errorStream.flush();
          return FailRecipeStep(
              context, name + ": module verification failed: " + verifyErrors);
        }

        DxilRecipeStepResult result;
        result.moduleVerified = true;
        return result;
      }};
}

DxilRecipeStep MakeExpectTextureStep(std::string id, std::string name) {
  return DxilRecipeStep{
      name, [id = std::move(id), name](DxilRecipeContext &context) {
        if (context.dxilModule == nullptr) {
          return FailRecipeStep(
              context, name + ": recipe context is missing DXIL module");
        }

        auto textureIt = context.textures.find(id);
        if (textureIt == context.textures.end()) {
          return FailRecipeStep(context,
                                name + ": unknown texture id '" + id + "'");
        }

        const TextureResourceDesc &desc = textureIt->second;
        const hlsl::DxilResourceBase *resource = FindResourceByBinding(
            *context.dxilModule, hlsl::DXIL::ResourceClass::SRV,
            desc.binding.GetBindPoint(), desc.binding.GetSpace());
        if (resource == nullptr || resource->GetGlobalName() != desc.name) {
          return FailRecipeStep(context, name +
                                             ": expected texture resource '" +
                                             desc.name + "' is missing");
        }

        return DxilRecipeStepResult{};
      }};
}

DxilRecipeStep MakeExpectTextureUAVStep(std::string id, std::string name) {
  return DxilRecipeStep{
      name, [id = std::move(id), name](DxilRecipeContext &context) {
        if (context.dxilModule == nullptr) {
          return FailRecipeStep(
              context, name + ": recipe context is missing DXIL module");
        }

        auto textureIt = context.uavs.find(id);
        if (textureIt == context.uavs.end()) {
          return FailRecipeStep(context,
                                name + ": unknown UAV id '" + id + "'");
        }

        const TextureResourceDesc &desc = textureIt->second;
        const hlsl::DxilResourceBase *resource = FindResourceByBinding(
            *context.dxilModule, hlsl::DXIL::ResourceClass::UAV,
            desc.binding.GetBindPoint(), desc.binding.GetSpace());
        if (resource == nullptr || resource->GetGlobalName() != desc.name) {
          return FailRecipeStep(context, name + ": expected texture UAV '" +
                                             desc.name + "' is missing");
        }

        return DxilRecipeStepResult{};
      }};
}

DxilRecipeStep MakeExpectCBufferStep(std::string id, std::string name) {
  return DxilRecipeStep{
      name, [id = std::move(id), name](DxilRecipeContext &context) {
        if (context.dxilModule == nullptr) {
          return FailRecipeStep(
              context, name + ": recipe context is missing DXIL module");
        }

        auto cbufferIt = context.cbuffers.find(id);
        if (cbufferIt == context.cbuffers.end()) {
          return FailRecipeStep(context,
                                name + ": unknown cbuffer id '" + id + "'");
        }

        const CBufferDesc &desc = cbufferIt->second;
        const hlsl::DxilResourceBase *resource = FindResourceByBinding(
            *context.dxilModule, hlsl::DXIL::ResourceClass::CBuffer,
            desc.binding.GetBindPoint(), desc.binding.GetSpace());
        if (resource == nullptr || resource->GetGlobalName() != desc.name) {
          return FailRecipeStep(context, name + ": expected cbuffer '" +
                                             desc.name + "' is missing");
        }

        return DxilRecipeStepResult{};
      }};
}

bool ExecuteDxilRecipe(const DxilRecipe &recipe, Module &module,
                       hlsl::DxilModule &dxilModule,
                       DxilRecipeContext *outContext, bool traceEnabled) {
  DxilRecipeExecutionOptions options;
  options.traceEnabled = traceEnabled;
  return ExecuteDxilRecipe(recipe, module, dxilModule, options, outContext);
}

bool ExecuteDxilRecipe(const DxilRecipe &recipe, Module &module,
                       hlsl::DxilModule &dxilModule,
                       const DxilRecipeExecutionOptions &options,
                       DxilRecipeContext *outContext) {
  DxilRecipeContext context;
  context.module = &module;
  context.dxilModule = &dxilModule;
  context.entryFunction = dxilModule.GetEntryFunction();
  context.traceEnabled = options.traceEnabled;
  context.inputs = options.inputs;
  context.state = options.initialState;

  for (const DxilRecipeStep &step : recipe.GetSteps()) {
    if (!step.execute) {
      context.lastError = "recipe step has no executor: " + step.name;
      if (outContext != nullptr)
        *outContext = context;
      return false;
    }

    const DxilRecipeStepResult result = step.execute(context);
    context.totalRuleMatches += result.matchCount;
    context.moduleModified = context.moduleModified || result.changed;
    context.resourceBindingsChanged =
      context.resourceBindingsChanged || result.resourceBindingsChanged;
    context.resourcesRefreshed =
      context.resourcesRefreshed || result.resourcesRefreshed;
    context.moduleVerified = context.moduleVerified || result.moduleVerified;
    context.entryFunction = dxilModule.GetEntryFunction();
    if (!result.success) {
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