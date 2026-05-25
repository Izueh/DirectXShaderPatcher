#include "TestSupport.h"

#include <iostream>
#include <vector>

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/ValueHandle.h"

namespace {

struct IgnNoiseChainMatch {
  llvm::CallInst *finalFrcCall = nullptr;
  llvm::Value *baseX = nullptr;
  llvm::Value *baseY = nullptr;
  bool usesDecorrelatedComponent = false;
};

enum class BlueNoiseTextureKind {
  Scalar,
  Vec2,
};

struct BlueNoiseTextureLoadMatch {
  llvm::CallInst *textureLoadCall = nullptr;
  llvm::Value *coordX = nullptr;
  llvm::Value *coordY = nullptr;
  llvm::Value *originalZ = nullptr;
  bool usesStackedYSlice = false;
  BlueNoiseTextureKind kind = BlueNoiseTextureKind::Scalar;
};

struct ComputeNoiseRewriteSupport {
  llvm::Function *entryFunction = nullptr;
  const hlsl::DxilResource *noiseSrv = nullptr;
  const hlsl::DxilCBuffer *frameIndexCBuffer = nullptr;
  llvm::CallInst *prototypeCreateHandle = nullptr;
  llvm::CallInst *prototypeAnnotateHandle = nullptr;
  llvm::CallInst *prototypeTextureLoad = nullptr;
  llvm::CallInst *prototypeCBufferLoadI32 = nullptr;
  llvm::Value *groupIdX = nullptr;
  llvm::Value *groupIdY = nullptr;
  llvm::Constant *textureResourceProps = nullptr;
  llvm::Constant *cbufferResourceProps = nullptr;
  llvm::Type *createHandleIndexType = nullptr;
  llvm::Type *cbufferLoadIndexType = nullptr;
};

struct MaterializedRewriteResources {
  llvm::Value *annotatedTextureHandle = nullptr;
  llvm::Value *annotatedCBufferHandle = nullptr;
  llvm::Value *frameIndexValue = nullptr;
};

static void PruneTrackedInstructionRoots(
    const std::vector<llvm::WeakTrackingVH> &trackedRoots);

static void TraceMessage(bool enabled, const char *message) {
  if (enabled)
    std::cout << message << std::endl;
}

static bool IsConstantIntValue(const llvm::Value *value,
                               uint64_t expectedValue) {
  const llvm::ConstantInt *constantInt =
      llvm::dyn_cast<llvm::ConstantInt>(value);
  return constantInt != nullptr && constantInt->getZExtValue() == expectedValue;
}

static bool TryGetConstantStructIntField(const llvm::Value *value,
                                         unsigned fieldIndex,
                                         uint64_t &fieldValue) {
  const llvm::Constant *constantValue = llvm::dyn_cast<llvm::Constant>(value);
  if (constantValue == nullptr)
    return false;

  const llvm::ConstantStruct *constantStruct =
      llvm::dyn_cast<llvm::ConstantStruct>(constantValue);
  if (constantStruct == nullptr ||
      fieldIndex >= constantStruct->getNumOperands())
    return false;

  const llvm::ConstantInt *fieldConstant =
      llvm::dyn_cast<llvm::ConstantInt>(constantStruct->getOperand(fieldIndex));
  if (fieldConstant == nullptr)
    return false;

  fieldValue = fieldConstant->getZExtValue();
  return true;
}

static bool TryGetDxilOpCode(const llvm::CallInst &call,
                             hlsl::OP::OpCode &opcode) {
  llvm::Function *callee = call.getCalledFunction();
  if (callee == nullptr)
    return false;

  const llvm::StringRef calleeName = callee->getName();
  if (!calleeName.startswith("dx.op."))
    return false;

  const llvm::ConstantInt *opcodeConstant =
      llvm::dyn_cast<llvm::ConstantInt>(call.getArgOperand(0));
  if (opcodeConstant == nullptr)
    return false;

  opcode = static_cast<hlsl::OP::OpCode>(opcodeConstant->getZExtValue());
  return true;
}

static bool IsDxOpCall(const llvm::CallInst &call,
                       hlsl::OP::OpCode expectedOpcode) {
  hlsl::OP::OpCode actualOpcode;
  return TryGetDxilOpCode(call, actualOpcode) && actualOpcode == expectedOpcode;
}

static llvm::Value *TryGetFAddBaseOperand(llvm::Value *value) {
  llvm::Instruction *instruction = llvm::dyn_cast<llvm::Instruction>(value);
  if (instruction == nullptr ||
      instruction->getOpcode() != llvm::Instruction::FAdd)
    return nullptr;

  const bool lhsIsConstant =
      llvm::isa<llvm::ConstantFP>(instruction->getOperand(0));
  const bool rhsIsConstant =
      llvm::isa<llvm::ConstantFP>(instruction->getOperand(1));
  if (lhsIsConstant == rhsIsConstant)
    return nullptr;

  return lhsIsConstant ? instruction->getOperand(1)
                       : instruction->getOperand(0);
}

static bool TryMatchIgnNoiseChain(llvm::CallInst *call,
                                  IgnNoiseChainMatch &match) {
  if (call == nullptr || !IsDxOpCall(*call, hlsl::OP::OpCode::Frc) ||
      !IsConstantIntValue(call->getArgOperand(0),
                          static_cast<uint64_t>(hlsl::OP::OpCode::Frc))) {
    return false;
  }

  llvm::Instruction *scaledNoise =
      llvm::dyn_cast<llvm::Instruction>(call->getArgOperand(1));
  if (scaledNoise == nullptr ||
      scaledNoise->getOpcode() != llvm::Instruction::FMul)
    return false;

  llvm::Value *preScaleNoise = nullptr;
  const bool lhsIsConstant =
      llvm::isa<llvm::ConstantFP>(scaledNoise->getOperand(0));
  const bool rhsIsConstant =
      llvm::isa<llvm::ConstantFP>(scaledNoise->getOperand(1));
  if (lhsIsConstant == rhsIsConstant)
    return false;

  preScaleNoise =
      lhsIsConstant ? scaledNoise->getOperand(1) : scaledNoise->getOperand(0);

  llvm::CallInst *firstFrcCall = llvm::dyn_cast<llvm::CallInst>(preScaleNoise);
  if (firstFrcCall == nullptr ||
      !IsDxOpCall(*firstFrcCall, hlsl::OP::OpCode::Frc) ||
      !IsConstantIntValue(firstFrcCall->getArgOperand(0),
                          static_cast<uint64_t>(hlsl::OP::OpCode::Frc))) {
    return false;
  }

  llvm::CallInst *dotCall =
      llvm::dyn_cast<llvm::CallInst>(firstFrcCall->getArgOperand(1));
  if (dotCall == nullptr || !IsDxOpCall(*dotCall, hlsl::OP::OpCode::Dot2) ||
      !IsConstantIntValue(dotCall->getArgOperand(0),
                          static_cast<uint64_t>(hlsl::OP::OpCode::Dot2)) ||
      !llvm::isa<llvm::ConstantFP>(dotCall->getArgOperand(3)) ||
      !llvm::isa<llvm::ConstantFP>(dotCall->getArgOperand(4))) {
    return false;
  }

  llvm::Value *decorrelatedBaseX =
      TryGetFAddBaseOperand(dotCall->getArgOperand(1));
  llvm::Value *decorrelatedBaseY =
      TryGetFAddBaseOperand(dotCall->getArgOperand(2));

  if ((decorrelatedBaseX == nullptr) != (decorrelatedBaseY == nullptr))
    return false;

  match.finalFrcCall = call;
  match.baseX = decorrelatedBaseX != nullptr ? decorrelatedBaseX
                                             : dotCall->getArgOperand(1);
  match.baseY = decorrelatedBaseY != nullptr ? decorrelatedBaseY
                                             : dotCall->getArgOperand(2);
  match.usesDecorrelatedComponent = decorrelatedBaseX != nullptr;
  return true;
}

static unsigned
CollectIgnNoiseChains(llvm::Function &function,
                      std::vector<IgnNoiseChainMatch> &matches) {
  matches.clear();

  for (llvm::BasicBlock &basicBlock : function) {
    for (llvm::Instruction &instruction : basicBlock) {
      llvm::CallInst *call = llvm::dyn_cast<llvm::CallInst>(&instruction);
      if (call == nullptr)
        continue;

      IgnNoiseChainMatch match;
      if (!TryMatchIgnNoiseChain(call, match) || call->use_empty())
        continue;

      matches.push_back(match);
    }
  }

  return static_cast<unsigned>(matches.size());
}

static bool HasLiveInstructionUsers(const llvm::Value &value) {
  for (const llvm::User *user : value.users()) {
    const llvm::Instruction *instruction =
        llvm::dyn_cast<llvm::Instruction>(user);
    if (instruction == nullptr || !instruction->use_empty())
      return true;
  }

  return false;
}

static const hlsl::DxilResource *
FindSrvByGlobalName(hlsl::DxilModule &dxilModule, llvm::StringRef name) {
  for (const auto &srv : dxilModule.GetSRVs()) {
    if (srv->GetGlobalName() == name)
      return srv.get();
  }

  return nullptr;
}

static bool TryResolveTextureLoadBinding(llvm::CallInst *textureLoadCall,
                                         unsigned &bindPoint, unsigned &space) {
  if (textureLoadCall == nullptr ||
      !IsDxOpCall(*textureLoadCall, hlsl::OP::OpCode::TextureLoad)) {
    return false;
  }

  llvm::CallInst *annotateHandleCall =
      llvm::dyn_cast<llvm::CallInst>(textureLoadCall->getArgOperand(1));
  if (annotateHandleCall == nullptr ||
      !IsDxOpCall(*annotateHandleCall, hlsl::OP::OpCode::AnnotateHandle)) {
    return false;
  }

  llvm::CallInst *createHandleCall =
      llvm::dyn_cast<llvm::CallInst>(annotateHandleCall->getArgOperand(1));
  if (createHandleCall == nullptr ||
      !IsDxOpCall(*createHandleCall,
                  hlsl::OP::OpCode::CreateHandleFromBinding)) {
    return false;
  }

  uint64_t lowerBound = 0;
  uint64_t spaceId = 0;
  if (!TryGetConstantStructIntField(createHandleCall->getArgOperand(1), 0,
                                    lowerBound) ||
      !TryGetConstantStructIntField(createHandleCall->getArgOperand(1), 2,
                                    spaceId)) {
    return false;
  }

  bindPoint = static_cast<unsigned>(lowerBound);
  space = static_cast<unsigned>(spaceId);
  return true;
}

static bool TryMatchBlueNoiseSliceIndex(llvm::Value *value,
                                        llvm::Value *&sliceIndex) {
  llvm::Instruction *instruction = llvm::dyn_cast<llvm::Instruction>(value);
  if (instruction == nullptr ||
      instruction->getOpcode() != llvm::Instruction::Mul)
    return false;

  for (unsigned operandIndex = 0; operandIndex < 2; ++operandIndex) {
    llvm::Value *candidateSlice = instruction->getOperand(operandIndex);
    llvm::Instruction *candidateInstruction =
        llvm::dyn_cast<llvm::Instruction>(candidateSlice);
    if (candidateInstruction != nullptr &&
        candidateInstruction->getOpcode() == llvm::Instruction::And) {
      sliceIndex = candidateSlice;
      return true;
    }
  }

  return false;
}

static bool TrySplitBlueNoiseStackedYCoordinate(llvm::Value *value,
                                                llvm::Value *&coordY,
                                                llvm::Value *&sliceIndex) {
  llvm::Instruction *instruction = llvm::dyn_cast<llvm::Instruction>(value);
  if (instruction == nullptr ||
      instruction->getOpcode() != llvm::Instruction::Add)
    return false;

  for (unsigned operandIndex = 0; operandIndex < 2; ++operandIndex) {
    llvm::Value *candidateSliceBase = instruction->getOperand(operandIndex);
    llvm::Value *candidateCoordY = instruction->getOperand(1 - operandIndex);
    llvm::Value *resolvedSliceIndex = nullptr;
    if (!TryMatchBlueNoiseSliceIndex(candidateSliceBase, resolvedSliceIndex))
      continue;

    coordY = candidateCoordY;
    sliceIndex = resolvedSliceIndex;
    return true;
  }

  return false;
}

static bool TryMatchBlueNoiseTextureLoad(llvm::CallInst *call,
                                         hlsl::DxilModule &dxilModule,
                                         BlueNoiseTextureLoadMatch &match) {
  if (call == nullptr || !IsDxOpCall(*call, hlsl::OP::OpCode::TextureLoad) ||
      !HasLiveInstructionUsers(*call)) {
    return false;
  }

  const hlsl::DxilResource *blueNoiseScalarTexture =
      FindSrvByGlobalName(dxilModule, "BlueNoise_ScalarTexture");
  const hlsl::DxilResource *blueNoiseVec2Texture =
      FindSrvByGlobalName(dxilModule, "BlueNoise_Vec2Texture");
  if (blueNoiseScalarTexture == nullptr && blueNoiseVec2Texture == nullptr)
    return false;

  unsigned bindPoint = 0;
  unsigned space = 0;
  if (!TryResolveTextureLoadBinding(call, bindPoint, space))
    return false;

  if (blueNoiseScalarTexture != nullptr &&
      blueNoiseScalarTexture->GetLowerBound() == bindPoint &&
      blueNoiseScalarTexture->GetSpaceID() == space) {
    match.kind = BlueNoiseTextureKind::Scalar;
  } else if (blueNoiseVec2Texture != nullptr &&
             blueNoiseVec2Texture->GetLowerBound() == bindPoint &&
             blueNoiseVec2Texture->GetSpaceID() == space) {
    match.kind = BlueNoiseTextureKind::Vec2;
  } else {
    return false;
  }

  llvm::Value *coordY = call->getArgOperand(4);
  llvm::Value *originalZ = call->getArgOperand(5);
  bool usesStackedYSlice = false;

  llvm::Value *stackedCoordY = nullptr;
  llvm::Value *stackedSliceIndex = nullptr;
  if (TrySplitBlueNoiseStackedYCoordinate(call->getArgOperand(4), stackedCoordY,
                                          stackedSliceIndex)) {
    coordY = stackedCoordY;
    originalZ = stackedSliceIndex;
    usesStackedYSlice = true;
  }

  match.textureLoadCall = call;
  match.coordX = call->getArgOperand(3);
  match.coordY = coordY;
  match.originalZ = originalZ;
  match.usesStackedYSlice = usesStackedYSlice;
  return true;
}

static llvm::Value *BuildFastNoiseSliceIndex(llvm::IRBuilder<> &builder,
                                             llvm::Value *frameIndexValue,
                                             llvm::Value *originalZ,
                                             bool usesStackedYSlice) {
  llvm::Value *sliceSource = frameIndexValue;
  if (!usesStackedYSlice && originalZ != nullptr &&
      !IsConstantIntValue(originalZ, 0)) {
    llvm::Value *normalizedOriginalZ = originalZ;
    if (normalizedOriginalZ->getType() != frameIndexValue->getType()) {
      normalizedOriginalZ = builder.CreateIntCast(
          normalizedOriginalZ, frameIndexValue->getType(), false);
    }
    sliceSource = builder.CreateAdd(normalizedOriginalZ, frameIndexValue);
  }

  return builder.CreateURem(
      sliceSource, llvm::ConstantInt::get(frameIndexValue->getType(), 32));
}

static unsigned
CollectBlueNoiseTextureLoads(llvm::Function &function,
                             hlsl::DxilModule &dxilModule,
                             std::vector<BlueNoiseTextureLoadMatch> &matches) {
  matches.clear();

  for (llvm::BasicBlock &basicBlock : function) {
    for (llvm::Instruction &instruction : basicBlock) {
      llvm::CallInst *call = llvm::dyn_cast<llvm::CallInst>(&instruction);
      BlueNoiseTextureLoadMatch match;
      if (!TryMatchBlueNoiseTextureLoad(call, dxilModule, match))
        continue;

      matches.push_back(match);
    }
  }

  return static_cast<unsigned>(matches.size());
}

static llvm::Constant *CreateResBindConstant(llvm::Type *resBindType,
                                             unsigned bindPoint, unsigned space,
                                             unsigned resourceClass) {
  llvm::StructType *resBindStructType =
      llvm::dyn_cast<llvm::StructType>(resBindType);
  if (resBindStructType == nullptr || resBindStructType->getNumElements() != 4)
    return nullptr;

  llvm::Type *lowerType = resBindStructType->getElementType(0);
  llvm::Type *upperType = resBindStructType->getElementType(1);
  llvm::Type *spaceType = resBindStructType->getElementType(2);
  llvm::Type *classType = resBindStructType->getElementType(3);
  llvm::Constant *constants[4] = {
      llvm::ConstantInt::get(lowerType, bindPoint),
      llvm::ConstantInt::get(upperType, bindPoint),
      llvm::ConstantInt::get(spaceType, space),
      llvm::ConstantInt::get(classType, resourceClass),
  };
  return llvm::ConstantStruct::get(resBindStructType, constants);
}

static bool
ResolveComputeNoiseRewriteSupport(hlsl::DxilModule &dxilModule,
                                  const TextureResourceDesc &textureDesc,
                                  const CBufferDesc &frameIndexCBufferDesc,
                                  ComputeNoiseRewriteSupport &support) {
  support = ComputeNoiseRewriteSupport();
  support.entryFunction = dxilModule.GetEntryFunction();
  if (support.entryFunction == nullptr || support.entryFunction->empty()) {
    std::cerr
        << "Failed to locate the DXIL entry function for IGN replacement.\n";
    return false;
  }

  for (const auto &srv : dxilModule.GetSRVs()) {
    if (srv->GetGlobalName() == textureDesc.name) {
      support.noiseSrv = srv.get();
      break;
    }
  }

  for (const auto &cbuffer : dxilModule.GetCBuffers()) {
    if (cbuffer->GetGlobalName() == frameIndexCBufferDesc.name) {
      support.frameIndexCBuffer = cbuffer.get();
      break;
    }
  }

  if (support.noiseSrv == nullptr || support.frameIndexCBuffer == nullptr) {
    std::cerr << "IGN replacement could not resolve the injected texture or "
                 "cbuffer metadata.\n";
    return false;
  }

  for (llvm::BasicBlock &basicBlock : *support.entryFunction) {
    for (llvm::Instruction &instruction : basicBlock) {
      llvm::CallInst *call = llvm::dyn_cast<llvm::CallInst>(&instruction);
      if (call == nullptr)
        continue;

      llvm::Function *callee = call->getCalledFunction();
      if (callee == nullptr)
        continue;

      const llvm::StringRef calleeName = callee->getName();
      if (support.prototypeCreateHandle == nullptr &&
          calleeName == "dx.op.createHandleFromBinding") {
        support.prototypeCreateHandle = call;
      } else if (support.prototypeAnnotateHandle == nullptr &&
                 calleeName == "dx.op.annotateHandle") {
        support.prototypeAnnotateHandle = call;
      } else if (support.prototypeTextureLoad == nullptr &&
                 calleeName == "dx.op.textureLoad.f32") {
        support.prototypeTextureLoad = call;
      } else if (support.prototypeCBufferLoadI32 == nullptr &&
                 calleeName == "dx.op.cbufferLoadLegacy.i32") {
        support.prototypeCBufferLoadI32 = call;
      } else if (calleeName == "dx.op.groupId.i32") {
        if (support.groupIdX == nullptr &&
            IsConstantIntValue(call->getArgOperand(1), 0))
          support.groupIdX = call;
        else if (support.groupIdY == nullptr &&
                 IsConstantIntValue(call->getArgOperand(1), 1))
          support.groupIdY = call;
      }
    }
  }

  if (support.prototypeCreateHandle == nullptr ||
      support.prototypeAnnotateHandle == nullptr ||
      support.prototypeTextureLoad == nullptr ||
      support.prototypeCBufferLoadI32 == nullptr ||
      support.groupIdX == nullptr || support.groupIdY == nullptr ||
      dxilModule.GetShaderModel() == nullptr) {
    std::cerr << "Failed to resolve the compute shader handle/load prototypes "
                 "for IGN replacement.\n";
    return false;
  }

  support.textureResourceProps = hlsl::resource_helper::getAsConstant(
      hlsl::resource_helper::loadPropsFromResourceBase(support.noiseSrv),
      support.prototypeAnnotateHandle->getArgOperand(2)->getType(),
      *dxilModule.GetShaderModel());
  support.cbufferResourceProps = hlsl::resource_helper::getAsConstant(
      hlsl::resource_helper::loadPropsFromResourceBase(
          support.frameIndexCBuffer),
      support.prototypeAnnotateHandle->getArgOperand(2)->getType(),
      *dxilModule.GetShaderModel());
  if (support.textureResourceProps == nullptr ||
      support.cbufferResourceProps == nullptr) {
    std::cerr
        << "Failed to materialize resource properties for IGN replacement.\n";
    return false;
  }

  support.createHandleIndexType =
      support.prototypeCreateHandle->getArgOperand(2)->getType();
  support.cbufferLoadIndexType =
      support.prototypeCBufferLoadI32->getArgOperand(2)->getType();
  return true;
}

static bool
MaterializeRewriteResources(llvm::IRBuilder<> &builder,
                            const ComputeNoiseRewriteSupport &support,
                            const TextureResourceDesc &textureDesc,
                            const CBufferDesc &frameIndexCBufferDesc,
                            MaterializedRewriteResources &resources) {
  resources = MaterializedRewriteResources();

  llvm::Constant *textureResBind = CreateResBindConstant(
      support.prototypeCreateHandle->getArgOperand(1)->getType(),
      textureDesc.binding.GetBindPoint(), textureDesc.binding.GetSpace(), 0);
  llvm::Constant *cbufferResBind = CreateResBindConstant(
      support.prototypeCreateHandle->getArgOperand(1)->getType(),
      frameIndexCBufferDesc.binding.GetBindPoint(),
      frameIndexCBufferDesc.binding.GetSpace(), 2);
  if (textureResBind == nullptr || cbufferResBind == nullptr)
    return false;

  llvm::Value *rawTextureHandle = builder.CreateCall(
      support.prototypeCreateHandle->getCalledFunction(),
      {support.prototypeCreateHandle->getArgOperand(0), textureResBind,
       llvm::ConstantInt::get(
           llvm::cast<llvm::IntegerType>(support.createHandleIndexType),
           textureDesc.binding.GetBindPoint()),
       support.prototypeCreateHandle->getArgOperand(3)});
  resources.annotatedTextureHandle =
      builder.CreateCall(support.prototypeAnnotateHandle->getCalledFunction(),
                         {support.prototypeAnnotateHandle->getArgOperand(0),
                          rawTextureHandle, support.textureResourceProps});

  llvm::Value *rawCBufferHandle = builder.CreateCall(
      support.prototypeCreateHandle->getCalledFunction(),
      {support.prototypeCreateHandle->getArgOperand(0), cbufferResBind,
       llvm::ConstantInt::get(
           llvm::cast<llvm::IntegerType>(support.createHandleIndexType),
           frameIndexCBufferDesc.binding.GetBindPoint()),
       support.prototypeCreateHandle->getArgOperand(3)});
  resources.annotatedCBufferHandle =
      builder.CreateCall(support.prototypeAnnotateHandle->getCalledFunction(),
                         {support.prototypeAnnotateHandle->getArgOperand(0),
                          rawCBufferHandle, support.cbufferResourceProps});

  llvm::Value *frameIndexLoad = builder.CreateCall(
      support.prototypeCBufferLoadI32->getCalledFunction(),
      {support.prototypeCBufferLoadI32->getArgOperand(0),
       resources.annotatedCBufferHandle,
       llvm::ConstantInt::get(
           llvm::cast<llvm::IntegerType>(support.cbufferLoadIndexType), 0)});
  resources.frameIndexValue = builder.CreateExtractValue(frameIndexLoad, 0);
  return true;
}

static llvm::Value *CreateFastNoiseTextureLoad(
    llvm::IRBuilder<> &builder, const ComputeNoiseRewriteSupport &support,
    llvm::Value *annotatedTextureHandle, llvm::Value *coordX,
    llvm::Value *coordY, llvm::Value *sliceIndex) {
  return builder.CreateCall(
      support.prototypeTextureLoad->getCalledFunction(),
      {support.prototypeTextureLoad->getArgOperand(0), annotatedTextureHandle,
       llvm::ConstantInt::get(
           llvm::cast<llvm::IntegerType>(
               support.prototypeTextureLoad->getArgOperand(2)->getType()),
           0),
       coordX, coordY, sliceIndex,
       llvm::UndefValue::get(
           support.prototypeTextureLoad->getArgOperand(6)->getType()),
       llvm::UndefValue::get(
           support.prototypeTextureLoad->getArgOperand(7)->getType()),
       llvm::UndefValue::get(
           support.prototypeTextureLoad->getArgOperand(8)->getType())});
}

static bool
BuildComputeNoiseRewriteRules(hlsl::DxilModule &dxilModule,
                              const TextureResourceDesc &textureDesc,
                              const CBufferDesc &frameIndexCBufferDesc,
                              std::vector<DxilRewriteRule> &rules) {
  ComputeNoiseRewriteSupport support;
  if (!ResolveComputeNoiseRewriteSupport(dxilModule, textureDesc,
                                         frameIndexCBufferDesc, support)) {
    return false;
  }

  const ComputeNoiseRewriteSupport capturedSupport = support;
  const TextureResourceDesc capturedTextureDesc = textureDesc;
  const CBufferDesc capturedFrameIndexCBufferDesc = frameIndexCBufferDesc;

  auto buildIgnRule = [&](const char *ruleName,
                          unsigned nestedNoiseOperandIndex,
                          unsigned scaleOperandIndex) {
    return RewriteRule(ruleName)
        .Mode(DxilRewriteMode::Replace)
        .ReplaceCapture("ign_root")
        .PruneDeadInstructions(true)
        .Match(
            DxOpCall(hlsl::OP::OpCode::Frc)
                .Capture("ign_root")
                .Args({
                    ConstantIntOperand(
                        0, static_cast<uint64_t>(hlsl::OP::OpCode::Frc)),
                    InstructionOperand(1, llvm::Instruction::FMul)
                        .Capture("outer_mul")
                        .Args({
                            DxOpOperand(nestedNoiseOperandIndex,
                                        hlsl::OP::OpCode::Frc)
                                .Capture("inner_frc")
                                .Args({
                                    ConstantIntOperand(
                                        0, static_cast<uint64_t>(
                                               hlsl::OP::OpCode::Frc)),
                                    DxOpOperand(1, hlsl::OP::OpCode::Dot2)
                                        .Capture("dot_call")
                                        .Args({
                                            ConstantIntOperand(
                                                0, static_cast<uint64_t>(
                                                       hlsl::OP::OpCode::Dot2)),
                                            AnyOperand(1).Capture("raw_x"),
                                            AnyOperand(2).Capture("raw_y"),
                                            AnyOperand(3).Capture("dot_c0"),
                                            AnyOperand(4).Capture("dot_c1"),
                                        }),
                                }),
                            AnyOperand(scaleOperandIndex).Capture("scale"),
                        }),
                }))
        .Where([](const DxilMatchResult &match) {
          return llvm::isa<llvm::ConstantFP>(match.GetCapture("scale")) &&
                 llvm::isa<llvm::ConstantFP>(match.GetCapture("dot_c0")) &&
                 llvm::isa<llvm::ConstantFP>(match.GetCapture("dot_c1"));
        })
        .Callback([capturedSupport, capturedTextureDesc,
                   capturedFrameIndexCBufferDesc](
                      const DxilMatchResult &match, llvm::IRBuilder<> &builder,
                      llvm::Module &, hlsl::DxilModule &) -> DxilRewriteResult {
          MaterializedRewriteResources resources;
          if (!MaterializeRewriteResources(
                  builder, capturedSupport, capturedTextureDesc,
                  capturedFrameIndexCBufferDesc, resources)) {
            return DxilRewriteResult{false};
          }

          llvm::Value *rawX = match.GetCapture("raw_x");
          llvm::Value *rawY = match.GetCapture("raw_y");
          const bool usesDecorrelatedComponent =
              TryGetFAddBaseOperand(rawX) != nullptr ||
              TryGetFAddBaseOperand(rawY) != nullptr;

          llvm::Value *sliceIndex = builder.CreateURem(
              resources.frameIndexValue,
              llvm::ConstantInt::get(resources.frameIndexValue->getType(), 32));
          llvm::Value *coordX = builder.CreateURem(
              capturedSupport.groupIdX,
              llvm::ConstantInt::get(capturedSupport.groupIdX->getType(), 128));
          llvm::Value *coordY = builder.CreateURem(
              capturedSupport.groupIdY,
              llvm::ConstantInt::get(capturedSupport.groupIdY->getType(), 128));
          llvm::Value *noiseLoad = CreateFastNoiseTextureLoad(
              builder, capturedSupport, resources.annotatedTextureHandle,
              coordX, coordY, sliceIndex);

          DxilRewriteResult result;
          result.replacementValue = builder.CreateExtractValue(
              noiseLoad, usesDecorrelatedComponent ? 1u : 0u);
          return result;
        });
  };

  DxilRewriteRule blueNoiseRule =
      RewriteRule("blue_noise_textureload_to_fastnoise")
          .Mode(DxilRewriteMode::Replace)
          .ReplaceCapture("texture_load")
          .PruneDeadInstructions(false)
          .Match(DxOpCall(hlsl::OP::OpCode::TextureLoad)
                     .Capture("texture_load")
                     .Args({
                         ResourceHandleOperand(1)
                             .Capture("handle")
                             .ResourceClass(hlsl::DXIL::ResourceClass::SRV)
                             .ResourceKind(hlsl::DXIL::ResourceKind::Texture2D),
                         AnyOperand(3).Capture("coord_x"),
                         AnyOperand(4).Capture("coord_y"),
                         AnyOperand(5).Capture("coord_z"),
                     }))
          .Where([&](const DxilMatchResult &match) {
            BlueNoiseTextureLoadMatch blueNoiseMatch;
            return TryMatchBlueNoiseTextureLoad(match.rootCall, dxilModule,
                                                blueNoiseMatch);
          })
          .Callback([capturedSupport, capturedTextureDesc,
                     capturedFrameIndexCBufferDesc,
                     &dxilModule](const DxilMatchResult &match,
                                  llvm::IRBuilder<> &builder, llvm::Module &,
                                  hlsl::DxilModule &) -> DxilRewriteResult {
            BlueNoiseTextureLoadMatch blueNoiseMatch;
            if (!TryMatchBlueNoiseTextureLoad(match.rootCall, dxilModule,
                                              blueNoiseMatch)) {
              return DxilRewriteResult{false};
            }

            MaterializedRewriteResources resources;
            if (!MaterializeRewriteResources(
                    builder, capturedSupport, capturedTextureDesc,
                    capturedFrameIndexCBufferDesc, resources)) {
              return DxilRewriteResult{false};
            }

            llvm::Value *sliceIndex = BuildFastNoiseSliceIndex(
                builder, resources.frameIndexValue, blueNoiseMatch.originalZ,
                blueNoiseMatch.usesStackedYSlice);
            llvm::Value *noiseLoad = CreateFastNoiseTextureLoad(
                builder, capturedSupport, resources.annotatedTextureHandle,
                blueNoiseMatch.coordX, blueNoiseMatch.coordY, sliceIndex);

            std::vector<llvm::WeakTrackingVH> pruneRoots;
            std::vector<llvm::User *> loadUsers(match.rootCall->user_begin(),
                                                match.rootCall->user_end());
            for (llvm::User *user : loadUsers) {
              llvm::ExtractValueInst *extractValue =
                  llvm::dyn_cast<llvm::ExtractValueInst>(user);
              if (extractValue == nullptr || extractValue->getNumIndices() != 1)
                continue;

              const unsigned extractIndex = *extractValue->idx_begin();
              if (extractIndex > 1) {
                if (!extractValue->use_empty())
                  return DxilRewriteResult{false};
                pruneRoots.emplace_back(extractValue);
                continue;
              }

              llvm::Value *replacementComponent =
                  builder.CreateExtractValue(noiseLoad, extractIndex);
              extractValue->replaceAllUsesWith(replacementComponent);
              pruneRoots.emplace_back(extractValue);
            }

            pruneRoots.emplace_back(match.rootCall);
            PruneTrackedInstructionRoots(pruneRoots);

            DxilRewriteResult result;
            result.handledReplacement = true;
            return result;
          });

  rules.clear();
  rules.push_back(buildIgnRule("ign_noise_lhs_constant", 1, 0));
  rules.push_back(buildIgnRule("ign_noise_rhs_constant", 0, 1));
  rules.push_back(std::move(blueNoiseRule));
  return true;
}

static void PruneTrackedInstructionRoots(
    const std::vector<llvm::WeakTrackingVH> &trackedRoots) {
  for (const llvm::WeakTrackingVH &candidateHandle : trackedRoots) {
    llvm::Instruction *candidate = llvm::dyn_cast_or_null<llvm::Instruction>(
        static_cast<llvm::Value *>(candidateHandle));
    if (candidate == nullptr)
      continue;

    PruneInstructionRoots({candidate});
  }
}

} // namespace

