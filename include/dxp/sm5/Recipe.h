#pragma once

#include "dxp/PatchReport.h"
#include "d3d11TokenizedProgramFormat.hpp"

#include <any>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dxp::sm5 {

struct Program;
struct Operand;
struct Instruction;

/// @brief Carries mutable state across SM5 recipe execution.
struct RecipeContext {
  Program *ProgramHandle = nullptr;
  bool TraceEnabled = false;
  uint32_t TotalRuleMatches = 0;
  bool ProgramModified = false;
  bool ResourceBindingsChanged = false;
  bool ResourcesRefreshed = false;
  bool ModuleVerified = false;
  uint32_t ReservedTempBase = 0;
  uint32_t ReservedTempCount = 0;
  std::unordered_map<std::string, uint32_t> TempBindings;
  std::unordered_map<std::string, uint32_t> InputBindings;
  std::unordered_map<std::string, uint32_t> OutputBindings;
  std::unordered_map<std::string, uint32_t> TextureBindings;
  std::unordered_map<std::string, uint32_t> RawResourceBindings;
  std::unordered_map<std::string, uint32_t> StructuredResourceBindings;
  std::unordered_map<std::string, uint32_t> CBufferBindings;
  std::unordered_map<std::string, uint32_t> SamplerBindings;
  std::unordered_map<std::string, uint32_t> UavBindings;
  std::string LastError;
  std::vector<std::string> Diagnostics;
  std::unordered_map<std::string, std::any> Inputs;
  std::unordered_map<std::string, std::any> State;

  void AddDiagnostic(std::string message) {
    Diagnostics.push_back(std::move(message));
  }

  template <typename TValue>
  void SetInput(const std::string &name, TValue value) {
    Inputs[name] = std::any(std::move(value));
  }

  template <typename TValue> TValue *FindInput(const std::string &name) {
    auto it = Inputs.find(name);
    if (it == Inputs.end()) {
      return nullptr;
    }
    return std::any_cast<TValue>(&it->second);
  }

  template <typename TValue>
  const TValue *FindInput(const std::string &name) const {
    auto it = Inputs.find(name);
    if (it == Inputs.end()) {
      return nullptr;
    }
    return std::any_cast<TValue>(&it->second);
  }

  template <typename TValue>
  void SetState(const std::string &name, TValue value) {
    State[name] = std::any(std::move(value));
  }

  template <typename TValue> TValue *FindState(const std::string &name) {
    auto it = State.find(name);
    if (it == State.end()) {
      return nullptr;
    }
    return std::any_cast<TValue>(&it->second);
  }

  template <typename TValue>
  const TValue *FindState(const std::string &name) const {
    auto it = State.find(name);
    if (it == State.end()) {
      return nullptr;
    }
    return std::any_cast<TValue>(&it->second);
  }
};

/// @brief Controls which match is rewritten when a rule matches more than once.
enum class RecipeRuleApplicationMode {
  First,
  Last,
  MatchAll,
};

/// @brief Selects how replacement instructions are applied.
enum class RecipeRuleRewriteMode {
  None,
  Replace,
  Before,
  After,
  ReplaceRange,
};

/// @brief Identifies the supported prefilter checks.
enum class PrefilterKind {
  CheckShaderVersion,
  CheckOpcodeCount,
  CheckResourceCount,
  CheckPatternMatch,
};

/// @brief Controls how multiple SM5 prefilter checks are combined.
enum class RecipePrefilterMode {
  All,
  Any,
};

/// @brief Declares a texture binding to add or reference in a recipe.
struct RecipeTextureDecl {
  uint32_t BindPoint = 0;
  uint32_t Dimension = D3D10_SB_RESOURCE_DIMENSION_TEXTURE2D;
  std::string Handle;
  bool AutoBind = false;

  RecipeTextureDecl &WithBindPoint(uint32_t bindPoint) & {
    BindPoint = bindPoint;
    return *this;
  }

  RecipeTextureDecl &&WithBindPoint(uint32_t bindPoint) && {
    BindPoint = bindPoint;
    return std::move(*this);
  }

  RecipeTextureDecl &WithDimension(uint32_t dimension) & {
    Dimension = dimension;
    return *this;
  }

  RecipeTextureDecl &&WithDimension(uint32_t dimension) && {
    Dimension = dimension;
    return std::move(*this);
  }

  RecipeTextureDecl &WithHandle(std::string handle) & {
    Handle = std::move(handle);
    return *this;
  }

  RecipeTextureDecl &&WithHandle(std::string handle) && {
    Handle = std::move(handle);
    return std::move(*this);
  }

  RecipeTextureDecl &AutoBindToNext(bool autoBind = true) & {
    AutoBind = autoBind;
    return *this;
  }

  RecipeTextureDecl &&AutoBindToNext(bool autoBind = true) && {
    AutoBind = autoBind;
    return std::move(*this);
  }
};

/// @brief Declares a temporary register handle used by a recipe.
struct RecipeTempDecl {
  std::string Handle;

  RecipeTempDecl &WithHandle(std::string handle) & {
    Handle = std::move(handle);
    return *this;
  }

  RecipeTempDecl &&WithHandle(std::string handle) && {
    Handle = std::move(handle);
    return std::move(*this);
  }
};

/// @brief Declares an input signature binding to add or reference.
struct RecipeInputDecl {
  uint32_t BindPoint = 0;
  uint32_t InterpolationMode = D3D10_SB_INTERPOLATION_LINEAR;
  std::string Handle;
  bool AutoBind = false;

  RecipeInputDecl &WithBindPoint(uint32_t bindPoint) & {
    BindPoint = bindPoint;
    return *this;
  }

  RecipeInputDecl &&WithBindPoint(uint32_t bindPoint) && {
    BindPoint = bindPoint;
    return std::move(*this);
  }

