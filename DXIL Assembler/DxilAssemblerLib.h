#pragma once

#include <any>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

#include "dxc/DXIL/DxilCBuffer.h"
#include "dxc/DXIL/DxilCompType.h"
#include "dxc/DXIL/DxilConstants.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilResource.h"
#include "dxc/DXIL/DxilResourceBinding.h"
#include "dxc/DXIL/DxilSampler.h"
#include "dxc/DXIL/DxilTypeSystem.h"

static constexpr unsigned kDxilRecipeAutoBinding = static_cast<unsigned>(-1);

struct ResourceBindingDesc {
  static ResourceBindingDesc SRV(unsigned bindPoint = 0, unsigned space = 0) {
    return ResourceBindingDesc(hlsl::DXIL::ResourceClass::SRV, bindPoint, space);
  }

  static ResourceBindingDesc UAV(unsigned bindPoint = 0, unsigned space = 0) {
    return ResourceBindingDesc(hlsl::DXIL::ResourceClass::UAV, bindPoint, space);
  }

  static ResourceBindingDesc CBuffer(unsigned bindPoint = 0,
                                     unsigned space = 0) {
    return ResourceBindingDesc(
        hlsl::DXIL::ResourceClass::CBuffer, bindPoint, space);
  }

  static ResourceBindingDesc Sampler(unsigned bindPoint = 0,
                                     unsigned space = 0) {
    return ResourceBindingDesc(
        hlsl::DXIL::ResourceClass::Sampler, bindPoint, space);
  }

  static ResourceBindingDesc AutoSRV(unsigned space = 0) {
    return SRV(kDxilRecipeAutoBinding, space);
  }

  static ResourceBindingDesc AutoUAV(unsigned space = 0) {
    return UAV(kDxilRecipeAutoBinding, space);
  }

  static ResourceBindingDesc AutoCBuffer(unsigned space = 0) {
    return CBuffer(kDxilRecipeAutoBinding, space);
  }

  static ResourceBindingDesc AutoSampler(unsigned space = 0) {
    return Sampler(kDxilRecipeAutoBinding, space);
  }

  explicit ResourceBindingDesc(
      hlsl::DXIL::ResourceClass resourceClass = hlsl::DXIL::ResourceClass::Invalid,
      unsigned bindPoint = 0,
      unsigned space = 0) {
    Set(bindPoint, space, resourceClass);
  }

  void Set(unsigned bindPoint,
           unsigned space,
           hlsl::DXIL::ResourceClass resourceClass) {
    dxilBinding.rangeLowerBound = bindPoint;
    dxilBinding.rangeUpperBound = bindPoint;
    dxilBinding.spaceID = space;
    dxilBinding.resourceClass = static_cast<uint8_t>(resourceClass);
    dxilBinding.Reserved1 = 0;
    dxilBinding.Reserved2 = 0;
    dxilBinding.Reserved3 = 0;
  }

  unsigned GetBindPoint() const { return dxilBinding.rangeLowerBound; }
  unsigned GetSpace() const { return dxilBinding.spaceID; }
  bool IsAutoBinding() const { return GetBindPoint() == kDxilRecipeAutoBinding; }
  hlsl::DXIL::ResourceClass GetResourceClass() const {
    return static_cast<hlsl::DXIL::ResourceClass>(dxilBinding.resourceClass);
  }

  ResourceBindingDesc &Register(unsigned bindPoint, unsigned space) {
    SetBindPoint(bindPoint);
    SetSpace(space);
    return *this;
  }

  ResourceBindingDesc &Auto(unsigned space = 0) {
    return Register(kDxilRecipeAutoBinding, space);
  }

  void SetBindPoint(unsigned bindPoint) {
    dxilBinding.rangeLowerBound = bindPoint;
    dxilBinding.rangeUpperBound = bindPoint;
  }

  void SetSpace(unsigned space) { dxilBinding.spaceID = space; }

  void SetResourceClass(hlsl::DXIL::ResourceClass resourceClass) {
    dxilBinding.resourceClass = static_cast<uint8_t>(resourceClass);
  }

