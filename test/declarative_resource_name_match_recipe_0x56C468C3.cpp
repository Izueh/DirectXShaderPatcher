#include "TestSupport.h"

#include <iostream>

#include "llvm/Support/raw_ostream.h"

static bool IsDxOpCall(const llvm::Instruction &instruction,
                       hlsl::OP::OpCode opCode) {
  if (!hlsl::OP::IsDxilOpFuncCallInst(&instruction))
    return false;
  return hlsl::OP::GetDxilOpFuncCallInst(&instruction) == opCode;
}

static bool TryGetConstantStructIntField(const llvm::Value *value,
                                         unsigned fieldIndex,
                                         uint64_t &result) {
  const llvm::Constant *constantValue = llvm::dyn_cast<llvm::Constant>(value);
  if (constantValue == nullptr)
    return false;

  if (llvm::isa<llvm::ConstantAggregateZero>(constantValue)) {
    result = 0;
    return true;
  }

  const llvm::ConstantStruct *constantStruct =
      llvm::dyn_cast<llvm::ConstantStruct>(constantValue);
  if (constantStruct == nullptr ||
      fieldIndex >= constantStruct->getNumOperands())
    return false;

  const llvm::ConstantInt *fieldConstant =
      llvm::dyn_cast<llvm::ConstantInt>(constantStruct->getOperand(fieldIndex));
  if (fieldConstant == nullptr)
    return false;

  result = fieldConstant->getZExtValue();
  return true;
}

static const hlsl::DxilResourceBase *
FindResourceByBinding(hlsl::DxilModule &dxilModule,
                      hlsl::DXIL::ResourceClass resourceClass,
                      unsigned bindPoint, unsigned space) {
  switch (resourceClass) {
  case hlsl::DXIL::ResourceClass::SRV:
    for (const auto &srv : dxilModule.GetSRVs()) {
      if (srv->GetLowerBound() == bindPoint && srv->GetSpaceID() == space)
        return srv.get();
    }
    break;
  case hlsl::DXIL::ResourceClass::UAV:
    for (const auto &uav : dxilModule.GetUAVs()) {
      if (uav->GetLowerBound() == bindPoint && uav->GetSpaceID() == space)
        return uav.get();
    }
    break;
  case hlsl::DXIL::ResourceClass::CBuffer:
    for (const auto &cbuffer : dxilModule.GetCBuffers()) {
      if (cbuffer->GetLowerBound() == bindPoint &&
          cbuffer->GetSpaceID() == space) {
        return cbuffer.get();
      }
    }
    break;
  case hlsl::DXIL::ResourceClass::Sampler:
    for (const auto &sampler : dxilModule.GetSamplers()) {
      if (sampler->GetLowerBound() == bindPoint &&
          sampler->GetSpaceID() == space)
        return sampler.get();
    }
    break;
  default:
    break;
  }

  return nullptr;
}

static const hlsl::DxilResourceBase *
FindResourceByOrdinal(hlsl::DxilModule &dxilModule,
                      hlsl::DXIL::ResourceClass resourceClass,
                      unsigned resourceIndex) {
  switch (resourceClass) {
  case hlsl::DXIL::ResourceClass::SRV: {
    const auto &srvs = dxilModule.GetSRVs();
    return resourceIndex < srvs.size() ? srvs[resourceIndex].get() : nullptr;
  }
  case hlsl::DXIL::ResourceClass::UAV: {
    const auto &uavs = dxilModule.GetUAVs();
    return resourceIndex < uavs.size() ? uavs[resourceIndex].get() : nullptr;
  }
  case hlsl::DXIL::ResourceClass::CBuffer: {
    const auto &cbuffers = dxilModule.GetCBuffers();
    return resourceIndex < cbuffers.size() ? cbuffers[resourceIndex].get()
                                           : nullptr;
  }
  case hlsl::DXIL::ResourceClass::Sampler: {
    const auto &samplers = dxilModule.GetSamplers();
    return resourceIndex < samplers.size() ? samplers[resourceIndex].get()
                                           : nullptr;
  }
  default:
    return nullptr;
  }
}