  RecipeInputDecl &WithInterpolationMode(uint32_t interpolationMode) & {
    InterpolationMode = interpolationMode;
    return *this;
  }

  RecipeInputDecl &&WithInterpolationMode(uint32_t interpolationMode) && {
    InterpolationMode = interpolationMode;
    return std::move(*this);
  }

  RecipeInputDecl &WithHandle(std::string handle) & {
    Handle = std::move(handle);
    return *this;
  }

  RecipeInputDecl &&WithHandle(std::string handle) && {
    Handle = std::move(handle);
    return std::move(*this);
  }

  RecipeInputDecl &AutoBindToNext(bool autoBind = true) & {
    AutoBind = autoBind;
    return *this;
  }

  RecipeInputDecl &&AutoBindToNext(bool autoBind = true) && {
    AutoBind = autoBind;
    return std::move(*this);
  }
};

/// @brief Declares an output signature binding to add or reference.
struct RecipeOutputDecl {
  uint32_t BindPoint = 0;
  std::string Handle;
  bool AutoBind = false;

  RecipeOutputDecl &WithBindPoint(uint32_t bindPoint) & {
    BindPoint = bindPoint;
    return *this;
  }

  RecipeOutputDecl &&WithBindPoint(uint32_t bindPoint) && {
    BindPoint = bindPoint;
    return std::move(*this);
  }

  RecipeOutputDecl &WithHandle(std::string handle) & {
    Handle = std::move(handle);
    return *this;
  }

  RecipeOutputDecl &&WithHandle(std::string handle) && {
    Handle = std::move(handle);
    return std::move(*this);
  }

  RecipeOutputDecl &AutoBindToNext(bool autoBind = true) & {
    AutoBind = autoBind;
    return *this;
  }

  RecipeOutputDecl &&AutoBindToNext(bool autoBind = true) && {
    AutoBind = autoBind;
    return std::move(*this);
  }
};

/// @brief Declares a constant buffer binding to add or reference.
struct RecipeCBufferDecl {
  uint32_t BindPoint = 0;
  uint32_t Elements = 1;
  uint32_t AccessPattern = D3D10_SB_CONSTANT_BUFFER_IMMEDIATE_INDEXED;
  std::string Handle;
  bool AutoBind = false;

  RecipeCBufferDecl &WithBindPoint(uint32_t bindPoint) & {
    BindPoint = bindPoint;
    return *this;
  }

  RecipeCBufferDecl &&WithBindPoint(uint32_t bindPoint) && {
    BindPoint = bindPoint;
    return std::move(*this);
  }

  RecipeCBufferDecl &WithElements(uint32_t elements) & {
    Elements = elements;
    return *this;
  }

  RecipeCBufferDecl &&WithElements(uint32_t elements) && {
    Elements = elements;
    return std::move(*this);
  }

  RecipeCBufferDecl &WithAccessPattern(uint32_t accessPattern) & {
    AccessPattern = accessPattern;
    return *this;
  }

  RecipeCBufferDecl &&WithAccessPattern(uint32_t accessPattern) && {
    AccessPattern = accessPattern;
    return std::move(*this);
  }

  RecipeCBufferDecl &WithHandle(std::string handle) & {
    Handle = std::move(handle);
    return *this;
  }

  RecipeCBufferDecl &&WithHandle(std::string handle) && {
    Handle = std::move(handle);
    return std::move(*this);
  }

  RecipeCBufferDecl &AutoBindToNext(bool autoBind = true) & {
    AutoBind = autoBind;
    return *this;
  }

  RecipeCBufferDecl &&AutoBindToNext(bool autoBind = true) && {
    AutoBind = autoBind;
    return std::move(*this);
  }
};

/// @brief Declares a sampler binding to add or reference.
struct RecipeSamplerDecl {
  uint32_t BindPoint = 0;
  uint32_t Mode = D3D10_SB_SAMPLER_MODE_DEFAULT;
  std::string Handle;
  bool AutoBind = false;

  RecipeSamplerDecl &WithBindPoint(uint32_t bindPoint) & {
    BindPoint = bindPoint;
    return *this;
  }

  RecipeSamplerDecl &&WithBindPoint(uint32_t bindPoint) && {
    BindPoint = bindPoint;
    return std::move(*this);
  }

  RecipeSamplerDecl &WithMode(uint32_t mode) & {
    Mode = mode;
    return *this;
  }

  RecipeSamplerDecl &&WithMode(uint32_t mode) && {
    Mode = mode;
    return std::move(*this);
  }

  RecipeSamplerDecl &WithHandle(std::string handle) & {
    Handle = std::move(handle);
    return *this;
  }

  RecipeSamplerDecl &&WithHandle(std::string handle) && {
    Handle = std::move(handle);
    return std::move(*this);
  }

  RecipeSamplerDecl &AutoBindToNext(bool autoBind = true) & {
    AutoBind = autoBind;
    return *this;
  }

  RecipeSamplerDecl &&AutoBindToNext(bool autoBind = true) && {
    AutoBind = autoBind;
    return std::move(*this);
  }
};

/// @brief Declares a raw resource binding to add or reference.
struct RecipeRawResourceDecl {
  uint32_t BindPoint = 0;
  std::string Handle;
  bool AutoBind = false;

  RecipeRawResourceDecl &WithBindPoint(uint32_t bindPoint) & {
    BindPoint = bindPoint;
    return *this;
  }

  RecipeRawResourceDecl &&WithBindPoint(uint32_t bindPoint) && {
    BindPoint = bindPoint;
    return std::move(*this);
  }

  RecipeRawResourceDecl &WithHandle(std::string handle) & {
    Handle = std::move(handle);
    return *this;
  }