  ResourceBindingDesc &AsSRV() {
    SetResourceClass(hlsl::DXIL::ResourceClass::SRV);
    return *this;
  }

  ResourceBindingDesc &AsUAV() {
    SetResourceClass(hlsl::DXIL::ResourceClass::UAV);
    return *this;
  }

  ResourceBindingDesc &AsCBuffer() {
    SetResourceClass(hlsl::DXIL::ResourceClass::CBuffer);
    return *this;
  }

  ResourceBindingDesc &AsSampler() {
    SetResourceClass(hlsl::DXIL::ResourceClass::Sampler);
    return *this;
  }

  const hlsl::DxilResourceBinding &GetDxilBinding() const { return dxilBinding; }

  hlsl::DxilResourceBinding dxilBinding = {};
};

struct CBufferDesc {
  std::string name;
  ResourceBindingDesc binding =
      ResourceBindingDesc(hlsl::DXIL::ResourceClass::CBuffer);
  unsigned sizeInBytes = 0;
  const struct CBufferSchema *schema = nullptr;
};

struct CBufferFieldDesc {
  std::string name;
  hlsl::CompType::Kind compType = hlsl::CompType::getU32().GetKind();
  unsigned vectorSize = 1;
  unsigned offset = 0;
};

struct CBufferSchema {
  std::string typeName;
  unsigned sizeInBytes = 0;
  std::vector<CBufferFieldDesc> fields;
};

template <typename TStruct>
class CBufferSchemaBuilder {
public:
  explicit CBufferSchemaBuilder(std::string typeName) {
    static_assert(std::is_standard_layout<TStruct>::value,
                  "CBuffer schema types must be standard-layout.");
    schema_.typeName = std::move(typeName);
    schema_.sizeInBytes = static_cast<unsigned>(sizeof(TStruct));
  }

  CBufferSchemaBuilder &Float(const std::string &name, unsigned offset) {
    return AddField(name, hlsl::CompType::getF32().GetKind(), 1, offset);
  }

  CBufferSchemaBuilder &Float2(const std::string &name, unsigned offset) {
    return AddField(name, hlsl::CompType::getF32().GetKind(), 2, offset);
  }

  CBufferSchemaBuilder &Float3(const std::string &name, unsigned offset) {
    return AddField(name, hlsl::CompType::getF32().GetKind(), 3, offset);
  }

  CBufferSchemaBuilder &Float4(const std::string &name, unsigned offset) {
    return AddField(name, hlsl::CompType::getF32().GetKind(), 4, offset);
  }

  CBufferSchemaBuilder &UInt(const std::string &name, unsigned offset) {
    return AddField(name, hlsl::CompType::getU32().GetKind(), 1, offset);
  }

  CBufferSchemaBuilder &UInt2(const std::string &name, unsigned offset) {
    return AddField(name, hlsl::CompType::getU32().GetKind(), 2, offset);
  }

  CBufferSchemaBuilder &UInt3(const std::string &name, unsigned offset) {
    return AddField(name, hlsl::CompType::getU32().GetKind(), 3, offset);
  }

  CBufferSchemaBuilder &UInt4(const std::string &name, unsigned offset) {
    return AddField(name, hlsl::CompType::getU32().GetKind(), 4, offset);
  }

  CBufferSchemaBuilder &Int(const std::string &name, unsigned offset) {
    return AddField(name, hlsl::CompType::getI32().GetKind(), 1, offset);
  }

  CBufferSchemaBuilder &Int2(const std::string &name, unsigned offset) {
    return AddField(name, hlsl::CompType::getI32().GetKind(), 2, offset);
  }

  CBufferSchemaBuilder &Int3(const std::string &name, unsigned offset) {
    return AddField(name, hlsl::CompType::getI32().GetKind(), 3, offset);
  }

  CBufferSchemaBuilder &Int4(const std::string &name, unsigned offset) {
    return AddField(name, hlsl::CompType::getI32().GetKind(), 4, offset);
  }

