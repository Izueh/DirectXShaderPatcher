#pragma once

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"

#include "dxc/DXIL/DxilOperations.h"

#include "Resources.h"

enum class DxilOperandPatternKind {
  Any,
  DxOpCall,
  Instruction,
  ResourceHandle,
  ConstantInt,
};

struct DxilOperandPattern {
  unsigned operandIndex = 0;
  DxilOperandPatternKind kind = DxilOperandPatternKind::Any;
  std::string captureName;
  std::string matchCaptureName;
  std::string calleeName;
  bool matchDxilOpCode = false;
  hlsl::OP::OpCode dxilOpCode = static_cast<hlsl::OP::OpCode>(0);
  unsigned instructionOpcode = 0;
  bool matchResourceClass = false;
  hlsl::DXIL::ResourceClass resourceClass = hlsl::DXIL::ResourceClass::Invalid;
  bool matchAnyTexture = false;
  bool matchResourceKind = false;
  hlsl::DXIL::ResourceKind resourceKind = hlsl::DXIL::ResourceKind::Invalid;
  std::string resourceName;
  std::string resourceNameLikePattern;
  int resourceBindPoint = -1;
  int resourceSpace = -1;
  uint64_t constantIntValue = 0;
  std::vector<DxilOperandPattern> operandPatterns;
};

struct DxilCallPattern {
  std::string calleeName;
  bool matchDxilOpCode = false;
  hlsl::OP::OpCode dxilOpCode = static_cast<hlsl::OP::OpCode>(0);
  std::string captureName;
  std::vector<DxilOperandPattern> operandPatterns;
};

struct RenderTargetStoreDesc {
  unsigned outputSigId = 0;
  unsigned rowIndex = 0;
  unsigned componentIndex = 0;
};

struct DxilMatchResult {
  llvm::CallInst *rootCall = nullptr;
  std::unordered_map<std::string, llvm::Value *> captures;

  llvm::Value *GetCapture(const std::string &name) const {
    auto it = captures.find(name);
    return it != captures.end() ? it->second : nullptr;
  }

  llvm::CallInst *GetCallCapture(const std::string &name) const {
    return llvm::dyn_cast_or_null<llvm::CallInst>(GetCapture(name));
  }
};

enum class DxilRewriteMode {
  Before,
  After,
  Replace,
  ReplaceRange,
};

struct DxilRewriteResult {
  bool success = true;
  bool handledReplacement = false;
  llvm::Value *replacementValue = nullptr;
  std::vector<llvm::Instruction *> pruneRoots;
};

enum class DxilRewriteEmitOperandKind {
  Capture,
  Temporary,
  ConstantInt,
  ResourceHandle,
  Undef,
};

struct DxilRewriteEmitOperand {
  unsigned operandIndex = 0;
  DxilRewriteEmitOperandKind kind = DxilRewriteEmitOperandKind::Capture;
  std::string captureName;
  std::string temporaryName;
  uint64_t constantIntValue = 0;
  std::string resourceName;
  ResourceBindingDesc resourceBinding;
};

enum class DxilRewriteEmitValueKind {
  DxOpCall,
  ExtractValue,
  BinaryInstruction,
  CastInstruction,
  CreateHandleForResource,
  AnnotateHandleForResource,
};

struct DxilRewriteEmitValue {
  std::string name;
  DxilRewriteEmitValueKind kind = DxilRewriteEmitValueKind::DxOpCall;
  hlsl::OP::OpCode dxilOpCode = static_cast<hlsl::OP::OpCode>(0);
  unsigned instructionOpcode = 0;
  unsigned castOpcode = 0;
  bool hasExplicitResultComponentType = false;
  hlsl::DXIL::ComponentType resultComponentType =
      hlsl::DXIL::ComponentType::Invalid;
  std::vector<DxilRewriteEmitOperand> operands;
  std::string aggregateName;
  std::string handleName;
  std::string resourceName;
  ResourceBindingDesc resourceBinding;
  unsigned extractIndex = 0;
};