  RecipeRawResourceDecl &&WithHandle(std::string handle) && {
    Handle = std::move(handle);
    return std::move(*this);
  }

  RecipeRawResourceDecl &AutoBindToNext(bool autoBind = true) & {
    AutoBind = autoBind;
    return *this;
  }

  RecipeRawResourceDecl &&AutoBindToNext(bool autoBind = true) && {
    AutoBind = autoBind;
    return std::move(*this);
  }
};

/// @brief Declares a structured resource binding to add or reference.
struct RecipeStructuredResourceDecl {
  uint32_t BindPoint = 0;
  uint32_t StructureStride = 16;
  std::string Handle;
  bool AutoBind = false;

  RecipeStructuredResourceDecl &WithBindPoint(uint32_t bindPoint) & {
    BindPoint = bindPoint;
    return *this;
  }

  RecipeStructuredResourceDecl &&WithBindPoint(uint32_t bindPoint) && {
    BindPoint = bindPoint;
    return std::move(*this);
  }

  RecipeStructuredResourceDecl &WithStructureStride(uint32_t structureStride) & {
    StructureStride = structureStride;
    return *this;
  }

  RecipeStructuredResourceDecl &&WithStructureStride(uint32_t structureStride) && {
    StructureStride = structureStride;
    return std::move(*this);
  }

  RecipeStructuredResourceDecl &WithHandle(std::string handle) & {
    Handle = std::move(handle);
    return *this;
  }

  RecipeStructuredResourceDecl &&WithHandle(std::string handle) && {
    Handle = std::move(handle);
    return std::move(*this);
  }

  RecipeStructuredResourceDecl &AutoBindToNext(bool autoBind = true) & {
    AutoBind = autoBind;
    return *this;
  }

  RecipeStructuredResourceDecl &&AutoBindToNext(bool autoBind = true) && {
    AutoBind = autoBind;
    return std::move(*this);
  }
};

/// @brief Identifies the UAV kind requested by a recipe declaration.
enum class RecipeUavKind {
  Typed,
  Raw,
  Structured,
};

/// @brief Declares a UAV binding to add or reference.
struct RecipeUavDecl {
  uint32_t BindPoint = 0;
  RecipeUavKind Kind = RecipeUavKind::Typed;
  uint32_t Dimension = D3D10_SB_RESOURCE_DIMENSION_TEXTURE2D;
  uint32_t StructureStride = 16;
  bool GloballyCoherent = false;
  bool HasOrderPreservingCounter = false;
  std::string Handle;
  bool AutoBind = false;

  RecipeUavDecl &WithBindPoint(uint32_t bindPoint) & {
    BindPoint = bindPoint;
    return *this;
  }

  RecipeUavDecl &&WithBindPoint(uint32_t bindPoint) && {
    BindPoint = bindPoint;
    return std::move(*this);
  }

  RecipeUavDecl &WithKind(RecipeUavKind kind) & {
    Kind = kind;
    return *this;
  }

  RecipeUavDecl &&WithKind(RecipeUavKind kind) && {
    Kind = kind;
    return std::move(*this);
  }

  RecipeUavDecl &WithDimension(uint32_t dimension) & {
    Dimension = dimension;
    return *this;
  }

  RecipeUavDecl &&WithDimension(uint32_t dimension) && {
    Dimension = dimension;
    return std::move(*this);
  }

  RecipeUavDecl &WithStructureStride(uint32_t structureStride) & {
    StructureStride = structureStride;
    return *this;
  }

  RecipeUavDecl &&WithStructureStride(uint32_t structureStride) && {
    StructureStride = structureStride;
    return std::move(*this);
  }

  RecipeUavDecl &WithGloballyCoherent(bool globallyCoherent = true) & {
    GloballyCoherent = globallyCoherent;
    return *this;
  }

  RecipeUavDecl &&WithGloballyCoherent(bool globallyCoherent = true) && {
    GloballyCoherent = globallyCoherent;
    return std::move(*this);
  }

  RecipeUavDecl &WithOrderPreservingCounter(bool hasCounter = true) & {
    HasOrderPreservingCounter = hasCounter;
    return *this;
  }

  RecipeUavDecl &&WithOrderPreservingCounter(bool hasCounter = true) && {
    HasOrderPreservingCounter = hasCounter;
    return std::move(*this);
  }

  RecipeUavDecl &WithHandle(std::string handle) & {
    Handle = std::move(handle);
    return *this;
  }

  RecipeUavDecl &&WithHandle(std::string handle) && {
    Handle = std::move(handle);
    return std::move(*this);
  }

  RecipeUavDecl &AutoBindToNext(bool autoBind = true) & {
    AutoBind = autoBind;
    return *this;
  }

  RecipeUavDecl &&AutoBindToNext(bool autoBind = true) && {
    AutoBind = autoBind;
    return std::move(*this);
  }
};

/// @brief Describes one operand in a declarative recipe pattern or template.
struct RecipeOperandPattern {
  std::string Type;
  std::vector<uint32_t> Indices;
  std::string BindHandle;
  std::string StateTemp;
  std::string Mask;
  std::string Swizzle;
  std::string Select;
  int32_t NumComponents = -1;
  std::string Modifier;
  std::vector<uint32_t> ImmediateU32;
  std::vector<float> ImmediateF32;
  std::string Capture;
  std::string MatchCapture;
  std::string Scratch;

  RecipeOperandPattern &WithType(std::string type) & {
    Type = std::move(type);
    return *this;
  }

  RecipeOperandPattern &&WithType(std::string type) && {
    Type = std::move(type);
    return std::move(*this);
  }

  RecipeOperandPattern &WithIndices(std::vector<uint32_t> indices) & {
    Indices = std::move(indices);
    return *this;
  }

  RecipeOperandPattern &&WithIndices(std::vector<uint32_t> indices) && {
    Indices = std::move(indices);
    return std::move(*this);
  }

