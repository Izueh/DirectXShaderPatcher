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

/// @brief Identifies the kind of DXIL operand pattern to match.
enum class DxilOperandPatternKind {
  Any,
  DxOpCall,
  Instruction,
  ResourceHandle,
  ConstantInt,
};

/// @brief Describes one operand constraint in a DXIL call pattern.
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

/// @brief Describes a DXIL call pattern to match.
struct DxilCallPattern {
  std::string calleeName;
  bool matchDxilOpCode = false;
  hlsl::OP::OpCode dxilOpCode = static_cast<hlsl::OP::OpCode>(0);
  std::string captureName;
  std::vector<DxilOperandPattern> operandPatterns;
};

/// @brief Identifies a render-target output component.
struct RenderTargetStoreDesc {
  unsigned outputSigId = 0;
  unsigned rowIndex = 0;
  unsigned componentIndex = 0;
};

/// @brief Stores one DXIL pattern match and its captured values.
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

/// @brief Selects how a DXIL rewrite is applied.
///
/// Replace rewrites the full matched instruction window. ReplaceRange rewrites
/// only the sub-window selected by rangeStartOffset and rangeEndOffset within
/// that matched window.
enum class DxilRewriteMode {
  None,
  Before,
  After,
  Replace,
  ReplaceRange,
};

/// @brief Reports the result of applying one DXIL rewrite.
///
/// replacementValue is consumed by the engine for Replace and ReplaceRange when
/// handledReplacement is false. pruneRoots names additional instructions the
/// callback or declarative rewrite made dead and wants the engine to prune.
struct DxilRewriteResult {
  bool success = true;
  bool handledReplacement = false;
  llvm::Value *replacementValue = nullptr;
  std::vector<llvm::Instruction *> pruneRoots;
};

/// @brief Identifies how an emitted operand is sourced.
enum class DxilRewriteEmitOperandKind {
  Capture,
  Temporary,
  ConstantInt,
  ResourceHandle,
  Undef,
};

/// @brief Describes one operand used by emitted rewrite code.
struct DxilRewriteEmitOperand {
  unsigned operandIndex = 0;
  DxilRewriteEmitOperandKind kind = DxilRewriteEmitOperandKind::Capture;
  std::string captureName;
  std::string temporaryName;
  uint64_t constantIntValue = 0;
  std::string resourceName;
  ResourceBindingDesc resourceBinding;
};

/// @brief Identifies the kind of emitted value to build.
enum class DxilRewriteEmitValueKind {
  DxOpCall,
  ExtractValue,
  BinaryInstruction,
  CastInstruction,
  CreateHandleForResource,
  AnnotateHandleForResource,
};

/// @brief Describes one emitted intermediate value in a rewrite sequence.
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

/// @brief Describes a sequence of emitted values and the final replacement.
struct DxilRewriteEmitSequence {
  std::vector<DxilRewriteEmitValue> values;
  std::string replacementValueName;
};

/// @brief Describes a single emitted DXIL operation call.
struct DxilRewriteEmitCall {
  bool enabled = false;
  hlsl::OP::OpCode dxilOpCode = static_cast<hlsl::OP::OpCode>(0);
  int extractIndex = -1;
  std::vector<DxilRewriteEmitOperand> operands;
};

/// @brief Summarizes how one DXIL rewrite rule matched and applied.
struct DxilRuleApplicationReport {
  std::string name;
  unsigned matchCount = 0;
  unsigned appliedCount = 0;
  unsigned mutatedCount = 0;
};

/// @brief Fluent builder for DxilRewriteResult values.
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

/// @brief Creates a fluent builder for a rewrite result.
inline DxilRewriteResultBuilder RewriteResult() {
  return DxilRewriteResultBuilder();
}

/// @brief Predicate signature used to filter DXIL matches.
using DxilMatchPredicate = std::function<bool(const DxilMatchResult &)>;
/// @brief Callback signature used to build custom DXIL rewrites.
///
/// Callbacks should report cleanup through DxilRewriteResult rather than
/// pruning directly. When handledReplacement is false, the engine applies the
/// Replace or ReplaceRange contract using replacementValue. When it is true,
/// the callback has already performed the replacement work and should return
/// any extra prune roots through pruneRoots.
using DxilRewriteCallback = std::function<DxilRewriteResult(
    const DxilMatchResult &, llvm::IRBuilder<> &, llvm::Module &,
    hlsl::DxilModule &)>;