struct DxilRewriteEmitSequence {
  std::vector<DxilRewriteEmitValue> values;
  std::string replacementValueName;
};

struct DxilRewriteEmitCall {
  bool enabled = false;
  hlsl::OP::OpCode dxilOpCode = static_cast<hlsl::OP::OpCode>(0);
  int extractIndex = -1;
  std::vector<DxilRewriteEmitOperand> operands;
};

class DxilRewriteResultBuilder {
public:
  DxilRewriteResultBuilder &Success(bool success = true) {
    result_.success = success;
    return *this;
  }

  DxilRewriteResultBuilder &HandledReplacement(bool handled = true) {
    result_.handledReplacement = handled;
    return *this;
  }

  DxilRewriteResultBuilder &ReplaceWith(llvm::Value *replacementValue) {
    result_.replacementValue = replacementValue;
    return *this;
  }

  DxilRewriteResultBuilder &Prune(llvm::Instruction *instruction) {
    if (instruction != nullptr)
      result_.pruneRoots.push_back(instruction);
    return *this;
  }

  DxilRewriteResultBuilder &
  Prune(const std::vector<llvm::Instruction *> &instructions) {
    for (llvm::Instruction *instruction : instructions) {
      if (instruction != nullptr)
        result_.pruneRoots.push_back(instruction);
    }
    return *this;
  }

  DxilRewriteResult Build() const { return result_; }

  operator DxilRewriteResult() const { return Build(); }

private:
  DxilRewriteResult result_;
};

using DxilMatchPredicate = std::function<bool(const DxilMatchResult &)>;
using DxilRewriteCallback = std::function<DxilRewriteResult(
    const DxilMatchResult &, llvm::IRBuilder<> &, llvm::Module &,
    hlsl::DxilModule &)>;

struct DxilRewriteRule {
  std::string name;
  DxilCallPattern pattern;
  std::vector<DxilCallPattern> bindingPatterns;
  DxilMatchPredicate predicate;
  DxilRewriteMode mode = DxilRewriteMode::Replace;
  std::string replaceCaptureName;
  std::string rangeStartCaptureName;
  std::string rangeEndCaptureName;
  std::string replacementCaptureName;
  DxilRewriteEmitCall emittedCall;
  DxilRewriteEmitSequence emittedSequence;
  std::vector<std::string> pruneCaptureNames;
  bool pruneDeadInstructions = true;
  DxilRewriteCallback replacementCallback;
};

class DxilOperandPatternBuilder {
public:
  explicit DxilOperandPatternBuilder(DxilOperandPattern pattern)
      : pattern_(std::move(pattern)) {}

  DxilOperandPatternBuilder &Capture(std::string captureName) {
    pattern_.captureName = std::move(captureName);
    return *this;
  }

  DxilOperandPatternBuilder &MatchCapture(std::string captureName) {
    pattern_.matchCaptureName = std::move(captureName);
    return *this;
  }

  DxilOperandPatternBuilder &
  Args(std::vector<DxilOperandPattern> operandPatterns) {
    pattern_.operandPatterns = std::move(operandPatterns);
    return *this;
  }

  DxilOperandPatternBuilder &
  Args(std::initializer_list<DxilOperandPattern> operandPatterns) {
    pattern_.operandPatterns.assign(operandPatterns.begin(),
                                    operandPatterns.end());
    return *this;
  }

  DxilOperandPatternBuilder &
  ResourceClass(hlsl::DXIL::ResourceClass resourceClass) {
    pattern_.matchResourceClass = true;
    pattern_.resourceClass = resourceClass;
    return *this;
  }

  DxilOperandPatternBuilder &AnyTexture() {
    pattern_.matchAnyTexture = true;
    return *this;
  }

  DxilOperandPatternBuilder &
  ResourceKind(hlsl::DXIL::ResourceKind resourceKind) {
    pattern_.matchResourceKind = true;
    pattern_.resourceKind = resourceKind;
    return *this;
  }