  RecipeOperandPattern &WithBindHandle(std::string bindHandle) & {
    BindHandle = std::move(bindHandle);
    return *this;
  }

  RecipeOperandPattern &&WithBindHandle(std::string bindHandle) && {
    BindHandle = std::move(bindHandle);
    return std::move(*this);
  }

  RecipeOperandPattern &WithStateTemp(std::string stateTemp) & {
    StateTemp = std::move(stateTemp);
    return *this;
  }

  RecipeOperandPattern &&WithStateTemp(std::string stateTemp) && {
    StateTemp = std::move(stateTemp);
    return std::move(*this);
  }

  RecipeOperandPattern &WithMask(std::string mask) & {
    Mask = std::move(mask);
    return *this;
  }

  RecipeOperandPattern &&WithMask(std::string mask) && {
    Mask = std::move(mask);
    return std::move(*this);
  }

  RecipeOperandPattern &WithSwizzle(std::string swizzle) & {
    Swizzle = std::move(swizzle);
    return *this;
  }

  RecipeOperandPattern &&WithSwizzle(std::string swizzle) && {
    Swizzle = std::move(swizzle);
    return std::move(*this);
  }

  RecipeOperandPattern &WithSelect(std::string select) & {
    Select = std::move(select);
    return *this;
  }

  RecipeOperandPattern &&WithSelect(std::string select) && {
    Select = std::move(select);
    return std::move(*this);
  }

  RecipeOperandPattern &WithNumComponents(int32_t numComponents) & {
    NumComponents = numComponents;
    return *this;
  }

  RecipeOperandPattern &&WithNumComponents(int32_t numComponents) && {
    NumComponents = numComponents;
    return std::move(*this);
  }

  RecipeOperandPattern &WithModifier(std::string modifier) & {
    Modifier = std::move(modifier);
    return *this;
  }

  RecipeOperandPattern &&WithModifier(std::string modifier) && {
    Modifier = std::move(modifier);
    return std::move(*this);
  }

  RecipeOperandPattern &WithImmediateU32(std::vector<uint32_t> immediateU32) & {
    ImmediateU32 = std::move(immediateU32);
    return *this;
  }

  RecipeOperandPattern &&WithImmediateU32(std::vector<uint32_t> immediateU32) && {
    ImmediateU32 = std::move(immediateU32);
    return std::move(*this);
  }

  RecipeOperandPattern &WithImmediateF32(std::vector<float> immediateF32) & {
    ImmediateF32 = std::move(immediateF32);
    return *this;
  }

  RecipeOperandPattern &&WithImmediateF32(std::vector<float> immediateF32) && {
    ImmediateF32 = std::move(immediateF32);
    return std::move(*this);
  }

  RecipeOperandPattern &CaptureAs(std::string capture) & {
    Capture = std::move(capture);
    return *this;
  }

  RecipeOperandPattern &&CaptureAs(std::string capture) && {
    Capture = std::move(capture);
    return std::move(*this);
  }

  RecipeOperandPattern &WithMatchCapture(std::string matchCapture) & {
    MatchCapture = std::move(matchCapture);
    return *this;
  }

  RecipeOperandPattern &&WithMatchCapture(std::string matchCapture) && {
    MatchCapture = std::move(matchCapture);
    return std::move(*this);
  }

  RecipeOperandPattern &WithScratch(std::string scratch) & {
    Scratch = std::move(scratch);
    return *this;
  }

  RecipeOperandPattern &&WithScratch(std::string scratch) && {
    Scratch = std::move(scratch);
    return std::move(*this);
  }
};

/// @brief Describes one instruction pattern for rule matching.
struct RecipeInstructionPattern {
  std::string Opcode;
  std::string Capture;
  std::string Saturate;
  std::string InterpolationMode;
  int32_t TestBoolean = -1;
  std::vector<RecipeOperandPattern> Operands;

  RecipeInstructionPattern &WithOpcode(std::string opcode) & {
    Opcode = std::move(opcode);
    return *this;
  }

  RecipeInstructionPattern &&WithOpcode(std::string opcode) && {
    Opcode = std::move(opcode);
    return std::move(*this);
  }

  RecipeInstructionPattern &CaptureAs(std::string capture) & {
    Capture = std::move(capture);
    return *this;
  }

  RecipeInstructionPattern &&CaptureAs(std::string capture) && {
    Capture = std::move(capture);
    return std::move(*this);
  }

  RecipeInstructionPattern &WithSaturate(std::string saturate) & {
    Saturate = std::move(saturate);
    return *this;
  }

  RecipeInstructionPattern &&WithSaturate(std::string saturate) && {
    Saturate = std::move(saturate);
    return std::move(*this);
  }

  RecipeInstructionPattern &WithInterpolationMode(std::string interpolationMode) & {
    InterpolationMode = std::move(interpolationMode);
    return *this;
  }

  RecipeInstructionPattern &&WithInterpolationMode(std::string interpolationMode) && {
    InterpolationMode = std::move(interpolationMode);
    return std::move(*this);
  }

  RecipeInstructionPattern &WithTestBoolean(int32_t testBoolean) & {
    TestBoolean = testBoolean;
    return *this;
  }

  RecipeInstructionPattern &&WithTestBoolean(int32_t testBoolean) && {
    TestBoolean = testBoolean;
    return std::move(*this);
  }

  RecipeInstructionPattern &AddOperand(RecipeOperandPattern operand) & {
    Operands.push_back(std::move(operand));
    return *this;
  }

  RecipeInstructionPattern &&AddOperand(RecipeOperandPattern operand) && {
    Operands.push_back(std::move(operand));
    return std::move(*this);
  }
};