unsigned CountIgnNoiseChains(llvm::Function &function) {
  std::vector<IgnNoiseChainMatch> matches;
  return CollectIgnNoiseChains(function, matches);
}

unsigned CountBlueNoiseTextureLoads(llvm::Function &function,
                                    hlsl::DxilModule &dxilModule) {
  std::vector<BlueNoiseTextureLoadMatch> matches;
  return CollectBlueNoiseTextureLoads(function, dxilModule, matches);
}

bool ReplaceIgnNoiseInComputeShaderWithTextureLoad(
    llvm::Module &module, hlsl::DxilModule &dxilModule,
    const TextureResourceDesc &textureDesc,
    const CBufferDesc &frameIndexCBufferDesc, bool traceEnabled) {
  if (textureDesc.kind != hlsl::DXIL::ResourceKind::Texture2DArray ||
      textureDesc.elementKind != hlsl::DXIL::ComponentType::F32 ||
      textureDesc.vectorWidth < 2 || textureDesc.isReadWrite) {
    std::cerr << "IGN replacement requires a Texture2DArray<float2+> SRV.\n";
    return false;
  }

  ComputeNoiseRewriteSupport support;
  if (!ResolveComputeNoiseRewriteSupport(dxilModule, textureDesc,
                                         frameIndexCBufferDesc, support)) {
    return false;
  }

  std::vector<IgnNoiseChainMatch> ignMatches;
  const unsigned ignMatchCount =
      CollectIgnNoiseChains(*support.entryFunction, ignMatches);

  std::vector<BlueNoiseTextureLoadMatch> blueNoiseMatches;
  const unsigned blueNoiseMatchCount = CollectBlueNoiseTextureLoads(
      *support.entryFunction, dxilModule, blueNoiseMatches);

  if (ignMatchCount == 0 && blueNoiseMatchCount == 0) {
    std::cerr << "Failed to locate any IGN or BlueNoise patterns to replace.\n";
    return false;
  }

  std::vector<llvm::WeakTrackingVH> deadInstructionRoots;

  for (const IgnNoiseChainMatch &ignMatch : ignMatches) {
    llvm::IRBuilder<> builder(ignMatch.finalFrcCall);

    MaterializedRewriteResources resources;
    if (!MaterializeRewriteResources(builder, support, textureDesc,
                                     frameIndexCBufferDesc, resources)) {
      std::cerr << "Failed to build resource binding constants for IGN "
                   "replacement.\n";
      return false;
    }

    llvm::Value *sliceIndex = builder.CreateURem(
        resources.frameIndexValue,
        llvm::ConstantInt::get(resources.frameIndexValue->getType(), 32));
    llvm::Value *coordX = builder.CreateURem(
        support.groupIdX,
        llvm::ConstantInt::get(support.groupIdX->getType(), 128));
    llvm::Value *coordY = builder.CreateURem(
        support.groupIdY,
        llvm::ConstantInt::get(support.groupIdY->getType(), 128));

    llvm::Value *noiseLoad = CreateFastNoiseTextureLoad(
        builder, support, resources.annotatedTextureHandle, coordX, coordY,
        sliceIndex);

    llvm::Value *replacementNoise = builder.CreateExtractValue(
        noiseLoad, ignMatch.usesDecorrelatedComponent ? 1u : 0u);
    ignMatch.finalFrcCall->replaceAllUsesWith(replacementNoise);
    deadInstructionRoots.emplace_back(ignMatch.finalFrcCall);
  }

  for (const BlueNoiseTextureLoadMatch &blueNoiseMatch : blueNoiseMatches) {
    llvm::IRBuilder<> builder(blueNoiseMatch.textureLoadCall);

    MaterializedRewriteResources resources;
    if (!MaterializeRewriteResources(builder, support, textureDesc,
                                     frameIndexCBufferDesc, resources)) {
      std::cerr << "Failed to build resource binding constants for BlueNoise "
                   "replacement.\n";
      return false;
    }

    llvm::Value *sliceIndex = BuildFastNoiseSliceIndex(
        builder, resources.frameIndexValue, blueNoiseMatch.originalZ,
        blueNoiseMatch.usesStackedYSlice);
    llvm::Value *noiseLoad = CreateFastNoiseTextureLoad(
        builder, support, resources.annotatedTextureHandle,
        blueNoiseMatch.coordX, blueNoiseMatch.coordY, sliceIndex);

    std::vector<llvm::User *> loadUsers(
        blueNoiseMatch.textureLoadCall->user_begin(),
        blueNoiseMatch.textureLoadCall->user_end());
    for (llvm::User *user : loadUsers) {
      llvm::ExtractValueInst *extractValue =
          llvm::dyn_cast<llvm::ExtractValueInst>(user);
      if (extractValue == nullptr || extractValue->getNumIndices() != 1)
        continue;

      const unsigned extractIndex = *extractValue->idx_begin();
      if (extractIndex > 1) {
        if (!extractValue->use_empty()) {
          std::cerr << "BlueNoise replacement encountered a live component "
                       "outside FASTNoiseTexture.xy.\n";
          return false;
        }
        deadInstructionRoots.emplace_back(extractValue);
        continue;
      }

      llvm::Value *replacementComponent =
          builder.CreateExtractValue(noiseLoad, extractIndex);
      extractValue->replaceAllUsesWith(replacementComponent);
      deadInstructionRoots.emplace_back(extractValue);
    }

    deadInstructionRoots.emplace_back(blueNoiseMatch.textureLoadCall);
  }

  PruneTrackedInstructionRoots(deadInstructionRoots);

  TraceMessage(traceEnabled, "compute IGN replacement: replaced IGN and "
                             "BlueNoise patterns with Texture2DArray.Load");
  return true;
}