static bool
TryResolveHandleResource(llvm::Value *value, hlsl::DxilModule &dxilModule,
                         hlsl::DXIL::ResourceClass preferredResourceClass,
                         const hlsl::DxilResourceBase *&resourceOut) {
  resourceOut = nullptr;

  llvm::CallInst *call = llvm::dyn_cast<llvm::CallInst>(value);
  if (call == nullptr)
    return false;

  llvm::CallInst *createHandleCall = call;
  if (IsDxOpCall(*call, hlsl::OP::OpCode::AnnotateHandle))
    createHandleCall = llvm::dyn_cast<llvm::CallInst>(call->getArgOperand(1));

  if (createHandleCall == nullptr ||
      !IsDxOpCall(*createHandleCall,
                  hlsl::OP::OpCode::CreateHandleFromBinding)) {
    return false;
  }

  uint64_t lowerBound = 0;
  uint64_t spaceId = 0;
  uint64_t resourceClassValue = 0;
  if (!TryGetConstantStructIntField(createHandleCall->getArgOperand(1), 0,
                                    lowerBound) ||
      !TryGetConstantStructIntField(createHandleCall->getArgOperand(1), 2,
                                    spaceId) ||
      !TryGetConstantStructIntField(createHandleCall->getArgOperand(1), 3,
                                    resourceClassValue)) {
    return false;
  }

  uint64_t handleIndex = lowerBound;
  if (const llvm::ConstantInt *handleIndexConstant =
          llvm::dyn_cast<llvm::ConstantInt>(
              createHandleCall->getArgOperand(2))) {
    handleIndex = handleIndexConstant->getZExtValue();
  }

  hlsl::DXIL::ResourceClass resolvedResourceClass =
      static_cast<hlsl::DXIL::ResourceClass>(resourceClassValue);
  if (resolvedResourceClass == hlsl::DXIL::ResourceClass::Invalid &&
      preferredResourceClass != hlsl::DXIL::ResourceClass::Invalid) {
    resolvedResourceClass = preferredResourceClass;
  }

  resourceOut = FindResourceByBinding(dxilModule, resolvedResourceClass,
                                      static_cast<unsigned>(handleIndex),
                                      static_cast<unsigned>(spaceId));
  if (resourceOut == nullptr &&
      resolvedResourceClass != hlsl::DXIL::ResourceClass::Invalid) {
    resourceOut = FindResourceByOrdinal(dxilModule, resolvedResourceClass,
                                        static_cast<unsigned>(handleIndex));
  }
  return resourceOut != nullptr;
}

static std::string EscapeRegexLiteral(llvm::StringRef text) {
  std::string escaped;
  escaped.reserve(text.size() * 2);
  for (char ch : text) {
    switch (ch) {
    case '\\':
    case '.':
    case '^':
    case '$':
    case '|':
    case '(':
    case ')':
    case '[':
    case ']':
    case '{':
    case '}':
    case '*':
    case '+':
    case '?':
      escaped.push_back('\\');
      break;
    default:
      break;
    }
    escaped.push_back(ch);
  }
  return escaped;
}

static DxilCallPattern MakeExactTextureLoadPattern() {
  return DxOpCall(hlsl::OP::OpCode::TextureLoad)
      .Capture("texture_load")
      .Args({ResourceHandleOperand(1)
                 .Capture("texture_handle")
                 .ResourceClass(hlsl::DXIL::ResourceClass::SRV)
                 .ResourceKind(hlsl::DXIL::ResourceKind::Texture2D)
                 .ResourceName("BlueNoise_ScalarTexture")
                 .Build(),
             AnyOperand(2).Capture("mip_or_sample").Build(),
             AnyOperand(3).Capture("coord_x").Build(),
             AnyOperand(4).Capture("coord_y").Build(),
             AnyOperand(5).Capture("coord_z").Build(),
             AnyOperand(6).Capture("offset_x").Build(),
             AnyOperand(7).Capture("offset_y").Build(),
             AnyOperand(8).Capture("offset_z").Build()})
      .Build();
}

static DxilCallPattern MakeRegexSampleLevelPattern() {
  return DxOpCall(hlsl::OP::OpCode::SampleLevel)
      .Capture("sample_level")
      .Args({ResourceHandleOperand(1)
                 .Capture("sampled_texture")
                 .ResourceClass(hlsl::DXIL::ResourceClass::SRV)
                 .ResourceNameLike("SceneTexturesStruct_.*Texture")
                 .Build(),
             AnyOperand(2).Capture("sampled_sampler").Build(),
             AnyOperand(3).Capture("coord0").Build(),
             AnyOperand(4).Capture("coord1").Build(),
             AnyOperand(5).Capture("coord2").Build(),
             AnyOperand(6).Capture("coord3").Build(),
             AnyOperand(7).Capture("offset0").Build(),
             AnyOperand(8).Capture("offset1").Build(),
             AnyOperand(9).Capture("offset2").Build(),
             AnyOperand(10).Capture("lod").Build()})
      .Build();
}