/// @brief Describes one instruction emitted by a rewrite rule.
struct RecipeInstructionTemplate {
  std::string Opcode;
  std::string Saturate;
  std::string InterpolationMode;
  int32_t TestBoolean = -1;
  std::vector<RecipeOperandPattern> Operands;

  RecipeInstructionTemplate &WithOpcode(std::string opcode) & {
    Opcode = std::move(opcode);
    return *this;
  }

  RecipeInstructionTemplate &&WithOpcode(std::string opcode) && {
    Opcode = std::move(opcode);
    return std::move(*this);
  }

  RecipeInstructionTemplate &WithSaturate(std::string saturate) & {
    Saturate = std::move(saturate);
    return *this;
  }

  RecipeInstructionTemplate &&WithSaturate(std::string saturate) && {
    Saturate = std::move(saturate);
    return std::move(*this);
  }

  RecipeInstructionTemplate &WithInterpolationMode(std::string interpolationMode) & {
    InterpolationMode = std::move(interpolationMode);
    return *this;
  }

  RecipeInstructionTemplate &&WithInterpolationMode(std::string interpolationMode) && {
    InterpolationMode = std::move(interpolationMode);
    return std::move(*this);
  }

  RecipeInstructionTemplate &WithTestBoolean(int32_t testBoolean) & {
    TestBoolean = testBoolean;
    return *this;
  }

  RecipeInstructionTemplate &&WithTestBoolean(int32_t testBoolean) && {
    TestBoolean = testBoolean;
    return std::move(*this);
  }

  RecipeInstructionTemplate &AddOperand(RecipeOperandPattern operand) & {
    Operands.push_back(std::move(operand));
    return *this;
  }

  RecipeInstructionTemplate &&AddOperand(RecipeOperandPattern operand) && {
    Operands.push_back(std::move(operand));
    return std::move(*this);
  }
};

/// @brief Describes the top-level match criteria for a recipe rule or
/// prefilter.
struct RecipeMatchPattern {
  std::string Opcode;
  std::string Capture;
  std::string Saturate;
  std::string InterpolationMode;
  int32_t TestBoolean = -1;
  std::vector<RecipeOperandPattern> Operands;
  std::vector<RecipeInstructionPattern> Sequence;

  RecipeMatchPattern &WithOpcode(std::string opcode) & {
    Opcode = std::move(opcode);
    return *this;
  }

  RecipeMatchPattern &&WithOpcode(std::string opcode) && {
    Opcode = std::move(opcode);
    return std::move(*this);
  }

  RecipeMatchPattern &CaptureAs(std::string capture) & {
    Capture = std::move(capture);
    return *this;
  }

  RecipeMatchPattern &&CaptureAs(std::string capture) && {
    Capture = std::move(capture);
    return std::move(*this);
  }

  RecipeMatchPattern &WithSaturate(std::string saturate) & {
    Saturate = std::move(saturate);
    return *this;
  }

  RecipeMatchPattern &&WithSaturate(std::string saturate) && {
    Saturate = std::move(saturate);
    return std::move(*this);
  }

  RecipeMatchPattern &WithInterpolationMode(std::string interpolationMode) & {
    InterpolationMode = std::move(interpolationMode);
    return *this;
  }

  RecipeMatchPattern &&WithInterpolationMode(std::string interpolationMode) && {
    InterpolationMode = std::move(interpolationMode);
    return std::move(*this);
  }

  RecipeMatchPattern &WithTestBoolean(int32_t testBoolean) & {
    TestBoolean = testBoolean;
    return *this;
  }

  RecipeMatchPattern &&WithTestBoolean(int32_t testBoolean) && {
    TestBoolean = testBoolean;
    return std::move(*this);
  }

  RecipeMatchPattern &AddOperand(RecipeOperandPattern operand) & {
    Operands.push_back(std::move(operand));
    return *this;
  }

  RecipeMatchPattern &&AddOperand(RecipeOperandPattern operand) && {
    Operands.push_back(std::move(operand));
    return std::move(*this);
  }

  RecipeMatchPattern &AddInstruction(RecipeInstructionPattern instruction) & {
    Sequence.push_back(std::move(instruction));
    return *this;
  }

  RecipeMatchPattern &&AddInstruction(RecipeInstructionPattern instruction) && {
    Sequence.push_back(std::move(instruction));
    return std::move(*this);
  }
};

/// @brief Describes one SM5 prefilter check used by a prefilter step.
struct RecipePrefilter {
  PrefilterKind Kind = PrefilterKind::CheckShaderVersion;
  std::string Name;
  bool Required = true;
  uint32_t ExpectedMajorVersion = 0;
  uint32_t ExpectedMinorVersion = 0;
  std::string Opcode;
  int32_t ExpectedCount = 0;
  int32_t ExpectedResourceCount = 0;
  RecipeMatchPattern Match;

  RecipePrefilter &Named(std::string name) & {
    Name = std::move(name);
    return *this;
  }

  RecipePrefilter &&Named(std::string name) && {
    Name = std::move(name);
    return std::move(*this);
  }

  RecipePrefilter &Require(bool required) & {
    Required = required;
    return *this;
  }

  RecipePrefilter &&Require(bool required) && {
    Required = required;
    return std::move(*this);
  }

  RecipePrefilter &CheckShaderVersion(uint32_t majorVersion,
                                      uint32_t minorVersion) & {
    Kind = PrefilterKind::CheckShaderVersion;
    ExpectedMajorVersion = majorVersion;
    ExpectedMinorVersion = minorVersion;
    return *this;
  }

  RecipePrefilter &&CheckShaderVersion(uint32_t majorVersion,
                                       uint32_t minorVersion) && {
    Kind = PrefilterKind::CheckShaderVersion;
    ExpectedMajorVersion = majorVersion;
    ExpectedMinorVersion = minorVersion;
    return std::move(*this);
  }