  DxilOperandPatternBuilder &ResourceName(std::string resourceName) {
    pattern_.resourceName = std::move(resourceName);
    return *this;
  }

  DxilOperandPatternBuilder &
  ResourceNameLike(std::string resourceNameLikePattern) {
    pattern_.resourceNameLikePattern = std::move(resourceNameLikePattern);
    return *this;
  }

  DxilOperandPatternBuilder &BindPoint(unsigned bindPoint) {
    pattern_.resourceBindPoint = static_cast<int>(bindPoint);
    return *this;
  }

  DxilOperandPatternBuilder &Space(unsigned space) {
    pattern_.resourceSpace = static_cast<int>(space);
    return *this;
  }

  DxilOperandPattern Build() const { return pattern_; }

  operator DxilOperandPattern() const { return Build(); }

private:
  DxilOperandPattern pattern_;
};

class DxilCallPatternBuilder {
public:
  explicit DxilCallPatternBuilder(DxilCallPattern pattern)
      : pattern_(std::move(pattern)) {}

  DxilCallPatternBuilder &Capture(std::string captureName) {
    pattern_.captureName = std::move(captureName);
    return *this;
  }

  DxilCallPatternBuilder &
  Args(std::vector<DxilOperandPattern> operandPatterns) {
    pattern_.operandPatterns = std::move(operandPatterns);
    return *this;
  }

  DxilCallPatternBuilder &
  Args(std::initializer_list<DxilOperandPattern> operandPatterns) {
    pattern_.operandPatterns.assign(operandPatterns.begin(),
                                    operandPatterns.end());
    return *this;
  }

  DxilCallPattern Build() const { return pattern_; }

  operator DxilCallPattern() const { return Build(); }

private:
  DxilCallPattern pattern_;
};

class DxilRewriteRuleBuilder {
public:
  explicit DxilRewriteRuleBuilder(std::string name) {
    rule_.name = std::move(name);
  }

  DxilRewriteRuleBuilder &Match(DxilCallPattern pattern) {
    rule_.pattern = std::move(pattern);
    return *this;
  }

  DxilRewriteRuleBuilder &Bind(DxilCallPattern pattern) {
    rule_.bindingPatterns.push_back(std::move(pattern));
    return *this;
  }

  DxilRewriteRuleBuilder &Where(DxilMatchPredicate predicate) {
    rule_.predicate = std::move(predicate);
    return *this;
  }

  DxilRewriteRuleBuilder &Mode(DxilRewriteMode mode) {
    rule_.mode = mode;
    return *this;
  }

  DxilRewriteRuleBuilder &ReplaceCapture(std::string captureName) {
    rule_.replaceCaptureName = std::move(captureName);
    return *this;
  }

  DxilRewriteRuleBuilder &RangeStartCapture(std::string captureName) {
    rule_.rangeStartCaptureName = std::move(captureName);
    return *this;
  }

  DxilRewriteRuleBuilder &RangeEndCapture(std::string captureName) {
    rule_.rangeEndCaptureName = std::move(captureName);
    return *this;
  }

  DxilRewriteRuleBuilder &ReplaceWithCapture(std::string captureName) {
    rule_.replacementCaptureName = std::move(captureName);
    return *this;
  }

  DxilRewriteRuleBuilder &EmitDxOp(hlsl::OP::OpCode dxilOpCode) {
    rule_.emittedCall.enabled = true;
    rule_.emittedCall.dxilOpCode = dxilOpCode;
    return *this;
  }

  DxilRewriteRuleBuilder &EmitExtract(unsigned extractIndex) {
    rule_.emittedCall.enabled = true;
    rule_.emittedCall.extractIndex = static_cast<int>(extractIndex);
    return *this;
  }

  DxilRewriteRuleBuilder &EmitOperand(DxilRewriteEmitOperand operand) {
    rule_.emittedCall.enabled = true;
    rule_.emittedCall.operands.push_back(std::move(operand));
    return *this;
  }

