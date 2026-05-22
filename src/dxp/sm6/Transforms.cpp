#include "../../../include/dxp/sm6/Transforms.h"
#include "../../../include/dxp/sm6/Resources.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Use.h"
#include "llvm/IR/ValueHandle.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Regex.h"
#include "llvm/Transforms/Utils/Local.h"

#include "dxc/DXIL/DxilConstants.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilResourceBase.h"
#include "dxc/DXIL/DxilResourceBinding.h"
#include "dxc/DXIL/DxilResourceProperties.h"

using llvm::Module;

// NOLINTBEGIN(llvm-prefer-static-over-anonymous-namespace)
namespace {

static void TraceMessage(bool traceEnabled, const char *message) {
  if (traceEnabled)
    std::cerr << "[trace] " << message << "\n";
}

static llvm::Type *GetDxilScalarType(llvm::LLVMContext &context,
                                     hlsl::DXIL::ComponentType componentType) {
  switch (componentType) {
  case hlsl::DXIL::ComponentType::F32:
    return llvm::Type::getFloatTy(context);
  case hlsl::DXIL::ComponentType::U32:
  case hlsl::DXIL::ComponentType::I32:
    return llvm::Type::getInt32Ty(context);
  case hlsl::DXIL::ComponentType::Invalid:
    return nullptr;
  default:
    return nullptr;
  }
}

static llvm::Type *GetEmitValueScalarType(const DxilRewriteEmitValue &value,
                                          llvm::LLVMContext &context,
                                          llvm::Type *fallbackType) {
  if (!value.hasExplicitResultComponentType)
    return fallbackType;

  return GetDxilScalarType(context, value.resultComponentType);
}

static bool IsDxOpCall(const llvm::Instruction &instruction,
                       llvm::StringRef functionName) {
  const llvm::CallInst *call = llvm::dyn_cast<llvm::CallInst>(&instruction);
  const llvm::Function *callee =
      call != nullptr ? call->getCalledFunction() : nullptr;
  return callee != nullptr && callee->getName() == functionName;
}

static bool TryGetDxilOpCode(const llvm::Instruction &instruction,
                             hlsl::OP::OpCode &opCode) {
  if (!hlsl::OP::IsDxilOpFuncCallInst(&instruction))
    return false;

  opCode = hlsl::OP::GetDxilOpFuncCallInst(&instruction);
  return true;
}

static bool IsDxOpCall(const llvm::Instruction &instruction,
                       hlsl::OP::OpCode opCode) {
  hlsl::OP::OpCode actualOpCode = static_cast<hlsl::OP::OpCode>(0);
  return TryGetDxilOpCode(instruction, actualOpCode) && actualOpCode == opCode;
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

  if (llvm::isa<llvm::ConstantAggregateZero>(constantValue)) {
    fieldValue = 0;
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

  fieldValue = fieldConstant->getZExtValue();
  return true;
}

static const hlsl::DxilResourceBase *
FindResourceByBinding(hlsl::DxilModule &dxilModule,
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

static const hlsl::DxilResourceBase *
FindResourceByOrdinal(hlsl::DxilModule &dxilModule,
                      hlsl::DXIL::ResourceClass resourceClass,
                      unsigned resourceIndex) {
  switch (resourceClass) {
  case hlsl::DXIL::ResourceClass::SRV: {
    const auto &resources = dxilModule.GetSRVs();
    return resourceIndex < resources.size() ? resources[resourceIndex].get()
                                            : nullptr;
  }
  case hlsl::DXIL::ResourceClass::UAV: {
    const auto &resources = dxilModule.GetUAVs();
    return resourceIndex < resources.size() ? resources[resourceIndex].get()
                                            : nullptr;
  }
  case hlsl::DXIL::ResourceClass::CBuffer: {
    const auto &resources = dxilModule.GetCBuffers();
    return resourceIndex < resources.size() ? resources[resourceIndex].get()
                                            : nullptr;
  }
  case hlsl::DXIL::ResourceClass::Sampler: {
    const auto &resources = dxilModule.GetSamplers();
    return resourceIndex < resources.size() ? resources[resourceIndex].get()
                                            : nullptr;
  }
  default:
    return nullptr;
  }
}

static const hlsl::DxilResourceBase *
FindResourceByName(hlsl::DxilModule &dxilModule,
                   hlsl::DXIL::ResourceClass resourceClass,
                   const std::string &resourceName) {
  auto matches = [&resourceName](const auto &resource) {
    return resource->GetGlobalName() == resourceName;
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

static bool MergeDxilMatchCaptures(
    const std::unordered_map<std::string, llvm::Value *> &sourceCaptures,
    std::unordered_map<std::string, llvm::Value *> &destinationCaptures) {
  for (const auto &entry : sourceCaptures) {
    auto destinationIt = destinationCaptures.find(entry.first);
    if (destinationIt != destinationCaptures.end()) {
      if (destinationIt->second != entry.second)
        return false;
      continue;
    }

    destinationCaptures.insert(entry);
  }

  return true;
}

static bool
CaptureMatchedValue(const std::string &captureName, llvm::Value *value,
                    std::unordered_map<std::string, llvm::Value *> &captures) {
  if (captureName.empty())
    return true;

  auto captureIt = captures.find(captureName);
  if (captureIt == captures.end()) {
    captures.emplace(captureName, value);
    return true;
  }

  return captureIt->second == value;
}

static bool IsTextureResourceKind(hlsl::DXIL::ResourceKind resourceKind) {
  switch (resourceKind) {
  case hlsl::DXIL::ResourceKind::Texture1D:
  case hlsl::DXIL::ResourceKind::Texture2D:
  case hlsl::DXIL::ResourceKind::Texture2DMS:
  case hlsl::DXIL::ResourceKind::Texture3D:
  case hlsl::DXIL::ResourceKind::TextureCube:
  case hlsl::DXIL::ResourceKind::Texture1DArray:
  case hlsl::DXIL::ResourceKind::Texture2DArray:
  case hlsl::DXIL::ResourceKind::Texture2DMSArray:
  case hlsl::DXIL::ResourceKind::TextureCubeArray:
  case hlsl::DXIL::ResourceKind::FeedbackTexture2D:
  case hlsl::DXIL::ResourceKind::FeedbackTexture2DArray:
    return true;
  default:
    return false;
  }
}

static bool MatchDxilOperandPattern(
    llvm::Value *value, const DxilOperandPattern &pattern,
    std::unordered_map<std::string, llvm::Value *> &captures,
    hlsl::DxilModule *dxilModule);

static bool
TryResolveResourceFromHandle(llvm::Value *value, hlsl::DxilModule &dxilModule,
                             hlsl::DXIL::ResourceClass preferredResourceClass,
                             const hlsl::DxilResourceBase *&resource) {
  const llvm::CallInst *const callInit = llvm::dyn_cast<llvm::CallInst>(value);
  const llvm::CallInst *call = callInit;
  if (call == nullptr)
    return false;

  if (IsDxOpCall(*call, hlsl::OP::OpCode::AnnotateHandle)) {
    if (call->getNumArgOperands() < 2)
      return false;
    call = llvm::dyn_cast<llvm::CallInst>(call->getArgOperand(1));
    if (call == nullptr)
      return false;
  }

  if (!IsDxOpCall(*call, hlsl::OP::OpCode::CreateHandleFromBinding) ||
      call->getNumArgOperands() < 4) {
    return false;
  }

  uint64_t bindPoint = 0;
  if (!TryGetConstantStructIntField(call->getArgOperand(1), 0, bindPoint) &&
      !TryGetConstantStructIntField(call->getArgOperand(1), 1, bindPoint)) {
    return false;
  }

  uint64_t space = 0;
  if (!TryGetConstantStructIntField(call->getArgOperand(1), 2, space))
    return false;

  uint64_t resourceClassValue = 0;
  if (!TryGetConstantStructIntField(call->getArgOperand(1), 3,
                                    resourceClassValue))
    return false;

  uint64_t handleIndex = bindPoint;
  if (const llvm::ConstantInt *handleIndexConstant =
          llvm::dyn_cast<llvm::ConstantInt>(call->getArgOperand(2))) {
    handleIndex = handleIndexConstant->getZExtValue();
  }

  hlsl::DXIL::ResourceClass resolvedResourceClass =
      static_cast<hlsl::DXIL::ResourceClass>(resourceClassValue);
  if (resolvedResourceClass == hlsl::DXIL::ResourceClass::Invalid &&
      preferredResourceClass != hlsl::DXIL::ResourceClass::Invalid) {
    resolvedResourceClass = preferredResourceClass;
  }

  resource = FindResourceByBinding(dxilModule, resolvedResourceClass,
                                   static_cast<unsigned>(handleIndex),
                                   static_cast<unsigned>(space));
  if (resource == nullptr &&
      resolvedResourceClass != hlsl::DXIL::ResourceClass::Invalid) {
    resource = FindResourceByOrdinal(dxilModule, resolvedResourceClass,
                                     static_cast<unsigned>(handleIndex));
  }
  return resource != nullptr;
}

// NOLINTNEXTLINE(misc-no-recursion)
static bool MatchDxilOperandPattern(
    llvm::Value *value, const DxilOperandPattern &pattern,
    std::unordered_map<std::string, llvm::Value *> &captures,
    hlsl::DxilModule *dxilModule) {
  if (value == nullptr)
    return false;

  if (!pattern.matchCaptureName.empty()) {
    auto captureIt = captures.find(pattern.matchCaptureName);
    if (captureIt == captures.end() || captureIt->second != value)
      return false;
  }

  switch (pattern.kind) {
  case DxilOperandPatternKind::Any:
    break;
  case DxilOperandPatternKind::ConstantInt:
    if (!IsConstantIntValue(value, pattern.constantIntValue))
      return false;
    break;
  case DxilOperandPatternKind::Instruction: {
    const llvm::Instruction *const instruction =
        llvm::dyn_cast<llvm::Instruction>(value);
    if (instruction == nullptr ||
        instruction->getOpcode() != pattern.instructionOpcode)
      return false;
    for (const DxilOperandPattern &operandPattern : pattern.operandPatterns) {
      if (operandPattern.operandIndex >= instruction->getNumOperands() ||
          !MatchDxilOperandPattern(
              instruction->getOperand(operandPattern.operandIndex),
              operandPattern, captures, dxilModule)) {
        return false;
      }
    }
    break;
  }
  case DxilOperandPatternKind::DxOpCall: {
    const llvm::CallInst *const call = llvm::dyn_cast<llvm::CallInst>(value);
    if (call == nullptr)
      return false;

    if (!pattern.calleeName.empty()) {
      const llvm::Function *const callee = call->getCalledFunction();
      if (callee == nullptr || callee->getName() != pattern.calleeName)
        return false;
    }

    if (pattern.matchDxilOpCode && !IsDxOpCall(*call, pattern.dxilOpCode))
      return false;

    for (const DxilOperandPattern &operandPattern : pattern.operandPatterns) {
      if (operandPattern.operandIndex >= call->getNumArgOperands() ||
          !MatchDxilOperandPattern(
              call->getArgOperand(operandPattern.operandIndex), operandPattern,
              captures, dxilModule)) {
        return false;
      }
    }
    break;
  }
  case DxilOperandPatternKind::ResourceHandle: {
    if (dxilModule == nullptr)
      return false;

    const hlsl::DxilResourceBase *resource = nullptr;
    const hlsl::DXIL::ResourceClass preferredResourceClass =
        pattern.matchResourceClass ? pattern.resourceClass
                                   : hlsl::DXIL::ResourceClass::Invalid;
    if (!TryResolveResourceFromHandle(value, *dxilModule,
                                      preferredResourceClass, resource)) {
      return false;
    }

    if (pattern.matchResourceClass &&
        resource->GetClass() != pattern.resourceClass)
      return false;

    if (pattern.matchAnyTexture && !IsTextureResourceKind(resource->GetKind()))
      return false;

    if (pattern.matchResourceKind &&
        resource->GetKind() != pattern.resourceKind)
      return false;

    if (!pattern.resourceName.empty() &&
        resource->GetGlobalName() != pattern.resourceName)
      return false;

    if (!pattern.resourceNameLikePattern.empty()) {
      llvm::Regex resourceNameRegex(pattern.resourceNameLikePattern);
      if (!resourceNameRegex.match(resource->GetGlobalName()))
        return false;
    }

    if (pattern.resourceBindPoint >= 0 &&
        resource->GetLowerBound() !=
            static_cast<unsigned>(pattern.resourceBindPoint)) {
      return false;
    }

    if (pattern.resourceSpace >= 0 &&
        resource->GetSpaceID() !=
            static_cast<unsigned>(pattern.resourceSpace)) {
      return false;
    }
    break;
  }
  }

  return CaptureMatchedValue(pattern.captureName, value, captures);
}

static bool
MatchDxilCallPattern(llvm::CallInst *call, const DxilCallPattern &pattern,
                     std::unordered_map<std::string, llvm::Value *> &captures,
                     hlsl::DxilModule *dxilModule) {
  if (call == nullptr)
    return false;

  if (!pattern.calleeName.empty()) {
    const llvm::Function *callee = call->getCalledFunction();
    if (callee == nullptr || callee->getName() != pattern.calleeName)
      return false;
  }

  if (pattern.matchDxilOpCode && !IsDxOpCall(*call, pattern.dxilOpCode))
    return false;

  if (!CaptureMatchedValue(pattern.captureName, call, captures))
    return false;

  for (const DxilOperandPattern &operandPattern : pattern.operandPatterns) {
    if (operandPattern.operandIndex >= call->getNumArgOperands() ||
        !MatchDxilOperandPattern(
            call->getArgOperand(operandPattern.operandIndex), operandPattern,
            captures, dxilModule)) {
      return false;
    }
  }

  return true;
}

static bool IsPrunableDxilInstruction(const llvm::Instruction &instruction) {
  if (const llvm::CallInst *call =
          llvm::dyn_cast<llvm::CallInst>(&instruction)) {
    const llvm::Function *callee = call->getCalledFunction();
    if (callee == nullptr)
      return false;
    if (call->doesNotAccessMemory() || call->onlyReadsMemory())
      return true;

    const llvm::StringRef calleeName = callee->getName();
    return calleeName == "dx.op.annotateHandle" ||
           calleeName == "dx.op.createHandleFromBinding";
  }

  return !instruction.mayHaveSideEffects();
}

static void CollectPrunableOperands( // NOLINT(misc-no-recursion)
    llvm::Instruction *instruction,
    std::unordered_set<llvm::Instruction *> &visited,
    std::vector<llvm::WeakTrackingVH> &postOrder) {
  if (instruction == nullptr || !visited.insert(instruction).second)
    return;

  postOrder.emplace_back(instruction);
  for (const llvm::Use &operandUse : instruction->operands()) {
    llvm::Instruction *operandInstruction =
        llvm::dyn_cast<llvm::Instruction>(operandUse.get());
    if (operandInstruction == nullptr || !operandInstruction->use_empty())
      continue;

    CollectPrunableOperands(operandInstruction, visited, postOrder);
  }
}

static void PruneDeadDxilTree(llvm::Instruction *root) {
  if (root == nullptr)
    return;

  std::unordered_set<llvm::Instruction *> visited;
  std::vector<llvm::WeakTrackingVH> postOrder;
  CollectPrunableOperands(root, visited, postOrder);

  for (auto it = postOrder.rbegin(); it != postOrder.rend(); ++it) {
    llvm::Instruction *candidate = llvm::dyn_cast_or_null<llvm::Instruction>(
        static_cast<llvm::Value *>(*it));
    if (candidate == nullptr || !candidate->use_empty() ||
        !IsPrunableDxilInstruction(*candidate)) {
      continue;
    }

    if (llvm::isInstructionTriviallyDead(candidate)) {
      llvm::RecursivelyDeleteTriviallyDeadInstructions(candidate);
      continue;
    }

    candidate->eraseFromParent();
  }
}

static llvm::Constant *CreateResBindConstant(llvm::Type *resBindType,
                                             unsigned bindPoint, unsigned space,
                                             unsigned resourceClass) {
  llvm::StructType *structType = llvm::dyn_cast<llvm::StructType>(resBindType);
  if (structType == nullptr || structType->getNumElements() != 4)
    return nullptr;

  llvm::Type *lowerType = structType->getElementType(0);
  llvm::Type *upperType = structType->getElementType(1);
  llvm::Type *spaceType = structType->getElementType(2);
  llvm::Type *classType = structType->getElementType(3);

  llvm::Constant *const fields[] = {
      llvm::ConstantInt::get(lowerType, bindPoint),
      llvm::ConstantInt::get(upperType, bindPoint),
      llvm::ConstantInt::get(spaceType, space),
      llvm::ConstantInt::get(classType, resourceClass),
  };
  return llvm::ConstantStruct::get(structType, fields);
}

static void
PruneCandidateInstructions(const std::vector<llvm::Instruction *> &candidates) {
  std::vector<llvm::WeakTrackingVH> candidateHandles;
  candidateHandles.reserve(candidates.size());
  for (llvm::Instruction *candidate : candidates)
    candidateHandles.emplace_back(candidate);

  for (const llvm::WeakTrackingVH &candidateHandle : candidateHandles) {
    llvm::Instruction *candidate = llvm::dyn_cast_or_null<llvm::Instruction>(
        static_cast<llvm::Value *>(candidateHandle));
    if (candidate == nullptr)
      continue;

    if (llvm::isInstructionTriviallyDead(candidate)) {
      llvm::RecursivelyDeleteTriviallyDeadInstructions(candidate);
      continue;
    }

    PruneDeadDxilTree(candidate);
  }
}

static llvm::Instruction *
ResolveMatchInstruction(const DxilMatchResult &match,
                        const std::string &captureName) {
  if (captureName.empty())
    return match.rootCall;

  return llvm::dyn_cast_or_null<llvm::Instruction>(
      match.GetCapture(captureName));
}

static void
AppendUniqueInstruction(std::vector<llvm::Instruction *> &instructions,
                        llvm::Instruction *instruction) {
  if (instruction == nullptr)
    return;

  // NOLINTNEXTLINE(llvm-use-ranges)
  if (std::find(instructions.begin(), instructions.end(), instruction) ==
      instructions.end()) {
    instructions.push_back(instruction);
  }
}

static bool CollectInstructionRange(llvm::Instruction *start,
                                    llvm::Instruction *end,
                                    std::vector<llvm::Instruction *> &range) {
  range.clear();
  if (start == nullptr || end == nullptr ||
      start->getParent() != end->getParent())
    return false;

  bool foundStart = false;
  for (llvm::Instruction &instruction : *start->getParent()) {
    if (&instruction == start)
      foundStart = true;
    if (!foundStart)
      continue;

    range.push_back(&instruction);
    if (&instruction == end)
      return true;
  }

  range.clear();
  return false;
}

static const llvm::Argument *GetFunctionArg(llvm::Function *function,
                                            unsigned index) {
  if (function == nullptr || index >= function->arg_size())
    return nullptr;

  auto argIt = function->arg_begin();
  std::advance(argIt, index);
  return &*argIt;
}

static llvm::Function *FindDxilOpPrototype(llvm::Function &entryFunction,
                                           hlsl::OP::OpCode opCode) {
  for (llvm::BasicBlock &basicBlock : entryFunction) {
    for (llvm::Instruction &instruction : basicBlock) {
      if (!IsDxOpCall(instruction, opCode))
        continue;

      const llvm::CallInst *call = llvm::dyn_cast<llvm::CallInst>(&instruction);
      llvm::Function *callee =
          call != nullptr ? call->getCalledFunction() : nullptr;
      if (callee != nullptr)
        return callee;
    }
  }

  return nullptr;
}

static const hlsl::DxilResourceBase *
ResolveEmitResource(const std::string &resourceName,
                    const ResourceBindingDesc &resourceBinding,
                    hlsl::DxilModule &dxilModule) {
  const hlsl::DxilResourceBase *resource = nullptr;
  if (!resourceName.empty()) {
    resource = FindResourceByName(
        dxilModule, resourceBinding.GetResourceClass(), resourceName);
  }
  if (resource == nullptr && !resourceBinding.IsAutoBinding()) {
    resource = FindResourceByBinding(
        dxilModule, resourceBinding.GetResourceClass(),
        resourceBinding.GetBindPoint(), resourceBinding.GetSpace());
  }
  return resource;
}

static ResourceBindingDesc
ResolveEmitResourceBinding(const ResourceBindingDesc &resourceBinding,
                           const hlsl::DxilResourceBase &resource) {
  if (!resourceBinding.IsAutoBinding())
    return resourceBinding;

  ResourceBindingDesc resolvedBinding(
      resource.GetClass(), resource.GetLowerBound(), resource.GetSpaceID());
  return resolvedBinding;
}

static llvm::Constant *
BuildEmitResourceBindingConstant(hlsl::OP &dxilOp, hlsl::DxilModule &dxilModule,
                                 const ResourceBindingDesc &resourceBinding) {
  return hlsl::resource_helper::getAsConstant(resourceBinding.GetDxilBinding(),
                                              dxilOp.GetResourceBindingType(),
                                              *dxilModule.GetShaderModel());
}

static llvm::Constant *
BuildEmitResourcePropertiesConstant(hlsl::OP &dxilOp,
                                    hlsl::DxilModule &dxilModule,
                                    const hlsl::DxilResourceBase &resource) {
  return hlsl::resource_helper::getAsConstant(
      hlsl::resource_helper::loadPropsFromResourceBase(&resource),
      dxilOp.GetResourcePropertiesType(), *dxilModule.GetShaderModel());
}

static llvm::Value *ResolveEmitOperandValue(
    const DxilRewriteEmitOperand &operand, llvm::Type *argType,
    llvm::IRBuilder<> &builder, llvm::Module &module,
    hlsl::DxilModule &dxilModule, const DxilMatchResult &match,
    const std::unordered_map<std::string, llvm::Value *> *temporaryValues) {
  switch (operand.kind) {
  case DxilRewriteEmitOperandKind::Capture:
    return match.GetCapture(operand.captureName);
  case DxilRewriteEmitOperandKind::Temporary:
    if (temporaryValues == nullptr)
      return nullptr;
    {
      auto temporaryIt = temporaryValues->find(operand.temporaryName);
      return temporaryIt != temporaryValues->end() ? temporaryIt->second
                                                   : nullptr;
    }
  case DxilRewriteEmitOperandKind::ConstantInt: {
    llvm::IntegerType *integerType = llvm::dyn_cast<llvm::IntegerType>(argType);
    if (integerType == nullptr)
      return nullptr;
    return llvm::ConstantInt::get(integerType, operand.constantIntValue);
  }
  case DxilRewriteEmitOperandKind::ResourceHandle: {
    hlsl::OP dxilOp(module.getContext(), &module);
    dxilOp.InitWithMinPrecision(dxilModule.GetUseMinPrecision());

    llvm::Function *entryFunction = dxilModule.GetEntryFunction();
    if (entryFunction == nullptr)
      return nullptr;

    const hlsl::DxilResourceBase *resource = ResolveEmitResource(
        operand.resourceName, operand.resourceBinding, dxilModule);
    if (resource == nullptr)
      return nullptr;

    const ResourceBindingDesc resolvedBinding =
        ResolveEmitResourceBinding(operand.resourceBinding, *resource);

    llvm::Constant *resourceBindingConstant =
        BuildEmitResourceBindingConstant(dxilOp, dxilModule, resolvedBinding);
    llvm::Constant *resourcePropsConstant =
        BuildEmitResourcePropertiesConstant(dxilOp, dxilModule, *resource);
    llvm::Function *createHandleFunction = FindDxilOpPrototype(
        *entryFunction, hlsl::OP::OpCode::CreateHandleFromBinding);
    llvm::Function *annotateHandleFunction =
        FindDxilOpPrototype(*entryFunction, hlsl::OP::OpCode::AnnotateHandle);
    if (resourceBindingConstant == nullptr ||
        resourcePropsConstant == nullptr || createHandleFunction == nullptr ||
        annotateHandleFunction == nullptr) {
      return nullptr;
    }

    const llvm::Argument *createHandleOpcodeArgument =
        GetFunctionArg(createHandleFunction, 0);
    const llvm::Argument *createHandleIndexArgument =
        GetFunctionArg(createHandleFunction, 2);
    const llvm::Argument *createHandleNonUniformArgument =
        GetFunctionArg(createHandleFunction, 3);
    const llvm::Argument *annotateHandleOpcodeArgument =
        GetFunctionArg(annotateHandleFunction, 0);
    if (createHandleOpcodeArgument == nullptr ||
        createHandleIndexArgument == nullptr ||
        createHandleNonUniformArgument == nullptr ||
        annotateHandleOpcodeArgument == nullptr) {
      return nullptr;
    }

    llvm::Value *rawHandle = builder.CreateCall(
        createHandleFunction,
        {llvm::ConstantInt::get(
             createHandleOpcodeArgument->getType(),
             static_cast<uint64_t>(hlsl::OP::OpCode::CreateHandleFromBinding)),
         resourceBindingConstant,
         llvm::ConstantInt::get(createHandleIndexArgument->getType(),
                                resolvedBinding.GetBindPoint()),
         llvm::ConstantInt::get(createHandleNonUniformArgument->getType(), 0)});
    return builder.CreateCall(
        annotateHandleFunction,
        {llvm::ConstantInt::get(
             annotateHandleOpcodeArgument->getType(),
             static_cast<uint64_t>(hlsl::OP::OpCode::AnnotateHandle)),
         rawHandle, resourcePropsConstant});
  }
  case DxilRewriteEmitOperandKind::Undef:
    return llvm::UndefValue::get(argType);
  }

  return nullptr;
}

static bool BuildDeclarativeSequenceRewriteResult(
    const DxilRewriteRule &rule, llvm::Instruction *replacementTarget,
    llvm::IRBuilder<> &builder, llvm::Module &module,
    hlsl::DxilModule &dxilModule, const DxilMatchResult &match,
    DxilRewriteResult &result) {
  if (replacementTarget == nullptr || rule.emittedSequence.values.empty() ||
      rule.emittedSequence.replacementValueName.empty()) {
    return false;
  }

  std::unordered_map<std::string, llvm::Value *> temporaryValues;
  hlsl::OP dxilOp(module.getContext(), &module);
  dxilOp.InitWithMinPrecision(dxilModule.GetUseMinPrecision());

  for (const DxilRewriteEmitValue &value : rule.emittedSequence.values) {
    if (value.name.empty())
      return false;

    llvm::Value *emittedValue = nullptr;
    if (value.kind == DxilRewriteEmitValueKind::DxOpCall) {
      llvm::Type *emittedResultType = GetEmitValueScalarType(
          value, module.getContext(), replacementTarget->getType());
      if (emittedResultType == nullptr)
        return false;

      llvm::Function *emittedFunction =
          dxilOp.GetOpFunc(value.dxilOpCode, emittedResultType);
      if (emittedFunction == nullptr)
        return false;

      std::vector<DxilRewriteEmitOperand> emitOperands = value.operands;
      std::sort(emitOperands.begin(), emitOperands.end(),
                [](const DxilRewriteEmitOperand &lhs,
                   const DxilRewriteEmitOperand &rhs) {
                  return lhs.operandIndex < rhs.operandIndex;
                });

      std::vector<llvm::Value *> emittedArgs;
      emittedArgs.reserve(emittedFunction->arg_size());
      const llvm::Argument *opcodeArgument = GetFunctionArg(emittedFunction, 0);
      if (opcodeArgument == nullptr)
        return false;
      emittedArgs.push_back(llvm::ConstantInt::get(
          opcodeArgument->getType(), static_cast<uint64_t>(value.dxilOpCode)));

      size_t emitOperandIndex = 0;
      for (unsigned argIndex = 1; argIndex < emittedFunction->arg_size();
           ++argIndex) {
        if (emitOperandIndex >= emitOperands.size() ||
            emitOperands[emitOperandIndex].operandIndex != argIndex) {
          return false;
        }

        const llvm::Argument *arg = GetFunctionArg(emittedFunction, argIndex);
        if (arg == nullptr)
          return false;

        // ResolveEmitOperandValue returns a mutable Value* used in mutable LLVM
        // APIs. NOLINTNEXTLINE(misc-const-correctness)
        llvm::Value *argValue = ResolveEmitOperandValue(
            emitOperands[emitOperandIndex++], arg->getType(), builder, module,
            dxilModule, match, &temporaryValues);
        if (argValue == nullptr || argValue->getType() != arg->getType())
          return false;

        emittedArgs.push_back(argValue);
      }

      emittedValue = builder.CreateCall(emittedFunction, emittedArgs);
    } else if (value.kind == DxilRewriteEmitValueKind::ExtractValue) {
      auto aggregateIt = temporaryValues.find(value.aggregateName);
      if (aggregateIt == temporaryValues.end())
        return false;
      emittedValue =
          builder.CreateExtractValue(aggregateIt->second, value.extractIndex);
    } else if (value.kind == DxilRewriteEmitValueKind::BinaryInstruction) {
      llvm::Type *resultType = GetEmitValueScalarType(
          value, module.getContext(), replacementTarget->getType());
      if (resultType == nullptr)
        return false;

      std::vector<DxilRewriteEmitOperand> emitOperands = value.operands;
      std::sort(emitOperands.begin(), emitOperands.end(),
                [](const DxilRewriteEmitOperand &lhs,
                   const DxilRewriteEmitOperand &rhs) {
                  return lhs.operandIndex < rhs.operandIndex;
                });
      if (emitOperands.size() != 2 || emitOperands[0].operandIndex != 0 ||
          emitOperands[1].operandIndex != 1) {
        return false;
      }

      llvm::Value *lhs =
          ResolveEmitOperandValue(emitOperands[0], resultType, builder, module,
                                  dxilModule, match, &temporaryValues);
      llvm::Value *rhs =
          ResolveEmitOperandValue(emitOperands[1], resultType, builder, module,
                                  dxilModule, match, &temporaryValues);
      if (lhs == nullptr || rhs == nullptr || lhs->getType() != resultType ||
          rhs->getType() != resultType) {
        return false;
      }

      emittedValue = builder.CreateBinOp(
          static_cast<llvm::Instruction::BinaryOps>(value.instructionOpcode),
          lhs, rhs);
    } else if (value.kind == DxilRewriteEmitValueKind::CastInstruction) {
      llvm::Type *resultType = GetEmitValueScalarType(
          value, module.getContext(), replacementTarget->getType());
      if (resultType == nullptr)
        return false;

      std::vector<DxilRewriteEmitOperand> emitOperands = value.operands;
      std::sort(emitOperands.begin(), emitOperands.end(),
                [](const DxilRewriteEmitOperand &lhs,
                   const DxilRewriteEmitOperand &rhs) {
                  return lhs.operandIndex < rhs.operandIndex;
                });
      if (emitOperands.size() != 1 || emitOperands[0].operandIndex != 0)
        return false;

      llvm::Value *source =
          ResolveEmitOperandValue(emitOperands[0], nullptr, builder, module,
                                  dxilModule, match, &temporaryValues);
      if (source == nullptr)
        return false;

      emittedValue = builder.CreateCast(
          static_cast<llvm::Instruction::CastOps>(value.castOpcode), source,
          resultType);
    } else if (value.kind ==
               DxilRewriteEmitValueKind::CreateHandleForResource) {
      llvm::Function *entryFunction = dxilModule.GetEntryFunction();
      if (entryFunction == nullptr)
        return false;

      const hlsl::DxilResourceBase *resource = ResolveEmitResource(
          value.resourceName, value.resourceBinding, dxilModule);
      if (resource == nullptr)
        return false;

      const ResourceBindingDesc resolvedBinding =
          ResolveEmitResourceBinding(value.resourceBinding, *resource);

      llvm::Constant *resourceBindingConstant =
          BuildEmitResourceBindingConstant(dxilOp, dxilModule, resolvedBinding);
      llvm::Function *createHandleFunction = FindDxilOpPrototype(
          *entryFunction, hlsl::OP::OpCode::CreateHandleFromBinding);
      if (resourceBindingConstant == nullptr || createHandleFunction == nullptr)
        return false;

      const llvm::Argument *opcodeArgument =
          GetFunctionArg(createHandleFunction, 0);
      const llvm::Argument *indexArgument =
          GetFunctionArg(createHandleFunction, 2);
      const llvm::Argument *nonUniformArgument =
          GetFunctionArg(createHandleFunction, 3);
      if (opcodeArgument == nullptr || indexArgument == nullptr ||
          nonUniformArgument == nullptr) {
        return false;
      }

      emittedValue = builder.CreateCall(
          createHandleFunction,
          {llvm::ConstantInt::get(
               opcodeArgument->getType(),
               static_cast<uint64_t>(
                   hlsl::OP::OpCode::CreateHandleFromBinding)),
           resourceBindingConstant,
           llvm::ConstantInt::get(indexArgument->getType(),
                                  resolvedBinding.GetBindPoint()),
           llvm::ConstantInt::get(nonUniformArgument->getType(), 0)});
    } else if (value.kind ==
               DxilRewriteEmitValueKind::AnnotateHandleForResource) {
      llvm::Function *entryFunction = dxilModule.GetEntryFunction();
      if (entryFunction == nullptr)
        return false;

      auto handleIt = temporaryValues.find(value.handleName);
      if (handleIt == temporaryValues.end())
        return false;

      const hlsl::DxilResourceBase *resource = ResolveEmitResource(
          value.resourceName, value.resourceBinding, dxilModule);
      if (resource == nullptr)
        return false;

      llvm::Constant *resourcePropsConstant =
          BuildEmitResourcePropertiesConstant(dxilOp, dxilModule, *resource);
      llvm::Function *annotateHandleFunction =
          FindDxilOpPrototype(*entryFunction, hlsl::OP::OpCode::AnnotateHandle);
      if (resourcePropsConstant == nullptr || annotateHandleFunction == nullptr)
        return false;

      const llvm::Argument *opcodeArgument =
          GetFunctionArg(annotateHandleFunction, 0);
      if (opcodeArgument == nullptr)
        return false;

      emittedValue = builder.CreateCall(
          annotateHandleFunction,
          {llvm::ConstantInt::get(
               opcodeArgument->getType(),
               static_cast<uint64_t>(hlsl::OP::OpCode::AnnotateHandle)),
           handleIt->second, resourcePropsConstant});
    } else {
      return false;
    }

    if (emittedValue == nullptr)
      return false;

    temporaryValues[value.name] = emittedValue;
  }

  auto replacementIt =
      temporaryValues.find(rule.emittedSequence.replacementValueName);
  if (replacementIt == temporaryValues.end())
    return false;

  result = DxilRewriteResult{};
  result.success = true;
  result.replacementValue = replacementIt->second;
  for (const std::string &captureName : rule.pruneCaptureNames) {
    const llvm::Instruction *instruction =
        llvm::dyn_cast_or_null<llvm::Instruction>(
            match.GetCapture(captureName));
    if (instruction != nullptr)
      result.pruneRoots.push_back(const_cast<llvm::Instruction *>(instruction));
  }
  return true;
}

static bool BuildDeclarativeRewriteResult(const DxilRewriteRule &rule,
                                          llvm::Instruction *replacementTarget,
                                          llvm::IRBuilder<> &builder,
                                          llvm::Module &module,
                                          hlsl::DxilModule &dxilModule,
                                          const DxilMatchResult &match,
                                          DxilRewriteResult &result) {
  if (!rule.emittedSequence.values.empty()) {
    return BuildDeclarativeSequenceRewriteResult(
        rule, replacementTarget, builder, module, dxilModule, match, result);
  }

  result = DxilRewriteResult{};
  result.success = true;

  if (rule.emittedCall.enabled) {
    if (replacementTarget == nullptr)
      return false;

    hlsl::OP dxilOp(module.getContext(), &module);
    dxilOp.InitWithMinPrecision(dxilModule.GetUseMinPrecision());
    llvm::Function *emittedFunction = dxilOp.GetOpFunc(
        rule.emittedCall.dxilOpCode, replacementTarget->getType());
    if (emittedFunction == nullptr)
      return false;

    std::vector<DxilRewriteEmitOperand> emitOperands =
        rule.emittedCall.operands;
    std::sort(emitOperands.begin(), emitOperands.end(),
              [](const DxilRewriteEmitOperand &lhs,
                 const DxilRewriteEmitOperand &rhs) {
                return lhs.operandIndex < rhs.operandIndex;
              });

    std::vector<llvm::Value *> emittedArgs;
    emittedArgs.reserve(emittedFunction->arg_size());
    const llvm::Argument *opcodeArgument = GetFunctionArg(emittedFunction, 0);
    if (opcodeArgument == nullptr)
      return false;
    emittedArgs.push_back(llvm::ConstantInt::get(
        opcodeArgument->getType(),
        static_cast<uint64_t>(rule.emittedCall.dxilOpCode)));

    size_t emitOperandIndex = 0;
    for (unsigned argIndex = 1; argIndex < emittedFunction->arg_size();
         ++argIndex) {
      if (emitOperandIndex >= emitOperands.size() ||
          emitOperands[emitOperandIndex].operandIndex != argIndex) {
        return false;
      }

      const DxilRewriteEmitOperand &operand = emitOperands[emitOperandIndex++];
      const llvm::Argument *arg = GetFunctionArg(emittedFunction, argIndex);
      if (arg == nullptr)
        return false;
      llvm::Type *argType = arg->getType();
      llvm::Value *argValue = nullptr; // NOLINT(misc-const-correctness)
      switch (operand.kind) {
      case DxilRewriteEmitOperandKind::Capture:
        argValue = match.GetCapture(operand.captureName);
        break;
      case DxilRewriteEmitOperandKind::ConstantInt: {
        llvm::IntegerType *integerType =
            llvm::dyn_cast<llvm::IntegerType>(argType);
        if (integerType == nullptr)
          return false;
        argValue =
            llvm::ConstantInt::get(integerType, operand.constantIntValue);
        break;
      }
      case DxilRewriteEmitOperandKind::ResourceHandle: {
        llvm::Function *entryFunction = dxilModule.GetEntryFunction();
        if (entryFunction == nullptr)
          return false;

        const hlsl::DxilResourceBase *resource = nullptr;
        if (!operand.resourceName.empty()) {
          resource = FindResourceByName(
              dxilModule, operand.resourceBinding.GetResourceClass(),
              operand.resourceName);
        }
        if (resource == nullptr && !operand.resourceBinding.IsAutoBinding()) {
          resource = FindResourceByBinding(
              dxilModule, operand.resourceBinding.GetResourceClass(),
              operand.resourceBinding.GetBindPoint(),
              operand.resourceBinding.GetSpace());
        }
        if (resource == nullptr)
          return false;

        const ResourceBindingDesc resolvedBinding =
            ResolveEmitResourceBinding(operand.resourceBinding, *resource);

        llvm::Constant *resourceBindingConstant =
            hlsl::resource_helper::getAsConstant(
                resolvedBinding.GetDxilBinding(),
                dxilOp.GetResourceBindingType(), *dxilModule.GetShaderModel());
        llvm::Constant *resourcePropsConstant =
            hlsl::resource_helper::getAsConstant(
                hlsl::resource_helper::loadPropsFromResourceBase(resource),
                dxilOp.GetResourcePropertiesType(),
                *dxilModule.GetShaderModel());
        llvm::Function *createHandleFunction = FindDxilOpPrototype(
            *entryFunction, hlsl::OP::OpCode::CreateHandleFromBinding);
        llvm::Function *annotateHandleFunction = FindDxilOpPrototype(
            *entryFunction, hlsl::OP::OpCode::AnnotateHandle);
        if (resourceBindingConstant == nullptr ||
            resourcePropsConstant == nullptr ||
            createHandleFunction == nullptr ||
            annotateHandleFunction == nullptr) {
          return false;
        }

        const llvm::Argument *createHandleOpcodeArgument =
            GetFunctionArg(createHandleFunction, 0);
        const llvm::Argument *createHandleIndexArgument =
            GetFunctionArg(createHandleFunction, 2);
        const llvm::Argument *createHandleNonUniformArgument =
            GetFunctionArg(createHandleFunction, 3);
        const llvm::Argument *annotateHandleOpcodeArgument =
            GetFunctionArg(annotateHandleFunction, 0);
        if (createHandleOpcodeArgument == nullptr ||
            createHandleIndexArgument == nullptr ||
            createHandleNonUniformArgument == nullptr ||
            annotateHandleOpcodeArgument == nullptr) {
          return false;
        }

        llvm::Value *rawHandle = builder.CreateCall(
            createHandleFunction,
            {llvm::ConstantInt::get(
                 createHandleOpcodeArgument->getType(),
                 static_cast<uint64_t>(
                     hlsl::OP::OpCode::CreateHandleFromBinding)),
             resourceBindingConstant,
             llvm::ConstantInt::get(createHandleIndexArgument->getType(),
                                    resolvedBinding.GetBindPoint()),
             llvm::ConstantInt::get(createHandleNonUniformArgument->getType(),
                                    0)});
        argValue = builder.CreateCall(
            annotateHandleFunction,
            {llvm::ConstantInt::get(
                 annotateHandleOpcodeArgument->getType(),
                 static_cast<uint64_t>(hlsl::OP::OpCode::AnnotateHandle)),
             rawHandle, resourcePropsConstant});
        break;
      }
      case DxilRewriteEmitOperandKind::Undef:
        argValue = llvm::UndefValue::get(argType);
        break;
      case DxilRewriteEmitOperandKind::Temporary:
        return false;
      }

      if (argValue == nullptr || argValue->getType() != argType)
        return false;

      emittedArgs.push_back(argValue);
    }

    llvm::Value *replacementValue =
        builder.CreateCall(emittedFunction, emittedArgs);
    if (rule.emittedCall.extractIndex >= 0) {
      replacementValue = builder.CreateExtractValue(
          replacementValue,
          static_cast<unsigned>(rule.emittedCall.extractIndex));
    }
    result.replacementValue = replacementValue;
  } else {
    if (rule.replacementCaptureName.empty())
      return false;

    llvm::Value *replacementValue =
        match.GetCapture(rule.replacementCaptureName);
    if (replacementValue == nullptr)
      return false;

    result.replacementValue = replacementValue;
  }

  for (const std::string &captureName : rule.pruneCaptureNames) {
    const llvm::Instruction *instruction =
        llvm::dyn_cast_or_null<llvm::Instruction>(
            match.GetCapture(captureName));
    if (instruction != nullptr)
      result.pruneRoots.push_back(const_cast<llvm::Instruction *>(instruction));
  }

  return true;
}

static bool SupportsTextureSampleInjection(const TextureResourceDesc &desc) {
  return desc.binding.GetResourceClass() == hlsl::DXIL::ResourceClass::SRV &&
         desc.kind == hlsl::DXIL::ResourceKind::Texture2D &&
         desc.elementKind == hlsl::DXIL::ComponentType::F32 &&
         desc.vectorWidth == 4 && !desc.isReadWrite;
}

} // namespace
// NOLINTEND(llvm-prefer-static-over-anonymous-namespace)

bool FindDxilCallMatch(llvm::Function &function, const DxilCallPattern &pattern,
                       DxilMatchResult &result, hlsl::DxilModule *dxilModule) {
  std::vector<DxilMatchResult> results;
  if (CollectDxilCallMatches(function, pattern, results, dxilModule) == 0)
    return false;

  result = std::move(results.front());
  return true;
}

unsigned CollectDxilCallMatches(llvm::Function &function,
                                const DxilCallPattern &pattern,
                                std::vector<DxilMatchResult> &results,
                                hlsl::DxilModule *dxilModule) {
  results.clear();

  for (llvm::BasicBlock &basicBlock : function) {
    for (llvm::Instruction &instruction : basicBlock) {
      llvm::CallInst *const call = llvm::dyn_cast<llvm::CallInst>(&instruction);
      if (call == nullptr)
        continue;

      std::unordered_map<std::string, llvm::Value *> captures;
      if (!MatchDxilCallPattern(call, pattern, captures, dxilModule))
        continue;

      DxilMatchResult result;
      result.rootCall = call;
      result.captures = std::move(captures);
      results.push_back(std::move(result));
    }
  }

  return static_cast<unsigned>(results.size());
}

void PruneInstructionRoots(const std::vector<llvm::Instruction *> &roots) {
  PruneCandidateInstructions(roots);
}

void PruneFunctionDeadCode(llvm::Function &function) {
  bool changed = false;
  do {
    changed = false;

    std::vector<llvm::WeakTrackingVH> candidates;
    for (llvm::BasicBlock &basicBlock : function) {
      for (llvm::Instruction &instruction : basicBlock) {
        if (!instruction.use_empty())
          continue;
        if (instruction.isTerminator())
          continue;

        candidates.emplace_back(&instruction);
      }
    }

    for (const llvm::WeakTrackingVH &candidateHandle : candidates) {
      llvm::Instruction *candidate = llvm::dyn_cast_or_null<llvm::Instruction>(
          static_cast<llvm::Value *>(candidateHandle));
      if (candidate == nullptr || !candidate->use_empty())
        continue;

      if (llvm::isInstructionTriviallyDead(candidate)) {
        llvm::RecursivelyDeleteTriviallyDeadInstructions(candidate);
        changed = true;
        continue;
      }

      const llvm::WeakTrackingVH pruneProbe(candidate);
      PruneDeadDxilTree(candidate);
      if (static_cast<llvm::Value *>(pruneProbe) == nullptr)
        changed = true;
    }
  } while (changed);
}

bool ApplyDxilRewriteRulesMatchAll(llvm::Function &function,
                                   llvm::Module &module,
                                   hlsl::DxilModule &dxilModule,
                                   const std::vector<DxilRewriteRule> &rules,
                                   unsigned *appliedRuleCount) {
  struct ReplacementWork {
    const DxilRewriteRule *rule = nullptr;
    llvm::WeakTrackingVH replacementTarget;
    llvm::WeakTrackingVH anchorInstruction;
    DxilRewriteMode mode = DxilRewriteMode::Replace;
    bool handledReplacement = false;
    llvm::Value *replacementValue = nullptr;
    std::vector<llvm::WeakTrackingVH> pruneRoots;
    llvm::WeakTrackingVH rangeStart;
    llvm::WeakTrackingVH rangeEnd;
    llvm::BasicBlock *scratchBlock = nullptr;
  };

  unsigned appliedCount = 0;
  std::vector<llvm::Instruction *> allPruneCandidates;
  std::vector<ReplacementWork> replacements;

  // Phase 1: collect matches for all rules from the original IR before any
  // declarative replacement mutates the function.
  for (const DxilRewriteRule &rule : rules) {
    std::vector<DxilMatchResult> matches;
    CollectDxilCallMatches(function, rule.pattern, matches, &dxilModule);

    for (const DxilMatchResult &match : matches) {
      DxilMatchResult effectiveMatch = match;
      bool missingBinding = false;
      for (const DxilCallPattern &bindingPattern : rule.bindingPatterns) {
        DxilMatchResult bindingMatch;
        if (!FindDxilCallMatch(function, bindingPattern, bindingMatch,
                               &dxilModule) ||
            !MergeDxilMatchCaptures(bindingMatch.captures,
                                    effectiveMatch.captures)) {
          missingBinding = true;
          break;
        }
      }
      if (missingBinding)
        continue;
      if (rule.predicate && !rule.predicate(effectiveMatch))
        continue;

      llvm::Instruction *replaceInstruction =
          ResolveMatchInstruction(effectiveMatch, rule.replaceCaptureName);
      llvm::Instruction *rangeStartInstruction =
          ResolveMatchInstruction(effectiveMatch, rule.rangeStartCaptureName);
      llvm::Instruction *rangeEndInstruction =
          ResolveMatchInstruction(effectiveMatch, rule.rangeEndCaptureName);

      llvm::Instruction *anchorInstruction = nullptr;
      if (rule.mode == DxilRewriteMode::After) {
        anchorInstruction =
            rangeEndInstruction != nullptr
                ? rangeEndInstruction
                : (replaceInstruction != nullptr ? replaceInstruction
                                                 : effectiveMatch.rootCall);
      } else {
        anchorInstruction =
            rangeStartInstruction != nullptr
                ? rangeStartInstruction
                : (replaceInstruction != nullptr ? replaceInstruction
                                                 : effectiveMatch.rootCall);
      }
      if (anchorInstruction == nullptr)
        return false;

      llvm::Instruction *replacementTarget = replaceInstruction != nullptr
                                                 ? replaceInstruction
                                                 : effectiveMatch.rootCall;

      // Build replacements in a scratch basic block to avoid mutating
      // the real IR during collection. Keep the block detached so later rule
      // matching still sees the original function snapshot.
      llvm::BasicBlock *scratchBlock =
          llvm::BasicBlock::Create(function.getContext(), "scratch");
      llvm::IRBuilder<> builder(scratchBlock);

      DxilRewriteResult rewriteResult;
      if (rule.replacementCallback) {
        rewriteResult = rule.replacementCallback(effectiveMatch, builder,
                                                 module, dxilModule);
      } else if (!BuildDeclarativeRewriteResult(
                     rule, replacementTarget, builder, module, dxilModule,
                     effectiveMatch, rewriteResult)) {
        return false;
      }
      if (!rewriteResult.success)
        return false;

      ReplacementWork work;
      work.rule = &rule;
      work.replacementTarget = replacementTarget;
      work.anchorInstruction = anchorInstruction;
      work.mode = rule.mode;
      work.handledReplacement = rewriteResult.handledReplacement;
      work.replacementValue = rewriteResult.replacementValue;
      work.rangeStart = rangeStartInstruction;
      work.rangeEnd = rangeEndInstruction;
      work.scratchBlock = scratchBlock;
      for (llvm::Instruction *root : rewriteResult.pruneRoots)
        work.pruneRoots.emplace_back(root);
      replacements.push_back(std::move(work));
    }
  }

  // Phase 2: materialize all collected replacements into the function, then
  // redirect uses and collect prune candidates.
  for (const ReplacementWork &work : replacements) {
    llvm::Instruction *anchorInstruction =
        llvm::dyn_cast_or_null<llvm::Instruction>(
            static_cast<llvm::Value *>(work.anchorInstruction));
    if (anchorInstruction == nullptr) {
      delete work.scratchBlock;
      continue;
    }

    std::vector<llvm::Instruction *> scratchInstructions;
    for (llvm::Instruction &instruction : *work.scratchBlock)
      scratchInstructions.push_back(&instruction);

    for (llvm::Instruction *instruction : scratchInstructions) {
      instruction->removeFromParent();
      if (work.mode == DxilRewriteMode::After)
        instruction->insertAfter(anchorInstruction);
      else
        instruction->insertBefore(anchorInstruction);
    }
    delete work.scratchBlock;

    llvm::Instruction *replacementTarget =
        llvm::dyn_cast_or_null<llvm::Instruction>(
            static_cast<llvm::Value *>(work.replacementTarget));
    if (work.mode == DxilRewriteMode::Replace ||
        work.mode == DxilRewriteMode::ReplaceRange) {
      if (replacementTarget == nullptr)
        continue;
      if (!work.handledReplacement && work.replacementValue != nullptr)
        replacementTarget->replaceAllUsesWith(work.replacementValue);
      AppendUniqueInstruction(allPruneCandidates, replacementTarget);
    }

    if (work.mode == DxilRewriteMode::ReplaceRange) {
      llvm::Instruction *rangeStart = llvm::dyn_cast_or_null<llvm::Instruction>(
          static_cast<llvm::Value *>(work.rangeStart));
      llvm::Instruction *rangeEnd = llvm::dyn_cast_or_null<llvm::Instruction>(
          static_cast<llvm::Value *>(work.rangeEnd));
      if (rangeStart == nullptr)
        rangeStart = replacementTarget != nullptr ? replacementTarget
                                                  : anchorInstruction;
      if (rangeEnd == nullptr)
        rangeEnd = rangeStart;

      std::vector<llvm::Instruction *> rangeInstructions;
      if (!CollectInstructionRange(rangeStart, rangeEnd, rangeInstructions))
        continue;

      for (llvm::Instruction *instruction : rangeInstructions)
        AppendUniqueInstruction(allPruneCandidates, instruction);
    }

    if (work.rule->pruneDeadInstructions) {
      for (const llvm::WeakTrackingVH &rootHandle : work.pruneRoots) {
        llvm::Instruction *root = llvm::dyn_cast_or_null<llvm::Instruction>(
            static_cast<llvm::Value *>(rootHandle));
        AppendUniqueInstruction(allPruneCandidates, root);
      }
    }

    ++appliedCount;
  }

  if (!allPruneCandidates.empty())
    PruneCandidateInstructions(allPruneCandidates);

  // Refresh OP cache once after all pruning completes.
  // Pruning may have deleted DXIL op function call instructions,
  // leaving stale pointers in the cache that cause crashes during
  // module destruction.
  {
    hlsl::OP *op = dxilModule.GetOP();
    if (op)
      op->RefreshCache();
  }

  if (appliedRuleCount != nullptr)
    *appliedRuleCount = appliedCount;

  return true;
}

bool ApplyDxilRewriteRules(llvm::Function &function, llvm::Module &module,
                           hlsl::DxilModule &dxilModule,
                           const std::vector<DxilRewriteRule> &rules,
                           unsigned *appliedRuleCount) {
  unsigned appliedCount = 0;

  for (const DxilRewriteRule &rule : rules) {
    while (true) {
      std::vector<DxilMatchResult> matches;
      CollectDxilCallMatches(function, rule.pattern, matches, &dxilModule);
      bool appliedRule = false;

      for (const DxilMatchResult &match : matches) {
        DxilMatchResult effectiveMatch = match;
        bool missingBinding = false;
        for (const DxilCallPattern &bindingPattern : rule.bindingPatterns) {
          DxilMatchResult bindingMatch;
          if (!FindDxilCallMatch(function, bindingPattern, bindingMatch,
                                 &dxilModule) ||
              !MergeDxilMatchCaptures(bindingMatch.captures,
                                      effectiveMatch.captures)) {
            missingBinding = true;
            break;
          }
        }
        if (missingBinding)
          continue;

        if (rule.predicate && !rule.predicate(effectiveMatch))
          continue;

        llvm::Instruction *replaceInstruction =
            ResolveMatchInstruction(effectiveMatch, rule.replaceCaptureName);
        llvm::Instruction *rangeStartInstruction =
            ResolveMatchInstruction(effectiveMatch, rule.rangeStartCaptureName);
        llvm::Instruction *rangeEndInstruction =
            ResolveMatchInstruction(effectiveMatch, rule.rangeEndCaptureName);

        llvm::Instruction *anchorInstruction = nullptr;
        if (rule.mode == DxilRewriteMode::After) {
          anchorInstruction =
              rangeEndInstruction != nullptr
                  ? rangeEndInstruction
                  : (replaceInstruction != nullptr ? replaceInstruction
                                                   : effectiveMatch.rootCall);
        } else {
          anchorInstruction =
              rangeStartInstruction != nullptr
                  ? rangeStartInstruction
                  : (replaceInstruction != nullptr ? replaceInstruction
                                                   : effectiveMatch.rootCall);
        }
        if (anchorInstruction == nullptr)
          return false;

        llvm::IRBuilder<> builder(anchorInstruction->getContext());
        if (rule.mode == DxilRewriteMode::After) {
          llvm::BasicBlock::iterator insertIt(anchorInstruction);
          ++insertIt;
          builder.SetInsertPoint(anchorInstruction->getParent(), insertIt);
        } else {
          builder.SetInsertPoint(anchorInstruction);
        }

        llvm::Instruction *replacementTarget = replaceInstruction != nullptr
                                                   ? replaceInstruction
                                                   : effectiveMatch.rootCall;

        DxilRewriteResult rewriteResult;
        if (rule.replacementCallback) {
          rewriteResult = rule.replacementCallback(effectiveMatch, builder,
                                                   module, dxilModule);
        } else if (!BuildDeclarativeRewriteResult(
                       rule, replacementTarget, builder, module, dxilModule,
                       effectiveMatch, rewriteResult)) {
          return false;
        }

        if (!rewriteResult.success)
          return false;

        std::vector<llvm::Instruction *> pruneCandidates =
            std::move(rewriteResult.pruneRoots);

        if (rule.mode == DxilRewriteMode::Replace ||
            rule.mode == DxilRewriteMode::ReplaceRange) {
          if (replacementTarget == nullptr)
            return false;

          if (!rewriteResult.handledReplacement) {
            if (rewriteResult.replacementValue == nullptr)
              return false;
            replacementTarget->replaceAllUsesWith(
                rewriteResult.replacementValue);
          }

          AppendUniqueInstruction(pruneCandidates, replacementTarget);
        }

        if (rule.mode == DxilRewriteMode::ReplaceRange) {
          llvm::Instruction *rangeStart =
              rangeStartInstruction != nullptr
                  ? rangeStartInstruction
                  : (replaceInstruction != nullptr ? replaceInstruction
                                                   : effectiveMatch.rootCall);
          llvm::Instruction *rangeEnd =
              rangeEndInstruction != nullptr ? rangeEndInstruction : rangeStart;

          std::vector<llvm::Instruction *> rangeInstructions;
          if (!CollectInstructionRange(rangeStart, rangeEnd, rangeInstructions))
            return false;

          for (llvm::Instruction *instruction : rangeInstructions)
            AppendUniqueInstruction(pruneCandidates, instruction);
        }

        if (rule.pruneDeadInstructions)
          PruneCandidateInstructions(pruneCandidates);

        ++appliedCount;
        appliedRule = true;
        break;
      }

      if (!appliedRule)
        break;
    }
  }

  if (appliedRuleCount != nullptr)
    *appliedRuleCount = appliedCount;

  return true;
}

bool ApplyDxilRewriteRulesOnce(llvm::Function &function, llvm::Module &module,
                               hlsl::DxilModule &dxilModule,
                               const std::vector<DxilRewriteRule> &rules,
                               bool useLastMatch, unsigned *appliedRuleCount) {
  unsigned appliedCount = 0;

  auto applyMatch = [&](const DxilRewriteRule &rule,
                        const DxilMatchResult &match) -> bool {
    DxilMatchResult effectiveMatch = match;
    for (const DxilCallPattern &bindingPattern : rule.bindingPatterns) {
      DxilMatchResult bindingMatch;
      if (!FindDxilCallMatch(function, bindingPattern, bindingMatch,
                             &dxilModule) ||
          !MergeDxilMatchCaptures(bindingMatch.captures,
                                  effectiveMatch.captures)) {
        return true;
      }
    }

    if (rule.predicate && !rule.predicate(effectiveMatch))
      return true;

    llvm::Instruction *replaceInstruction =
        ResolveMatchInstruction(effectiveMatch, rule.replaceCaptureName);
    llvm::Instruction *rangeStartInstruction =
        ResolveMatchInstruction(effectiveMatch, rule.rangeStartCaptureName);
    llvm::Instruction *rangeEndInstruction =
        ResolveMatchInstruction(effectiveMatch, rule.rangeEndCaptureName);

    llvm::Instruction *anchorInstruction = nullptr;
    if (rule.mode == DxilRewriteMode::After) {
      anchorInstruction =
          rangeEndInstruction != nullptr
              ? rangeEndInstruction
              : (replaceInstruction != nullptr ? replaceInstruction
                                               : effectiveMatch.rootCall);
    } else {
      anchorInstruction =
          rangeStartInstruction != nullptr
              ? rangeStartInstruction
              : (replaceInstruction != nullptr ? replaceInstruction
                                               : effectiveMatch.rootCall);
    }
    if (anchorInstruction == nullptr)
      return false;

    llvm::IRBuilder<> builder(anchorInstruction->getContext());
    if (rule.mode == DxilRewriteMode::After) {
      llvm::BasicBlock::iterator insertIt(anchorInstruction);
      ++insertIt;
      builder.SetInsertPoint(anchorInstruction->getParent(), insertIt);
    } else {
      builder.SetInsertPoint(anchorInstruction);
    }

    llvm::Instruction *replacementTarget = replaceInstruction != nullptr
                                               ? replaceInstruction
                                               : effectiveMatch.rootCall;

    DxilRewriteResult rewriteResult;
    if (rule.replacementCallback) {
      rewriteResult =
          rule.replacementCallback(effectiveMatch, builder, module, dxilModule);
    } else if (!BuildDeclarativeRewriteResult(rule, replacementTarget, builder,
                                              module, dxilModule,
                                              effectiveMatch, rewriteResult)) {
      return false;
    }

    if (!rewriteResult.success)
      return false;

    std::vector<llvm::Instruction *> pruneCandidates =
        std::move(rewriteResult.pruneRoots);

    if (rule.mode == DxilRewriteMode::Replace ||
        rule.mode == DxilRewriteMode::ReplaceRange) {
      if (replacementTarget == nullptr)
        return false;

      if (!rewriteResult.handledReplacement) {
        if (rewriteResult.replacementValue == nullptr)
          return false;
        replacementTarget->replaceAllUsesWith(rewriteResult.replacementValue);
      }

      AppendUniqueInstruction(pruneCandidates, replacementTarget);
    }

    if (rule.mode == DxilRewriteMode::ReplaceRange) {
      llvm::Instruction *rangeStart =
          rangeStartInstruction != nullptr
              ? rangeStartInstruction
              : (replaceInstruction != nullptr ? replaceInstruction
                                               : effectiveMatch.rootCall);
      llvm::Instruction *rangeEnd =
          rangeEndInstruction != nullptr ? rangeEndInstruction : rangeStart;

      std::vector<llvm::Instruction *> rangeInstructions;
      if (!CollectInstructionRange(rangeStart, rangeEnd, rangeInstructions))
        return false;

      for (llvm::Instruction *instruction : rangeInstructions)
        AppendUniqueInstruction(pruneCandidates, instruction);
    }

    if (rule.pruneDeadInstructions)
      PruneCandidateInstructions(pruneCandidates);

    ++appliedCount;
    return true;
  };

  if (!useLastMatch) {
    for (const DxilRewriteRule &rule : rules) {
      std::vector<DxilMatchResult> matches;
      CollectDxilCallMatches(function, rule.pattern, matches, &dxilModule);
      for (const DxilMatchResult &match : matches) {
        const unsigned beforeApplyCount = appliedCount;
        if (!applyMatch(rule, match))
          return false;
        if (appliedCount != beforeApplyCount) {
          if (appliedRuleCount != nullptr)
            *appliedRuleCount = appliedCount;
          return true;
        }
      }
    }
  } else {
    for (auto ruleIt = rules.rbegin(); ruleIt != rules.rend(); ++ruleIt) {
      std::vector<DxilMatchResult> matches;
      CollectDxilCallMatches(function, ruleIt->pattern, matches, &dxilModule);
      for (auto matchIt = matches.rbegin(); matchIt != matches.rend();
           ++matchIt) {
        const unsigned beforeApplyCount = appliedCount;
        if (!applyMatch(*ruleIt, *matchIt))
          return false;
        if (appliedCount != beforeApplyCount) {
          if (appliedRuleCount != nullptr)
            *appliedRuleCount = appliedCount;
          return true;
        }
      }
    }
  }

  if (appliedRuleCount != nullptr)
    *appliedRuleCount = appliedCount;

  return true;
}

bool InjectTextureSampleIntoEntryPoint(Module &module,
                                       hlsl::DxilModule &dxilModule,
                                       const TextureResourceDesc &desc,
                                       bool traceEnabled) {
  if (!SupportsTextureSampleInjection(desc)) {
    TraceMessage(traceEnabled,
                 "sample injection: unsupported texture shape, skipping");
    return true;
  }

  llvm::Function *entryFunction = dxilModule.GetEntryFunction();
  if (entryFunction == nullptr || entryFunction->empty()) {
    std::cerr << "Failed to locate the DXIL entry function for texture sample "
                 "injection.\n";
    return false;
  }

  DxilCallPattern sampleChainPattern;
  sampleChainPattern.matchDxilOpCode = true;
  sampleChainPattern.dxilOpCode = hlsl::OP::OpCode::Sample;
  sampleChainPattern.captureName = "prototypeSampleCall";

  DxilOperandPattern sampledRawTextureHandle;
  sampledRawTextureHandle.operandIndex = 1;
  sampledRawTextureHandle.kind = DxilOperandPatternKind::DxOpCall;
  sampledRawTextureHandle.captureName = "prototypeRawTextureHandle";
  sampledRawTextureHandle.matchDxilOpCode = true;
  sampledRawTextureHandle.dxilOpCode =
      hlsl::OP::OpCode::CreateHandleFromBinding;

  DxilOperandPattern sampledTextureHandle;
  sampledTextureHandle.operandIndex = 1;
  sampledTextureHandle.kind = DxilOperandPatternKind::DxOpCall;
  sampledTextureHandle.captureName = "prototypeAnnotatedTextureHandle";
  sampledTextureHandle.matchDxilOpCode = true;
  sampledTextureHandle.dxilOpCode = hlsl::OP::OpCode::AnnotateHandle;
  sampledTextureHandle.operandPatterns = {sampledRawTextureHandle};

  DxilOperandPattern sampledSamplerHandle;
  sampledSamplerHandle.operandIndex = 2;
  sampledSamplerHandle.kind = DxilOperandPatternKind::Any;
  sampledSamplerHandle.captureName = "prototypeSamplerHandle";

  sampleChainPattern.operandPatterns = {
      sampledTextureHandle,
      sampledSamplerHandle,
  };

  DxilMatchResult sampleChainMatch;
  if (!FindDxilCallMatch(*entryFunction, sampleChainPattern,
                         sampleChainMatch)) {
    std::cerr << "Failed to find an existing sample.f32 call to clone for the "
                 "injected texture.\n";
    return false;
  }

  llvm::CallInst *prototypeSampleCall = sampleChainMatch.rootCall;
  const llvm::CallInst *const prototypeAnnotatedTextureHandle =
      sampleChainMatch.GetCallCapture("prototypeAnnotatedTextureHandle");
  const llvm::CallInst *const prototypeRawTextureHandle =
      sampleChainMatch.GetCallCapture("prototypeRawTextureHandle");
  const llvm::Value *const prototypeSamplerHandle =
      sampleChainMatch.GetCapture("prototypeSamplerHandle");
  if (prototypeAnnotatedTextureHandle == nullptr ||
      prototypeRawTextureHandle == nullptr ||
      !IsDxOpCall(*prototypeAnnotatedTextureHandle,
                  hlsl::OP::OpCode::AnnotateHandle) ||
      !IsDxOpCall(*prototypeRawTextureHandle,
                  hlsl::OP::OpCode::CreateHandleFromBinding) ||
      prototypeSamplerHandle == nullptr) {
    std::cerr
        << "Failed to resolve the existing texture sample handle chain.\n";
    return false;
  }

  llvm::CallInst *redStore = nullptr;
  llvm::CallInst *greenStore = nullptr;
  llvm::CallInst *blueStore = nullptr;
  for (llvm::BasicBlock &basicBlock : *entryFunction) {
    for (llvm::Instruction &instruction : basicBlock) {
      if (!IsDxOpCall(instruction, "dx.op.storeOutput.f32"))
        continue;

      llvm::CallInst *storeCall = llvm::cast<llvm::CallInst>(&instruction);
      if (!IsConstantIntValue(storeCall->getArgOperand(1), 0) ||
          !IsConstantIntValue(storeCall->getArgOperand(2), 0)) {
        continue;
      }

      if (IsConstantIntValue(storeCall->getArgOperand(3), 0))
        redStore = storeCall;
      else if (IsConstantIntValue(storeCall->getArgOperand(3), 1))
        greenStore = storeCall;
      else if (IsConstantIntValue(storeCall->getArgOperand(3), 2))
        blueStore = storeCall;
    }
  }

  if (redStore == nullptr || greenStore == nullptr || blueStore == nullptr) {
    std::cerr << "Failed to locate the final RGB output stores for texture "
                 "sample injection.\n";
    return false;
  }

  llvm::IRBuilder<> sampleBuilder(prototypeSampleCall);
  llvm::Constant *resBind = CreateResBindConstant(
      prototypeRawTextureHandle->getArgOperand(1)->getType(),
      desc.binding.GetBindPoint(), desc.binding.GetSpace(), 0);
  if (resBind == nullptr) {
    std::cerr << "Failed to create a resource binding constant for the "
                 "injected texture sample.\n";
    return false;
  }

  llvm::Value *newRawTextureHandle = sampleBuilder.CreateCall(
      prototypeRawTextureHandle->getCalledFunction(),
      {prototypeRawTextureHandle->getArgOperand(0), resBind,
       llvm::ConstantInt::get(
           prototypeRawTextureHandle->getArgOperand(2)->getType(),
           desc.binding.GetBindPoint()),
       prototypeRawTextureHandle->getArgOperand(3)});

  llvm::Value *newAnnotatedTextureHandle = sampleBuilder.CreateCall(
      prototypeAnnotatedTextureHandle->getCalledFunction(),
      {prototypeAnnotatedTextureHandle->getArgOperand(0), newRawTextureHandle,
       prototypeAnnotatedTextureHandle->getArgOperand(2)});

  std::vector<llvm::Value *> sampleArgs;
  const unsigned sampleArgCount = prototypeSampleCall->getNumArgOperands();
  sampleArgs.reserve(sampleArgCount);
  for (unsigned argIndex = 0; argIndex < sampleArgCount; ++argIndex) {
    sampleArgs.push_back(argIndex == 1
                             ? newAnnotatedTextureHandle
                             : prototypeSampleCall->getArgOperand(argIndex));
  }

  llvm::Value *newSample = sampleBuilder.CreateCall(
      prototypeSampleCall->getCalledFunction(), sampleArgs);
  llvm::Value *newSampleRed = sampleBuilder.CreateExtractValue(newSample, 0);
  llvm::Value *newSampleGreen = sampleBuilder.CreateExtractValue(newSample, 1);
  llvm::Value *newSampleBlue = sampleBuilder.CreateExtractValue(newSample, 2);

  llvm::Constant *blendWeight =
      llvm::ConstantFP::get(llvm::Type::getFloatTy(module.getContext()), 0.25f);
  llvm::Value *const sampleContributions[] = {newSampleRed, newSampleGreen,
                                              newSampleBlue};
  // storeBuilder and later operand rewrites require mutable call instructions.
  // NOLINTNEXTLINE(misc-const-correctness)
  llvm::CallInst *stores[] = {redStore, greenStore, blueStore};
  for (unsigned channelIndex = 0; channelIndex < 3; ++channelIndex) {
    llvm::IRBuilder<> storeBuilder(stores[channelIndex]);
    llvm::Value *scaledSample =
        storeBuilder.CreateFMul(sampleContributions[channelIndex], blendWeight);
    llvm::Value *blendedOutput = storeBuilder.CreateFAdd(
        stores[channelIndex]->getArgOperand(4), scaledSample);
    stores[channelIndex]->setArgOperand(4, blendedOutput);
  }

  TraceMessage(
      traceEnabled,
      "sample injection: cloned a texture sample into the entry function");
  return true;
}