bool ReplaceIgnNoiseInComputeShaderWithTextureLoadUsingRules(
    llvm::Module &module, hlsl::DxilModule &dxilModule,
    const TextureResourceDesc &textureDesc,
    const CBufferDesc &frameIndexCBufferDesc, bool traceEnabled) {
  if (textureDesc.kind != hlsl::DXIL::ResourceKind::Texture2DArray ||
      textureDesc.elementKind != hlsl::DXIL::ComponentType::F32 ||
      textureDesc.vectorWidth < 2 || textureDesc.isReadWrite) {
    std::cerr << "IGN replacement requires a Texture2DArray<float2+> SRV.\n";
    return false;
  }

  ComputeNoiseRewriteSupport support;
  if (!ResolveComputeNoiseRewriteSupport(dxilModule, textureDesc,
                                         frameIndexCBufferDesc, support)) {
    return false;
  }

  const unsigned ignMatchCount = CountIgnNoiseChains(*support.entryFunction);
  const unsigned blueNoiseMatchCount =
      CountBlueNoiseTextureLoads(*support.entryFunction, dxilModule);
  if (ignMatchCount == 0 && blueNoiseMatchCount == 0) {
    std::cerr << "Failed to locate any IGN or BlueNoise patterns to replace.\n";
    return false;
  }

  std::vector<DxilRewriteRule> rules;
  if (!BuildComputeNoiseRewriteRules(dxilModule, textureDesc,
                                     frameIndexCBufferDesc, rules)) {
    return false;
  }

  unsigned appliedRuleCount = 0;
  if (!ApplyDxilRewriteRules(*support.entryFunction, module, dxilModule, rules,
                             &appliedRuleCount)) {
    return false;
  }

  if (appliedRuleCount == 0) {
    std::cerr << "Failed to apply any IGN or BlueNoise rewrite rules.\n";
    return false;
  }

  TraceMessage(traceEnabled,
               "compute IGN replacement: rule-based rewrite replaced IGN and "
               "BlueNoise patterns with Texture2DArray.Load");
  return true;
}