  DxilRewriteRuleBuilder &
  EmitOperands(std::vector<DxilRewriteEmitOperand> operands) {
    rule_.emittedCall.enabled = true;
    rule_.emittedCall.operands = std::move(operands);
    return *this;
  }

  DxilRewriteRuleBuilder &EmitValue(DxilRewriteEmitValue value) {
    rule_.emittedSequence.values.push_back(std::move(value));
    return *this;
  }

  DxilRewriteRuleBuilder &ReplaceWithEmittedValue(std::string valueName) {
    rule_.emittedSequence.replacementValueName = std::move(valueName);
    return *this;
  }

  DxilRewriteRuleBuilder &PruneCapture(std::string captureName) {
    rule_.pruneCaptureNames.push_back(std::move(captureName));
    return *this;
  }

  DxilRewriteRuleBuilder &PruneDeadInstructions(bool pruneDeadInstructions) {
    rule_.pruneDeadInstructions = pruneDeadInstructions;
    return *this;
  }

  DxilRewriteRuleBuilder &Callback(DxilRewriteCallback callback) {
    rule_.replacementCallback = std::move(callback);
    return *this;
  }

  DxilRewriteRule Build() const { return rule_; }

  operator DxilRewriteRule() const { return Build(); }

private:
  DxilRewriteRule rule_;
};

inline DxilOperandPatternBuilder AnyOperand(unsigned operandIndex) {
  DxilOperandPattern pattern;
  pattern.operandIndex = operandIndex;
  pattern.kind = DxilOperandPatternKind::Any;
  return DxilOperandPatternBuilder(std::move(pattern));
}

inline DxilOperandPatternBuilder ConstantIntOperand(unsigned operandIndex,
                                                    uint64_t constantIntValue) {
  DxilOperandPattern pattern;
  pattern.operandIndex = operandIndex;
  pattern.kind = DxilOperandPatternKind::ConstantInt;
  pattern.constantIntValue = constantIntValue;
  return DxilOperandPatternBuilder(std::move(pattern));
}

inline DxilOperandPatternBuilder DxOpOperand(unsigned operandIndex,
                                             hlsl::OP::OpCode dxilOpCode) {
  DxilOperandPattern pattern;
  pattern.operandIndex = operandIndex;
  pattern.kind = DxilOperandPatternKind::DxOpCall;
  pattern.matchDxilOpCode = true;
  pattern.dxilOpCode = dxilOpCode;
  return DxilOperandPatternBuilder(std::move(pattern));
}

inline DxilOperandPatternBuilder
InstructionOperand(unsigned operandIndex, unsigned instructionOpcode) {
  DxilOperandPattern pattern;
  pattern.operandIndex = operandIndex;
  pattern.kind = DxilOperandPatternKind::Instruction;
  pattern.instructionOpcode = instructionOpcode;
  return DxilOperandPatternBuilder(std::move(pattern));
}

inline DxilOperandPatternBuilder ResourceHandleOperand(unsigned operandIndex) {
  DxilOperandPattern pattern;
  pattern.operandIndex = operandIndex;
  pattern.kind = DxilOperandPatternKind::ResourceHandle;
  return DxilOperandPatternBuilder(std::move(pattern));
}

inline DxilCallPatternBuilder DxOpCall(hlsl::OP::OpCode dxilOpCode) {
  DxilCallPattern pattern;
  pattern.matchDxilOpCode = true;
  pattern.dxilOpCode = dxilOpCode;
  return DxilCallPatternBuilder(std::move(pattern));
}

inline DxilCallPatternBuilder NamedCall(std::string calleeName) {
  DxilCallPattern pattern;
  pattern.calleeName = std::move(calleeName);
  return DxilCallPatternBuilder(std::move(pattern));
}

inline DxilRewriteEmitOperand EmitCaptureOperand(unsigned operandIndex,
                                                 std::string captureName) {
  DxilRewriteEmitOperand operand;
  operand.operandIndex = operandIndex;
  operand.kind = DxilRewriteEmitOperandKind::Capture;
  operand.captureName = std::move(captureName);
  return operand;
}