  RecipePrefilter &CheckOpcodeCount(std::string opcode,
                                    int32_t expectedCount) & {
    Kind = PrefilterKind::CheckOpcodeCount;
    Opcode = std::move(opcode);
    ExpectedCount = expectedCount;
    return *this;
  }

  RecipePrefilter &&CheckOpcodeCount(std::string opcode,
                                     int32_t expectedCount) && {
    Kind = PrefilterKind::CheckOpcodeCount;
    Opcode = std::move(opcode);
    ExpectedCount = expectedCount;
    return std::move(*this);
  }

  RecipePrefilter &CheckResourceCount(int32_t expectedResourceCount) & {
    Kind = PrefilterKind::CheckResourceCount;
    ExpectedResourceCount = expectedResourceCount;
    return *this;
  }

  RecipePrefilter &&CheckResourceCount(int32_t expectedResourceCount) && {
    Kind = PrefilterKind::CheckResourceCount;
    ExpectedResourceCount = expectedResourceCount;
    return std::move(*this);
  }

  RecipePrefilter &CheckPatternMatch(RecipeMatchPattern match) & {
    Kind = PrefilterKind::CheckPatternMatch;
    Match = std::move(match);
    return *this;
  }

  RecipePrefilter &&CheckPatternMatch(RecipeMatchPattern match) && {
    Kind = PrefilterKind::CheckPatternMatch;
    Match = std::move(match);
    return std::move(*this);
  }
};

/// @brief Stores one callback-supplied SM5 rule match and its captures.
///
/// Callback matches are normalized into the same runtime rewrite flow used by
/// declarative rules. RangeStartIndex and RangeEndIndex describe the matched
/// instruction window when a callback wants ReplaceRange-style behavior.
struct RecipeRuleMatch {
  uint32_t InstructionIndex = 0;
  const Instruction *InstructionHandle = nullptr;
  uint32_t RangeStartIndex = 0;
  uint32_t RangeEndIndex = 0;
  std::unordered_map<std::string, const Operand *> CapturedOperands;
  std::unordered_map<std::string, const Instruction *> CapturedInstructions;
  std::unordered_map<std::string, uint32_t> CapturedInstructionIndices;

  const Operand *GetCapturedOperand(const std::string &name) const {
    const auto it = CapturedOperands.find(name);
    return it == CapturedOperands.end() ? nullptr : it->second;
  }

  const Instruction *GetCapturedInstruction(const std::string &name) const {
    const auto it = CapturedInstructions.find(name);
    return it == CapturedInstructions.end() ? nullptr : it->second;
  }

  const uint32_t *GetCapturedInstructionIndex(const std::string &name) const {
    const auto it = CapturedInstructionIndices.find(name);
    return it == CapturedInstructionIndices.end() ? nullptr : &it->second;
  }
};

/// @brief Enumerates callback-generated SM5 rewrite operations.
enum class RecipeRewriteActionKind {
  ReplaceOne,
  ReplaceRange,
  InsertBefore,
  InsertAfter,
  RemoveRange,
};

/// @brief Describes one callback-generated SM5 rewrite operation.
///
/// These actions are only produced by code callbacks. YAML recipes continue to
/// use declarative `emit`, `replace`, and `match.rewrite_mode` fields instead.
struct RecipeRewriteAction {
  RecipeRewriteActionKind Kind = RecipeRewriteActionKind::ReplaceOne;
  uint32_t ReplaceIndex = 0;
  uint32_t RangeStart = 0;
  uint32_t RangeEnd = 0;
  uint32_t InsertPosition = 0;
  uint32_t RemoveStart = 0;
  uint32_t RemoveEnd = 0;
  uint32_t RequiredTempCount = 0;
  std::vector<RecipeInstructionTemplate> Emit;

  RecipeRewriteAction &AddEmit(RecipeInstructionTemplate instruction) & {
    Emit.push_back(std::move(instruction));
    return *this;
  }

  RecipeRewriteAction &&AddEmit(RecipeInstructionTemplate instruction) && {
    Emit.push_back(std::move(instruction));
    return std::move(*this);
  }
};

/// @brief Produces explicit SM5 matches for a rule from the current program.
///
/// Use this overload when declarative `RecipeMatchPattern` is not expressive
/// enough. Callback matching is mutually exclusive with declarative `Match`.
using RecipeMatchCallback = std::function<std::vector<RecipeRuleMatch>(
    const Program &, RecipeContext &)>;

/// @brief Filters a rule using mutable recipe context state.
using RecipeRulePredicate = std::function<bool(RecipeContext &)>;

/// @brief Produces rewrite actions for one callback-supplied match.
///
/// Callback rewriting is mutually exclusive with declarative `Emit`, `Replace`,
/// and `RewriteMode` fields on the same rule.
using RecipeRewriteCallback = std::function<std::vector<RecipeRewriteAction>(
    const Program &, const RecipeRuleMatch &, RecipeContext &)>;

/// @brief Describes one SM5 rewrite rule.
///
/// A rule may be fully declarative through `Match`, `Emit`, `Replace`, and
/// `RewriteMode`, or it may use callback overloads for matching and/or
/// rewriting. Callback and declarative forms are compiled through the same
/// runtime path, but they must not be mixed for the same stage.
struct RecipeRule {
  RecipeMatchPattern Match;
  RecipeMatchCallback MatchCallback;
  std::vector<RecipeInstructionTemplate> Emit;
  std::string Replace;
  RecipeRuleApplicationMode ApplicationMode = RecipeRuleApplicationMode::First;
  RecipeRuleRewriteMode RewriteMode = RecipeRuleRewriteMode::Replace;
  RecipeRulePredicate Predicate;
  RecipeRewriteCallback RewriteCallback;