static DxilCallPattern MakeBroadSampleLevelPattern() {
  return DxOpCall(hlsl::OP::OpCode::SampleLevel)
      .Capture("sample_level")
      .Args({AnyOperand(1).Capture("sampled_texture").Build(),
             AnyOperand(2).Capture("sampled_sampler").Build(),
             AnyOperand(3).Capture("coord0").Build(),
             AnyOperand(4).Capture("coord1").Build(),
             AnyOperand(5).Capture("coord2").Build(),
             AnyOperand(6).Capture("coord3").Build(),
             AnyOperand(7).Capture("offset0").Build(),
             AnyOperand(8).Capture("offset1").Build(),
             AnyOperand(9).Capture("offset2").Build(),
             AnyOperand(10).Capture("lod").Build()})
      .Build();
}

static DxilCallPattern MakeTypedBufferLoadPattern() {
  return DxOpCall(hlsl::OP::OpCode::BufferLoad)
      .Capture("buffer_load")
      .Args({ResourceHandleOperand(1)
                 .Capture("buffer_handle")
                 .ResourceClass(hlsl::DXIL::ResourceClass::SRV)
                 .ResourceKind(hlsl::DXIL::ResourceKind::TypedBuffer)
                 .ResourceName("VirtualVoxel_PageIndexBuffer")
                 .Build(),
             AnyOperand(2).Capture("index").Build(),
             AnyOperand(3).Capture("wot").Build()})
      .Build();
}

static unsigned CountRuleMatches(llvm::Function &entryFunction,
                                 hlsl::DxilModule &dxilModule,
                                 const DxilCallPattern &pattern) {
  std::vector<DxilMatchResult> matches;
  CollectDxilCallMatches(entryFunction, pattern, matches, &dxilModule);
  return static_cast<unsigned>(matches.size());
}