  CBufferSchema Build() { return std::move(schema_); }

private:
  CBufferSchemaBuilder &AddField(const std::string &name,
                                 hlsl::CompType::Kind compType,
                                 unsigned vectorSize,
                                 unsigned offset) {
    schema_.fields.push_back({name, compType, vectorSize, offset});
    return *this;
  }

  CBufferSchema schema_;
};

struct TextureResourceDesc {
  std::string name;
  ResourceBindingDesc binding =
      ResourceBindingDesc(hlsl::DXIL::ResourceClass::SRV);
  hlsl::DXIL::ResourceKind kind = hlsl::DXIL::ResourceKind::Texture2D;
  hlsl::DXIL::ComponentType elementKind = hlsl::DXIL::ComponentType::F32;
  unsigned vectorWidth = 4;
  bool isReadWrite = false;
};

class TextureResourceBuilder {
public:
  explicit TextureResourceBuilder(std::string name) {
    desc_.name = std::move(name);
  }

  TextureResourceBuilder &SRV() {
    desc_.binding.AsSRV();
    desc_.isReadWrite = false;
    return *this;
  }

  TextureResourceBuilder &UAV() {
    desc_.binding.AsUAV();
    desc_.isReadWrite = true;
    return *this;
  }

  TextureResourceBuilder &Texture2D() {
    desc_.kind = hlsl::DXIL::ResourceKind::Texture2D;
    return *this;
  }

  TextureResourceBuilder &Texture2DArray() {
    desc_.kind = hlsl::DXIL::ResourceKind::Texture2DArray;
    return *this;
  }

  TextureResourceBuilder &RWTexture2D() {
    return UAV().Texture2D();
  }

  TextureResourceBuilder &RWTexture2DArray() {
    return UAV().Texture2DArray();
  }

  TextureResourceBuilder &Float(unsigned vectorWidth = 1) {
    return Element(hlsl::DXIL::ComponentType::F32, vectorWidth);
  }

  TextureResourceBuilder &Float2() { return Float(2); }
  TextureResourceBuilder &Float3() { return Float(3); }
  TextureResourceBuilder &Float4() { return Float(4); }

  TextureResourceBuilder &UInt(unsigned vectorWidth = 1) {
    return Element(hlsl::DXIL::ComponentType::U32, vectorWidth);
  }

  TextureResourceBuilder &UInt2() { return UInt(2); }
  TextureResourceBuilder &UInt3() { return UInt(3); }
  TextureResourceBuilder &UInt4() { return UInt(4); }

  TextureResourceBuilder &Int(unsigned vectorWidth = 1) {
    return Element(hlsl::DXIL::ComponentType::I32, vectorWidth);
  }

  TextureResourceBuilder &Int2() { return Int(2); }
  TextureResourceBuilder &Int3() { return Int(3); }
  TextureResourceBuilder &Int4() { return Int(4); }

  TextureResourceBuilder &Register(unsigned bindPoint, unsigned space = 0) {
    desc_.binding.Register(bindPoint, space);
    return *this;
  }

  TextureResourceBuilder &Space(unsigned space) {
    desc_.binding.SetSpace(space);
    return *this;
  }

  TextureResourceBuilder &AutoBinding(unsigned space = 0) {
    desc_.binding.Auto(space);
    return *this;
  }

  TextureResourceBuilder &Element(hlsl::DXIL::ComponentType elementKind,
                                  unsigned vectorWidth) {
    desc_.elementKind = elementKind;
    desc_.vectorWidth = vectorWidth;
    return *this;
  }

  TextureResourceDesc Build() { return desc_; }

private:
  TextureResourceDesc desc_;
};

struct SamplerDesc {
  std::string name;
  ResourceBindingDesc binding =
      ResourceBindingDesc(hlsl::DXIL::ResourceClass::Sampler);
};

std::string MakeUniqueGlobalName(const llvm::Module &module,
                                 const std::string &baseName);