  /// @brief Uses declarative pattern matching for this rule.
  RecipeRule &WithMatch(RecipeMatchPattern match) & {
    Match = std::move(match);
    MatchCallback = {};
    return *this;
  }

  /// @brief Uses declarative pattern matching for this rule.
  RecipeRule &&WithMatch(RecipeMatchPattern match) && {
    Match = std::move(match);
    MatchCallback = {};
    return std::move(*this);
  }

  /// @brief Uses callback-driven matching for this rule.
  RecipeRule &WithMatch(RecipeMatchCallback callback) & {
    Match = RecipeMatchPattern{};
    MatchCallback = std::move(callback);
    return *this;
  }

  /// @brief Uses callback-driven matching for this rule.
  RecipeRule &&WithMatch(RecipeMatchCallback callback) && {
    Match = RecipeMatchPattern{};
    MatchCallback = std::move(callback);
    return std::move(*this);
  }

  /// @brief Appends declarative emit output and clears callback rewriting.
  RecipeRule &AddEmit(RecipeInstructionTemplate instruction) & {
    RewriteCallback = {};
    Emit.push_back(std::move(instruction));
    return *this;
  }

  /// @brief Appends declarative emit output and clears callback rewriting.
  RecipeRule &&AddEmit(RecipeInstructionTemplate instruction) && {
    RewriteCallback = {};
    Emit.push_back(std::move(instruction));
    return std::move(*this);
  }

  /// @brief Selects the captured instruction replaced by declarative rewriting.
  RecipeRule &ReplaceCapture(std::string capture) & {
    RewriteCallback = {};
    Replace = std::move(capture);
    return *this;
  }

  /// @brief Selects the captured instruction replaced by declarative rewriting.
  RecipeRule &&ReplaceCapture(std::string capture) && {
    RewriteCallback = {};
    Replace = std::move(capture);
    return std::move(*this);
  }

  RecipeRule &ApplyMode(RecipeRuleApplicationMode applicationMode) & {
    ApplicationMode = applicationMode;
    return *this;
  }

  RecipeRule &&ApplyMode(RecipeRuleApplicationMode applicationMode) && {
    ApplicationMode = applicationMode;
    return std::move(*this);
  }

  /// @brief Selects the declarative rewrite mode and clears callback rewriting.
  RecipeRule &RewriteAs(RecipeRuleRewriteMode rewriteMode) & {
    RewriteCallback = {};
    RewriteMode = rewriteMode;
    return *this;
  }

  /// @brief Selects the declarative rewrite mode and clears callback rewriting.
  RecipeRule &&RewriteAs(RecipeRuleRewriteMode rewriteMode) && {
    RewriteCallback = {};
    RewriteMode = rewriteMode;
    return std::move(*this);
  }

  /// @brief Uses callback-driven rewriting and clears declarative rewrite data.
  RecipeRule &Rewrite(RecipeRewriteCallback callback) & {
    Emit.clear();
    Replace.clear();
    RewriteMode = RecipeRuleRewriteMode::Replace;
    RewriteCallback = std::move(callback);
    return *this;
  }

  /// @brief Uses callback-driven rewriting and clears declarative rewrite data.
  RecipeRule &&Rewrite(RecipeRewriteCallback callback) && {
    Emit.clear();
    Replace.clear();
    RewriteMode = RecipeRuleRewriteMode::Replace;
    RewriteCallback = std::move(callback);
    return std::move(*this);
  }

  RecipeRule &When(RecipeRulePredicate predicate) & {
    Predicate = std::move(predicate);
    return *this;
  }

  RecipeRule &&When(RecipeRulePredicate predicate) && {
    Predicate = std::move(predicate);
    return std::move(*this);
  }
};

/// @brief Reports the result of executing one recipe step.
struct RecipeStepResult {
  bool Success = true;
  bool Changed = false;
  uint32_t MatchCount = 0;
  bool StopRecipe = false;
  bool ResourceBindingsChanged = false;
  bool ResourcesRefreshed = false;
  bool ModuleVerified = false;
  std::string Error;
  std::vector<dxp::PatchRuleReport> RuleReports;
  std::vector<dxp::PatchSideEffect> SideEffects;
};

/// @brief Describes a generic step guard based on recipe context state.
struct RecipeStepCondition {
  std::string State;
  std::vector<RecipeStepCondition> All;
  std::vector<RecipeStepCondition> Any;
  bool Negate = false;

  static RecipeStepCondition FromState(std::string state,
                                       bool negate = false) {
    RecipeStepCondition condition;
    condition.State = std::move(state);
    condition.Negate = negate;
    return condition;
  }

  static RecipeStepCondition AllOf(std::vector<RecipeStepCondition> conditions,
                                   bool negate = false) {
    RecipeStepCondition condition;
    condition.All = std::move(conditions);
    condition.Negate = negate;
    return condition;
  }

  static RecipeStepCondition AnyOf(std::vector<RecipeStepCondition> conditions,
                                   bool negate = false) {
    RecipeStepCondition condition;
    condition.Any = std::move(conditions);
    condition.Negate = negate;
    return condition;
  }

  bool IsSet() const {
    return !State.empty() || !All.empty() || !Any.empty();
  }
};

/// @brief Callable signature for custom recipe steps.
using RecipeStepExecutor = std::function<RecipeStepResult(RecipeContext &)>;
using RecipeStepPredicate = std::function<bool(RecipeContext &)>;

/// @brief Represents one executable step in a recipe.
struct RecipeStep {
  std::string Name;
  std::vector<RecipeRule> Rules;
  RecipeRuleApplicationMode ApplicationMode = RecipeRuleApplicationMode::First;
  bool Required = true;
  RecipeStepCondition If;
  RecipeStepExecutor Execute;
  RecipeStepPredicate Predicate;