static std::vector<DxilMatchResult>
CollectMatches(llvm::Function &entryFunction, hlsl::DxilModule &dxilModule,
               const DxilCallPattern &pattern) {
  std::vector<DxilMatchResult> matches;
  CollectDxilCallMatches(entryFunction, pattern, matches, &dxilModule);
  return matches;
}

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "Usage: declarative_resource_name_match_recipe_0x56C468C3 "
                 "<input.cso> <recipe.yml>\n";
    return 1;
  }

  ScopedCoInitialize coinit;

  LoadedDxilShader shader;
  if (!LoadShaderFromPath(argv[1], shader, true))
    return 1;

  llvm::Function *entryFunction = shader.dxilModule->GetEntryFunction();
  if (entryFunction == nullptr) {
    std::cerr << "Failed to locate DXIL entry function.\n";
    return 1;
  }

  const unsigned initialBlueNoiseCount =
      CountBlueNoiseTextureLoads(*entryFunction, *shader.dxilModule);
  const unsigned initialSampleLevelCount =
      CountDxOpCalls(*entryFunction, "dx.op.sampleLevel.f32");
  const unsigned initialBufferLoadCount =
      CountDxOpCalls(*entryFunction, "dx.op.bufferLoad.i32");
  if (initialBlueNoiseCount == 0 || initialSampleLevelCount == 0 ||
      initialBufferLoadCount == 0) {
    std::cerr << "Expected test shader to contain BlueNoise loads, sampleLevel "
                 "calls, and bufferLoad.i32 calls.\n";
    return 1;
  }

  DxilRecipeParseResult parseResult;
  if (!ParseDxilRecipeFile(argv[2], parseResult)) {
    std::cerr << "Failed to parse recipe file: " << parseResult.error << "\n";
    return 1;
  }

  DxilCallPattern exactTextureLoadPattern = MakeExactTextureLoadPattern();
  DxilCallPattern regexSampleLevelPattern = MakeRegexSampleLevelPattern();
  DxilCallPattern typedBufferPattern = MakeTypedBufferLoadPattern();

  const unsigned exactTextureMatches = CountRuleMatches(
      *entryFunction, *shader.dxilModule, exactTextureLoadPattern);
  if (exactTextureMatches == 0) {
    std::cerr << "Expected exact resource_name TextureLoad matcher to find at "
                 "least one match.\n";
    return 1;
  }

  DxilCallPattern wrongExactTexturePattern = exactTextureLoadPattern;
  wrongExactTexturePattern.operandPatterns[0].resourceName =
      "BlueNoise_ScalarTexture_DOES_NOT_EXIST";
  if (CountRuleMatches(*entryFunction, *shader.dxilModule,
                       wrongExactTexturePattern) != 0) {
    std::cerr << "Expected non-matching exact resource_name TextureLoad "
                 "matcher to find zero matches.\n";
    return 1;
  }

  const std::vector<DxilMatchResult> broadSampleMatches = CollectMatches(
      *entryFunction, *shader.dxilModule, MakeBroadSampleLevelPattern());
  if (broadSampleMatches.empty()) {
    std::cerr << "Expected the test shader to contain at least one SampleLevel "
                 "call with Texture2D and Sampler handles.\n";
    return 1;
  }

  const hlsl::DxilResourceBase *sampleTextureResource = nullptr;
  if (!TryResolveHandleResource(
          broadSampleMatches.front().GetCapture("sampled_texture"),
          *shader.dxilModule, hlsl::DXIL::ResourceClass::SRV,
          sampleTextureResource) ||
      sampleTextureResource == nullptr) {
    std::string unresolvedHandleText;
    llvm::raw_string_ostream handleStream(unresolvedHandleText);
    if (llvm::Value *sampledTextureHandle =
            broadSampleMatches.front().GetCapture("sampled_texture")) {
      sampledTextureHandle->print(handleStream);
      if (llvm::CallInst *annotateHandleCall =
              llvm::dyn_cast<llvm::CallInst>(sampledTextureHandle)) {
        handleStream << " ; raw=";
        annotateHandleCall->getArgOperand(1)->print(handleStream);
      }
    } else {
      handleStream << "<null>";
    }
    handleStream.flush();
    std::cerr << "Failed to resolve the SampleLevel texture handle for regex "
                 "matcher validation: "
              << unresolvedHandleText << "\n";
    return 1;
  }

  regexSampleLevelPattern.operandPatterns[0].resourceNameLikePattern =
      "^" + EscapeRegexLiteral(sampleTextureResource->GetGlobalName()) + "$";

  const unsigned regexSampleMatches = CountRuleMatches(
      *entryFunction, *shader.dxilModule, regexSampleLevelPattern);
  if (regexSampleMatches == 0) {
    std::cerr << "Expected resource_name_like SampleLevel matcher to find at "
                 "least one match.\n";
    return 1;
  }

  DxilCallPattern wrongRegexSamplePattern = regexSampleLevelPattern;
  wrongRegexSamplePattern.operandPatterns[0].resourceNameLikePattern =
      "DefinitelyNoSceneTextureMatch";
  if (CountRuleMatches(*entryFunction, *shader.dxilModule,
                       wrongRegexSamplePattern) != 0) {
    std::cerr << "Expected non-matching resource_name_like SampleLevel matcher "
                 "to find zero matches.\n";
    return 1;
  }

  const unsigned typedBufferMatches =
      CountRuleMatches(*entryFunction, *shader.dxilModule, typedBufferPattern);
  if (typedBufferMatches == 0) {
    std::cerr << "Expected TypedBuffer resource matcher to find at least one "
                 "match.\n";
    return 1;
  }

  DxilCallPattern wrongTypedBufferPattern = typedBufferPattern;
  wrongTypedBufferPattern.operandPatterns[0].resourceName =
      "VirtualVoxel_PageIndexBuffer_DOES_NOT_EXIST";
  if (CountRuleMatches(*entryFunction, *shader.dxilModule,
                       wrongTypedBufferPattern) != 0) {
    std::cerr << "Expected non-matching TypedBuffer resource_name matcher to "
                 "find zero matches.\n";
    return 1;
  }

  std::cout << "Declarative resource-name matching parsed and matched exact, "
               "regex, and TypedBuffer resource rules successfully.\n";
  std::cout.flush();
  std::cerr.flush();
  return 0;
}