template <typename TResource>
unsigned FindNextAvailableBinding(
    const std::vector<std::unique_ptr<TResource>> &resources,
    unsigned space,
    unsigned preferredBindPoint) {
  unsigned bindPoint = preferredBindPoint;
  while (true) {
    bool conflict = false;
    for (const auto &resource : resources) {
      if (resource->GetSpaceID() == space &&
          resource->GetLowerBound() == bindPoint) {
        conflict = true;
        break;
      }
    }

    if (!conflict)
      return bindPoint;

    ++bindPoint;
  }
}

bool AddCBuffer(llvm::Module &module,
                hlsl::DxilModule &dxilModule,
                const CBufferDesc &desc);
bool AddTextureSRV(llvm::Module &module,
                   hlsl::DxilModule &dxilModule,
                   const TextureResourceDesc &desc);
bool AddTextureUAV(llvm::Module &module,
                   hlsl::DxilModule &dxilModule,
                   const TextureResourceDesc &desc);
bool AddTexture2DSRV(llvm::Module &module,
                     hlsl::DxilModule &dxilModule,
                     const TextureResourceDesc &desc);
bool AddSampler(llvm::Module &module,
                hlsl::DxilModule &dxilModule,
                const SamplerDesc &desc);

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

  DxilRewriteResultBuilder &Prune(
      const std::vector<llvm::Instruction *> &instructions) {
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
    const DxilMatchResult &,
    llvm::IRBuilder<> &,
    llvm::Module &,
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

  DxilOperandPatternBuilder &Args(std::vector<DxilOperandPattern> operandPatterns) {
    pattern_.operandPatterns = std::move(operandPatterns);
    return *this;
  }

  DxilOperandPatternBuilder &Args(
      std::initializer_list<DxilOperandPattern> operandPatterns) {
    pattern_.operandPatterns.assign(operandPatterns.begin(), operandPatterns.end());
    return *this;
  }

  DxilOperandPatternBuilder &ResourceClass(hlsl::DXIL::ResourceClass resourceClass) {
    pattern_.matchResourceClass = true;
    pattern_.resourceClass = resourceClass;
    return *this;
  }

  DxilOperandPatternBuilder &AnyTexture() {
    pattern_.matchAnyTexture = true;
    return *this;
  }

  DxilOperandPatternBuilder &ResourceKind(hlsl::DXIL::ResourceKind resourceKind) {
    pattern_.matchResourceKind = true;
    pattern_.resourceKind = resourceKind;
    return *this;
  }

  DxilOperandPatternBuilder &ResourceName(std::string resourceName) {
    pattern_.resourceName = std::move(resourceName);
    return *this;
  }

  DxilOperandPatternBuilder &ResourceNameLike(std::string resourceNameLikePattern) {
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

  DxilCallPatternBuilder &Args(std::vector<DxilOperandPattern> operandPatterns) {
    pattern_.operandPatterns = std::move(operandPatterns);
    return *this;
  }

  DxilCallPatternBuilder &Args(
      std::initializer_list<DxilOperandPattern> operandPatterns) {
    pattern_.operandPatterns.assign(operandPatterns.begin(), operandPatterns.end());
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

  DxilRewriteRuleBuilder &EmitOperands(
      std::vector<DxilRewriteEmitOperand> operands) {
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

inline DxilOperandPatternBuilder InstructionOperand(unsigned operandIndex,
                                                    unsigned instructionOpcode) {
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

inline DxilRewriteEmitOperand EmitConstantIntOperand(unsigned operandIndex,
                                                     uint64_t constantIntValue) {
  DxilRewriteEmitOperand operand;
  operand.operandIndex = operandIndex;
  operand.kind = DxilRewriteEmitOperandKind::ConstantInt;
  operand.constantIntValue = constantIntValue;
  return operand;
}

inline DxilRewriteEmitOperand EmitResourceHandleOperand(
    unsigned operandIndex,
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

inline DxilRewriteEmitValue EmitDxOpValue(
    std::string name,
    hlsl::OP::OpCode dxilOpCode,
    std::vector<DxilRewriteEmitOperand> operands = {}) {
  DxilRewriteEmitValue value;
  value.name = std::move(name);
  value.kind = DxilRewriteEmitValueKind::DxOpCall;
  value.dxilOpCode = dxilOpCode;
  value.operands = std::move(operands);
  return value;
}

inline DxilRewriteEmitValue EmitDxOpValue(
    std::string name,
    hlsl::OP::OpCode dxilOpCode,
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

inline DxilRewriteEmitValue EmitBinaryInstructionValue(
    std::string name,
    unsigned instructionOpcode,
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

inline DxilRewriteEmitValue EmitCastInstructionValue(
    std::string name,
    unsigned castOpcode,
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

inline DxilRewriteEmitValue EmitCreateHandleValue(std::string name,
                                                  std::string resourceName,
                                                  ResourceBindingDesc resourceBinding) {
  DxilRewriteEmitValue value;
  value.name = std::move(name);
  value.kind = DxilRewriteEmitValueKind::CreateHandleForResource;
  value.resourceName = std::move(resourceName);
  value.resourceBinding = resourceBinding;
  return value;
}

inline DxilRewriteEmitValue EmitAnnotateHandleValue(
    std::string name,
    std::string handleName,
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

inline DxilCallPatternBuilder RenderTargetStoreCall(
    const RenderTargetStoreDesc &desc) {
  return DxOpCall(hlsl::OP::OpCode::StoreOutput)
      .Args({ConstantIntOperand(1, desc.outputSigId),
             ConstantIntOperand(2, desc.rowIndex),
             ConstantIntOperand(3, desc.componentIndex), AnyOperand(4)});
}

inline DxilRewriteRuleBuilder RewriteRule(std::string name) {
  return DxilRewriteRuleBuilder(std::move(name));
}

enum class DxilRecipeRuleApplicationMode {
  Once,
  UntilNoMatch,
};

struct DxilRecipeExecutionOptions {
  bool traceEnabled = false;
  std::unordered_map<std::string, std::any> inputs;
  std::unordered_map<std::string, std::any> initialState;
};

struct DxilLoadedShaderState {
  std::vector<uint8_t> inputBytes;
  llvm::LLVMContext context;
  std::unique_ptr<llvm::LLVMContext> reflectionContext;
  std::unique_ptr<llvm::Module> module;
  hlsl::DxilModule *dxilModule = nullptr;
};

struct DxilContainerPatchOptions {
  bool restoreReflection = true;
  bool refreshResources = true;
  bool verifyModule = true;
  DxilRecipeExecutionOptions recipeExecutionOptions;
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

  template <typename TValue>
  TValue *FindInput(const std::string &name) {
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

  template <typename TValue>
  TValue *FindState(const std::string &name) {
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

using DxilRecipeStepExecutor = std::function<DxilRecipeStepResult(DxilRecipeContext &)>;

DxilRecipeStepResult MakeRecipeStepSuccess(
    bool changed = false,
    unsigned matchCount = 0,
    bool invalidatedAnalyses = false);
DxilRecipeStepResult MakeRecipeStepFailure(
    DxilRecipeContext &context,
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

struct DxilRecipeParseResult {
  DxilRecipe recipe;
  DxilContainerPatchOptions patchOptions;
  std::string error;
};

DxilRecipeStep MakeCustomRecipeStep(
    std::string name,
    DxilRecipeStepExecutor execute);
DxilRecipeStep MakeAddTextureStep(std::string id, TextureResourceDesc desc);
DxilRecipeStep MakeAddTextureUAVStep(std::string id, TextureResourceDesc desc);
DxilRecipeStep MakeAddCBufferStep(std::string id, CBufferDesc desc);
DxilRecipeStep MakeAddSamplerStep(std::string id, SamplerDesc desc);
DxilRecipeStep MakeApplyRewriteRulesStep(
    std::string name,
    std::vector<DxilRewriteRule> rules,
    DxilRecipeRuleApplicationMode mode = DxilRecipeRuleApplicationMode::Once,
    bool required = true);
DxilRecipeStep MakeRefreshResourcesStep(
    std::string name = "refresh_resources");
DxilRecipeStep MakePruneDeadCodeStep(
  std::string name = "prune_dead_code");
DxilRecipeStep MakeVerifyModuleStep(
    std::string name = "verify_module");
DxilRecipeStep MakeExpectTextureStep(
    std::string id,
    std::string name = "expect_texture");
DxilRecipeStep MakeExpectTextureUAVStep(
  std::string id,
  std::string name = "expect_texture_uav");
DxilRecipeStep MakeExpectCBufferStep(
    std::string id,
    std::string name = "expect_cbuffer");
bool ExecuteDxilRecipe(const DxilRecipe &recipe,
                       llvm::Module &module,
                       hlsl::DxilModule &dxilModule,
                       DxilRecipeContext *outContext = nullptr,
                       bool traceEnabled = false);
bool ExecuteDxilRecipe(const DxilRecipe &recipe,
                       llvm::Module &module,
                       hlsl::DxilModule &dxilModule,
                       const DxilRecipeExecutionOptions &options,
                       DxilRecipeContext *outContext = nullptr);

bool FindDxilCallMatch(llvm::Function &function,
                       const DxilCallPattern &pattern,
                       DxilMatchResult &result,
                       hlsl::DxilModule *dxilModule = nullptr);
unsigned CollectDxilCallMatches(llvm::Function &function,
                                const DxilCallPattern &pattern,
                                std::vector<DxilMatchResult> &results,
                                hlsl::DxilModule *dxilModule = nullptr);
bool ApplyDxilRewriteRules(llvm::Function &function,
                           llvm::Module &module,
                           hlsl::DxilModule &dxilModule,
                           const std::vector<DxilRewriteRule> &rules,
                           unsigned *appliedRuleCount = nullptr);
void PruneInstructionRoots(const std::vector<llvm::Instruction *> &roots);
void PruneFunctionDeadCode(llvm::Function &function);
bool InjectTextureSampleIntoEntryPoint(llvm::Module &module,
                                       hlsl::DxilModule &dxilModule,
                                       const TextureResourceDesc &desc,
                                       bool traceEnabled = false);
bool LoadDxilContainerForMutation(const void *containerData,
                  size_t containerSize,
                  DxilLoadedShaderState &shader,
                  bool restoreReflection = true);
bool LoadDxilContainerForMutation(const std::vector<uint8_t> &containerBytes,
                  DxilLoadedShaderState &shader,
                  bool restoreReflection = true);
bool ReloadDxilContainerFromMemory(const std::vector<uint8_t> &containerBytes,
                   llvm::LLVMContext &context,
                   std::unique_ptr<llvm::Module> &module,
                   hlsl::DxilModule *&dxilModule);
bool PatchDxilContainerInMemory(const DxilRecipe &recipe,
                const void *inputData,
                size_t inputSize,
                std::vector<uint8_t> &outputContainer,
                const DxilContainerPatchOptions &options = {},
                DxilRecipeContext *outContext = nullptr);
bool PatchDxilContainerInMemory(
  const DxilRecipe &recipe,
  const std::vector<uint8_t> &inputContainer,
  std::vector<uint8_t> &outputContainer,
  const DxilContainerPatchOptions &options = {},
  DxilRecipeContext *outContext = nullptr);
bool ParseDxilRecipeText(llvm::StringRef recipeText,
                         DxilRecipeParseResult &result,
                         llvm::StringRef sourceName = "recipe");
bool ParseDxilRecipeFile(const std::string &recipePath,
                         DxilRecipeParseResult &result);
void RefreshDxilAfterResourceMutation(hlsl::DxilModule &dxilModule,
                                      bool traceEnabled = false);
std::vector<uint8_t> SerializeModuleToBitcode(llvm::Module &module);
bool SerializePatchedContainer(hlsl::DxilModule &dxilModule,
                               const std::vector<uint8_t> &moduleBitcode,
                               std::vector<uint8_t> &outputContainer);
void RestoreOriginalResourceReflection(
    const std::vector<uint8_t> &inputBytes,
    hlsl::DxilModule &targetDxilModule,
    llvm::LLVMContext &reflectionContext);