  RecipeStep &Require(bool required) & {
    Required = required;
    return *this;
  }

  RecipeStep &&Require(bool required) && {
    Required = required;
    return std::move(*this);
  }

  // Overload for declarative condition
  RecipeStep &When(RecipeStepCondition condition) & {
    If = std::move(condition);
    return *this;
  }

  RecipeStep &&When(RecipeStepCondition condition) && {
    If = std::move(condition);
    return std::move(*this);
  }

  // Overload for programmatic predicate
  RecipeStep &When(RecipeStepPredicate predicate) & {
    Predicate = std::move(predicate);
    return *this;
  }

  RecipeStep &&When(RecipeStepPredicate predicate) && {
    Predicate = std::move(predicate);
    return std::move(*this);
  }

  // RunIf removed; use When for both declarative and programmatic gating.

  bool IsCustom() const { return static_cast<bool>(Execute); }
};

/// @brief Creates a successful step result.
/// @param changed Whether the step changed program state.
/// @param matchCount Number of matches processed by the step.
/// @param stopRecipe Whether recipe execution should stop after this step.
/// @return Initialized step result.
RecipeStepResult MakeRecipeStepSuccess(bool changed = false,
                                       uint32_t matchCount = 0,
                                       bool stopRecipe = false);

/// @brief Creates a failed step result and records the message in context.
/// @param context Recipe execution context to update.
/// @param message Error message to store.
/// @return Initialized failed step result.
RecipeStepResult MakeRecipeStepFailure(RecipeContext &context,
                                       std::string message);

/// @brief Wraps a custom executor as a named recipe step.
RecipeStep MakeCustomRecipeStep(std::string name, RecipeStepExecutor execute);

/// @brief Creates a step that applies declarative rewrite rules.
RecipeStep MakeRewriteRulesStep(
    std::string name, std::vector<RecipeRule> rules,
    RecipeRuleApplicationMode mode = RecipeRuleApplicationMode::First,
  bool required = true);

/// @brief Creates a step that evaluates one or more SM5 prefilter checks.
RecipeStep MakePrefilterStep(
  std::string name, std::vector<RecipePrefilter> checks,
  std::string setState = {},
  RecipePrefilterMode mode = RecipePrefilterMode::All);

/// @brief Creates a step that adds an input declaration.
RecipeStep MakeAddInputStep(std::string id, RecipeInputDecl decl);

/// @brief Creates a step that adds an output declaration.
RecipeStep MakeAddOutputStep(std::string id, RecipeOutputDecl decl);

/// @brief Creates a step that adds a texture declaration.
RecipeStep MakeAddTextureStep(std::string id, RecipeTextureDecl decl);

/// @brief Creates a step that adds a raw resource declaration.
RecipeStep MakeAddRawResourceStep(std::string id, RecipeRawResourceDecl decl);

/// @brief Creates a step that adds a structured resource declaration.
RecipeStep MakeAddStructuredResourceStep(std::string id,
                                         RecipeStructuredResourceDecl decl);

/// @brief Creates a step that adds a constant buffer declaration.
RecipeStep MakeAddCBufferStep(std::string id, RecipeCBufferDecl decl);

/// @brief Creates a step that adds a sampler declaration.
RecipeStep MakeAddSamplerStep(std::string id, RecipeSamplerDecl decl);

/// @brief Creates a step that adds a UAV declaration.
RecipeStep MakeAddUavStep(std::string id, RecipeUavDecl decl);

/// @brief Creates a step that refreshes derived resource metadata.
RecipeStep MakeRefreshResourcesStep(std::string name = "refresh_resources");

/// @brief Creates a step that verifies the decoded program state.
RecipeStep MakeVerifyProgramStep(std::string name = "verify_program");

/// @brief Creates a shader-version prefilter.
RecipePrefilter MakeShaderVersionPrefilter(uint32_t majorVersion,
                                           uint32_t minorVersion,
                                           std::string name = {},
                                           bool required = true);

/// @brief Creates an opcode-count prefilter.
RecipePrefilter MakeOpcodeCountPrefilter(std::string opcode,
                                         int32_t expectedCount,
                                         std::string name = {},
                                         bool required = true);

/// @brief Creates a resource-count prefilter.
RecipePrefilter MakeResourceCountPrefilter(int32_t expectedResourceCount,
                                           std::string name = {},
                                           bool required = true);

/// @brief Creates a pattern-match prefilter.
RecipePrefilter MakePatternPrefilter(RecipeMatchPattern match,
                                     std::string name = {},
                                     bool required = true);

/// @brief Reserves a contiguous range of temporary registers.
/// @param context Recipe execution context to update.
/// @param count Number of temporary registers to reserve.
/// @param baseIndex Receives the first reserved register index.
/// @return `true` on success.
bool ReserveTempRegisters(RecipeContext &context, uint32_t count,
                          uint32_t &baseIndex);

/// @brief Owns the declarative SM5 recipe definition.
class Recipe {
public:
  Recipe &ReserveTemps(uint32_t count) {
    reservedTempRegisters_ = count;
    return *this;
  }

  Recipe &AddStep(RecipeStep step) {
    steps_.push_back(std::move(step));
    return *this;
  }

  Recipe &AddTempDecl(RecipeTempDecl decl) {
    tempDecls_.push_back(std::move(decl));
    return *this;
  }

  const std::vector<RecipeStep> &GetSteps() const { return steps_; }

  const std::vector<RecipeTempDecl> &GetTempDecls() const { return tempDecls_; }

  uint32_t GetReservedTempRegisters() const { return reservedTempRegisters_; }

private:
  uint32_t reservedTempRegisters_ = 0;
  std::vector<RecipeStep> steps_;
  std::vector<RecipeTempDecl> tempDecls_;
};

} // namespace dxp::sm5