/// @brief Describes one DXIL rewrite rule.
struct DxilRewriteRule {
  std::string name;
  DxilCallPattern pattern;
  std::vector<DxilCallPattern> bindingPatterns;
  DxilMatchPredicate predicate;
  DxilRewriteMode mode = DxilRewriteMode::Replace;
  std::string replaceCaptureName;
  int32_t rangeStartOffset = 0;
  int32_t rangeEndOffset = -1;
  std::string replacementCaptureName;
  DxilRewriteEmitCall emittedCall;
  DxilRewriteEmitSequence emittedSequence;
  std::vector<std::string> pruneCaptureNames;
  /// Automatically prune matched captured instructions that become dead after
  /// generic Replace or ReplaceRange. Explicit DxilRewriteResult pruneRoots are
  /// honored regardless of this flag.
  bool pruneDeadInstructions = true;
  DxilRewriteCallback replacementCallback;
};

/// @brief Fluent builder for DxilOperandPattern values.
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

/// @brief Fluent builder for DxilCallPattern values.
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

/// @brief Fluent builder for DxilRewriteRule values.
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

  DxilRewriteRuleBuilder &RangeStartOffset(int32_t offset) {
    rule_.rangeStartOffset = offset;
    return *this;
  }

  DxilRewriteRuleBuilder &RangeEndOffset(int32_t offset) {
    rule_.rangeEndOffset = offset;
    return *this;
  }

  DxilRewriteRuleBuilder &RangeOffsets(int32_t startOffset,
                                       int32_t endOffset) {
    rule_.rangeStartOffset = startOffset;
    rule_.rangeEndOffset = endOffset;
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

/// @brief Creates a call pattern for `dx.op.storeOutput`.
inline DxilCallPatternBuilder
RenderTargetStoreCall(const RenderTargetStoreDesc &desc) {
  return DxOpCall(hlsl::OP::OpCode::StoreOutput)
      .Args({ConstantIntOperand(1, desc.outputSigId),
             ConstantIntOperand(2, desc.rowIndex),
             ConstantIntOperand(3, desc.componentIndex), AnyOperand(4)});
}

/// @brief Creates a fluent builder for a named rewrite rule.
inline DxilRewriteRuleBuilder RewriteRule(std::string name) {
  return DxilRewriteRuleBuilder(std::move(name));
}

/// @brief Finds the first call in a function that matches a pattern.
bool FindDxilCallMatch(llvm::Function &function, const DxilCallPattern &pattern,
                       DxilMatchResult &result,
                       hlsl::DxilModule *dxilModule = nullptr);

/// @brief Collects all calls in a function that match a pattern.
unsigned CollectDxilCallMatches(llvm::Function &function,
                                const DxilCallPattern &pattern,
                                std::vector<DxilMatchResult> &results,
                                hlsl::DxilModule *dxilModule = nullptr);

/// @brief Applies all matching rewrite rules in one pass.
bool ApplyDxilRewriteRulesMatchAll(llvm::Function &function,
                                   llvm::Module &module,
                                   hlsl::DxilModule &dxilModule,
                                   const std::vector<DxilRewriteRule> &rules,
                                   unsigned *appliedRuleCount = nullptr,
                                   unsigned *mutatedRuleCount = nullptr,
                                   std::vector<DxilRuleApplicationReport> *ruleReports = nullptr);

/// @brief Applies the first or last matching rewrite rule once.
bool ApplyDxilRewriteRulesOnce(llvm::Function &function, llvm::Module &module,
                               hlsl::DxilModule &dxilModule,
                               const std::vector<DxilRewriteRule> &rules,
                               bool useLastMatch = false,
                               unsigned *appliedRuleCount = nullptr,
                               unsigned *mutatedRuleCount = nullptr,
                               std::vector<DxilRuleApplicationReport> *ruleReports = nullptr);

/// @brief Applies rewrite rules using the default match mode.
bool ApplyDxilRewriteRules(llvm::Function &function, llvm::Module &module,
                           hlsl::DxilModule &dxilModule,
                           const std::vector<DxilRewriteRule> &rules,
                           unsigned *appliedRuleCount = nullptr,
                           unsigned *mutatedRuleCount = nullptr,
                           std::vector<DxilRuleApplicationReport> *ruleReports = nullptr);

/// @brief Prunes instructions reachable from the supplied roots when dead.
void PruneInstructionRoots(const std::vector<llvm::Instruction *> &roots);

/// @brief Removes dead code from a function after rewrites.
void PruneFunctionDeadCode(llvm::Function &function);

/// @brief Injects a texture sample sequence into the shader entry point.
bool InjectTextureSampleIntoEntryPoint(llvm::Module &module,
                                       hlsl::DxilModule &dxilModule,
                                       const TextureResourceDesc &desc,
                                       bool traceEnabled = false);