inline DxilRewriteEmitOperand EmitTemporaryOperand(unsigned operandIndex,
                                                   std::string temporaryName) {
  DxilRewriteEmitOperand operand;
  operand.operandIndex = operandIndex;
  operand.kind = DxilRewriteEmitOperandKind::Temporary;
  operand.temporaryName = std::move(temporaryName);
  return operand;
}

inline DxilRewriteEmitOperand
EmitConstantIntOperand(unsigned operandIndex, uint64_t constantIntValue) {
  DxilRewriteEmitOperand operand;
  operand.operandIndex = operandIndex;
  operand.kind = DxilRewriteEmitOperandKind::ConstantInt;
  operand.constantIntValue = constantIntValue;
  return operand;
}

inline DxilRewriteEmitOperand
EmitResourceHandleOperand(unsigned operandIndex,
                          ResourceBindingDesc resourceBinding,
                          std::string resourceName = std::string()) {
  DxilRewriteEmitOperand operand;
  operand.operandIndex = operandIndex;
  operand.kind = DxilRewriteEmitOperandKind::ResourceHandle;
  operand.resourceName = std::move(resourceName);
  operand.resourceBinding = std::move(resourceBinding);
  return operand;
}

inline DxilRewriteEmitOperand EmitUndefOperand(unsigned operandIndex) {
  DxilRewriteEmitOperand operand;
  operand.operandIndex = operandIndex;
  operand.kind = DxilRewriteEmitOperandKind::Undef;
  return operand;
}

inline DxilRewriteEmitValue
EmitDxOpValue(std::string name, hlsl::OP::OpCode dxilOpCode,
              std::vector<DxilRewriteEmitOperand> operands = {}) {
  DxilRewriteEmitValue value;
  value.name = std::move(name);
  value.kind = DxilRewriteEmitValueKind::DxOpCall;
  value.dxilOpCode = dxilOpCode;
  value.operands = std::move(operands);
  return value;
}

inline DxilRewriteEmitValue
EmitDxOpValue(std::string name, hlsl::OP::OpCode dxilOpCode,
              hlsl::DXIL::ComponentType resultComponentType,
              std::vector<DxilRewriteEmitOperand> operands) {
  DxilRewriteEmitValue value =
      EmitDxOpValue(std::move(name), dxilOpCode, std::move(operands));
  value.hasExplicitResultComponentType = true;
  value.resultComponentType = resultComponentType;
  return value;
}

inline DxilRewriteEmitValue EmitExtractValue(std::string name,
                                             std::string aggregateName,
                                             unsigned extractIndex) {
  DxilRewriteEmitValue value;
  value.name = std::move(name);
  value.kind = DxilRewriteEmitValueKind::ExtractValue;
  value.aggregateName = std::move(aggregateName);
  value.extractIndex = extractIndex;
  return value;
}

inline DxilRewriteEmitValue
EmitBinaryInstructionValue(std::string name, unsigned instructionOpcode,
                           hlsl::DXIL::ComponentType resultComponentType,
                           std::vector<DxilRewriteEmitOperand> operands) {
  DxilRewriteEmitValue value;
  value.name = std::move(name);
  value.kind = DxilRewriteEmitValueKind::BinaryInstruction;
  value.instructionOpcode = instructionOpcode;
  value.hasExplicitResultComponentType = true;
  value.resultComponentType = resultComponentType;
  value.operands = std::move(operands);
  return value;
}

inline DxilRewriteEmitValue
EmitCastInstructionValue(std::string name, unsigned castOpcode,
                         hlsl::DXIL::ComponentType resultComponentType,
                         std::vector<DxilRewriteEmitOperand> operands) {
  DxilRewriteEmitValue value;
  value.name = std::move(name);
  value.kind = DxilRewriteEmitValueKind::CastInstruction;
  value.castOpcode = castOpcode;
  value.hasExplicitResultComponentType = true;
  value.resultComponentType = resultComponentType;
  value.operands = std::move(operands);
  return value;
}

inline DxilRewriteEmitValue
EmitCreateHandleValue(std::string name, std::string resourceName,
                      ResourceBindingDesc resourceBinding) {
  DxilRewriteEmitValue value;
  value.name = std::move(name);
  value.kind = DxilRewriteEmitValueKind::CreateHandleForResource;
  value.resourceName = std::move(resourceName);
  value.resourceBinding = resourceBinding;
  return value;
}

inline DxilRewriteEmitValue
EmitAnnotateHandleValue(std::string name, std::string handleName,
                        std::string resourceName,
                        ResourceBindingDesc resourceBinding) {
  DxilRewriteEmitValue value;
  value.name = std::move(name);
  value.kind = DxilRewriteEmitValueKind::AnnotateHandleForResource;
  value.handleName = std::move(handleName);
  value.resourceName = std::move(resourceName);
  value.resourceBinding = resourceBinding;
  return value;
}

class RenderTargetStoreBuilder {
public:
  explicit RenderTargetStoreBuilder(RenderTargetStoreDesc desc)
      : desc_(std::move(desc)) {}

  RenderTargetStoreBuilder &Target(unsigned outputSigId) {
    desc_.outputSigId = outputSigId;
    return *this;
  }

  RenderTargetStoreBuilder &Row(unsigned rowIndex) {
    desc_.rowIndex = rowIndex;
    return *this;
  }

  RenderTargetStoreBuilder &Component(unsigned componentIndex) {
    desc_.componentIndex = componentIndex;
    return *this;
  }

  RenderTargetStoreBuilder &R() { return Component(0); }
  RenderTargetStoreBuilder &G() { return Component(1); }
  RenderTargetStoreBuilder &B() { return Component(2); }
  RenderTargetStoreBuilder &A() { return Component(3); }

  RenderTargetStoreDesc Build() const { return desc_; }

  operator RenderTargetStoreDesc() const { return Build(); }

private:
  RenderTargetStoreDesc desc_;
};

inline RenderTargetStoreBuilder RenderTarget(unsigned outputSigId = 0) {
  RenderTargetStoreDesc desc;
  desc.outputSigId = outputSigId;
  return RenderTargetStoreBuilder(std::move(desc));
}

inline DxilCallPatternBuilder
RenderTargetStoreCall(const RenderTargetStoreDesc &desc) {
  return DxOpCall(hlsl::OP::OpCode::StoreOutput)
      .Args({ConstantIntOperand(1, desc.outputSigId),
             ConstantIntOperand(2, desc.rowIndex),
             ConstantIntOperand(3, desc.componentIndex), AnyOperand(4)});
}

inline DxilRewriteRuleBuilder RewriteRule(std::string name) {
  return DxilRewriteRuleBuilder(std::move(name));
}

bool FindDxilCallMatch(llvm::Function &function, const DxilCallPattern &pattern,
                       DxilMatchResult &result,
                       hlsl::DxilModule *dxilModule = nullptr);
unsigned CollectDxilCallMatches(llvm::Function &function,
                                const DxilCallPattern &pattern,
                                std::vector<DxilMatchResult> &results,
                                hlsl::DxilModule *dxilModule = nullptr);
bool ApplyDxilRewriteRules(llvm::Function &function, llvm::Module &module,
                           hlsl::DxilModule &dxilModule,
                           const std::vector<DxilRewriteRule> &rules,
                           unsigned *appliedRuleCount = nullptr);
void PruneInstructionRoots(const std::vector<llvm::Instruction *> &roots);
void PruneFunctionDeadCode(llvm::Function &function);
bool InjectTextureSampleIntoEntryPoint(llvm::Module &module,
                                       hlsl::DxilModule &dxilModule,
                                       const TextureResourceDesc &desc,
                                       bool traceEnabled = false);