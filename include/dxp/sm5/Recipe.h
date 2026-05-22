#pragma once

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
struct MatchResult;

struct RecipeContext {
  Program *ProgramHandle = nullptr;
  bool TraceEnabled = false;
  uint32_t TotalRuleMatches = 0;
  bool ProgramModified = false;
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

  template <typename TValue>
  TValue *FindInput(const std::string &name) {
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

  template <typename TValue>
  TValue *FindState(const std::string &name) {
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

enum class RecipeRuleApplicationMode {
  First,
  Last,
  MatchAll,
};

enum class PrefilterKind {
  CheckShaderVersion,
  CheckOpcodeCount,
  CheckResourceCount,
  CheckPatternMatch,
};

struct RecipeTextureDecl {
  uint32_t BindPoint = 0;
  uint32_t Dimension = D3D10_SB_RESOURCE_DIMENSION_TEXTURE2D;
  std::string Handle;
  bool AutoBind = false;
};

struct RecipeTempDecl {
  std::string Handle;
};

struct RecipeInputDecl {
  uint32_t BindPoint = 0;
  uint32_t InterpolationMode = D3D10_SB_INTERPOLATION_LINEAR;
  std::string Handle;
  bool AutoBind = false;
};

struct RecipeOutputDecl {
  uint32_t BindPoint = 0;
  std::string Handle;
  bool AutoBind = false;
};

struct RecipeCBufferDecl {
  uint32_t BindPoint = 0;
  uint32_t Elements = 1;
  uint32_t AccessPattern = D3D10_SB_CONSTANT_BUFFER_IMMEDIATE_INDEXED;
  std::string Handle;
  bool AutoBind = false;
};

struct RecipeSamplerDecl {
  uint32_t BindPoint = 0;
  uint32_t Mode = D3D10_SB_SAMPLER_MODE_DEFAULT;
  std::string Handle;
  bool AutoBind = false;
};

struct RecipeRawResourceDecl {
  uint32_t BindPoint = 0;
  std::string Handle;
  bool AutoBind = false;
};

struct RecipeStructuredResourceDecl {
  uint32_t BindPoint = 0;
  uint32_t StructureStride = 16;
  std::string Handle;
  bool AutoBind = false;
};

enum class RecipeUavKind {
  Typed,
  Raw,
  Structured,
};

struct RecipeUavDecl {
  uint32_t BindPoint = 0;
  RecipeUavKind Kind = RecipeUavKind::Typed;
  uint32_t Dimension = D3D10_SB_RESOURCE_DIMENSION_TEXTURE2D;
  uint32_t StructureStride = 16;
  bool GloballyCoherent = false;
  bool HasOrderPreservingCounter = false;
  std::string Handle;
  bool AutoBind = false;
};

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
  std::string FromCapture;
  std::string Scratch;
};

struct RecipeInstructionPattern {
  std::string Opcode;
  std::string Capture;
  std::string Saturate;
  std::string InterpolationMode;
  int32_t TestBoolean = -1;
  std::vector<RecipeOperandPattern> Operands;
};

struct RecipeInstructionTemplate {
  std::string Opcode;
  std::string Saturate;
  std::string InterpolationMode;
  int32_t TestBoolean = -1;
  std::vector<RecipeOperandPattern> Operands;
};

struct RecipeMatchPattern {
  std::string Opcode;
  std::string Capture;
  std::string Saturate;
  std::string InterpolationMode;
  int32_t TestBoolean = -1;
  std::vector<RecipeOperandPattern> Operands;
  std::vector<RecipeInstructionPattern> Sequence;
};

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
};

struct RecipeRule {
  RecipeMatchPattern Match;
  std::vector<RecipeInstructionTemplate> Emit;
  std::string Replace;
  RecipeRuleApplicationMode ApplicationMode = RecipeRuleApplicationMode::First;
  std::function<bool(RecipeContext &, const MatchResult &)> Predicate;
};

struct RecipeStepResult {
  bool Success = true;
  bool Changed = false;
  uint32_t MatchCount = 0;
  bool StopRecipe = false;
  std::string Error;
};

using RecipeStepExecutor = std::function<RecipeStepResult(RecipeContext &)>;

struct RecipeStep {
  std::string Name;
  std::vector<RecipeRule> Rules;
  RecipeRuleApplicationMode ApplicationMode = RecipeRuleApplicationMode::First;
  bool Required = true;
  RecipeStepExecutor Execute;

  bool IsCustom() const {
    return static_cast<bool>(Execute);
  }
};

RecipeStepResult MakeRecipeStepSuccess(bool changed = false,
                                       uint32_t matchCount = 0,
                                       bool stopRecipe = false);
RecipeStepResult MakeRecipeStepFailure(RecipeContext &context,
                                       std::string message);

RecipeStep MakeCustomRecipeStep(std::string name,
                                RecipeStepExecutor execute);
RecipeStep MakeRewriteRulesStep(
    std::string name, std::vector<RecipeRule> rules,
    RecipeRuleApplicationMode mode = RecipeRuleApplicationMode::First,
    bool required = true);

RecipePrefilter MakeShaderVersionPrefilter(uint32_t majorVersion,
                                           uint32_t minorVersion,
                                           std::string name = {},
                                           bool required = true);
RecipePrefilter MakeOpcodeCountPrefilter(std::string opcode,
                                         int32_t expectedCount,
                                         std::string name = {},
                                         bool required = true);
RecipePrefilter MakeResourceCountPrefilter(int32_t expectedResourceCount,
                                           std::string name = {},
                                           bool required = true);
RecipePrefilter MakePatternPrefilter(RecipeMatchPattern match,
                                     std::string name = {},
                                     bool required = true);

bool ReserveTempRegisters(RecipeContext &context,
                          uint32_t count,
                          uint32_t &baseIndex);

class Recipe {
public:
  Recipe &ReserveTemps(uint32_t count) {
    reservedTempRegisters_ = count;
    return *this;
  }

  Recipe &AddPrefilter(RecipePrefilter prefilter) {
    prefilters_.push_back(std::move(prefilter));
    return *this;
  }

  Recipe &AddStep(RecipeStep step) {
    steps_.push_back(std::move(step));
    return *this;
  }

  Recipe &AddTextureDecl(RecipeTextureDecl decl) {
    textureDecls_.push_back(std::move(decl));
    return *this;
  }

  Recipe &AddTempDecl(RecipeTempDecl decl) {
    tempDecls_.push_back(std::move(decl));
    return *this;
  }

  Recipe &AddInputDecl(RecipeInputDecl decl) {
    inputDecls_.push_back(std::move(decl));
    return *this;
  }

  Recipe &AddOutputDecl(RecipeOutputDecl decl) {
    outputDecls_.push_back(std::move(decl));
    return *this;
  }

  Recipe &AddCBufferDecl(RecipeCBufferDecl decl) {
    cbufferDecls_.push_back(std::move(decl));
    return *this;
  }

  Recipe &AddSamplerDecl(RecipeSamplerDecl decl) {
    samplerDecls_.push_back(std::move(decl));
    return *this;
  }

  Recipe &AddRawResourceDecl(RecipeRawResourceDecl decl) {
    rawResourceDecls_.push_back(std::move(decl));
    return *this;
  }

  Recipe &AddStructuredResourceDecl(RecipeStructuredResourceDecl decl) {
    structuredResourceDecls_.push_back(std::move(decl));
    return *this;
  }

  Recipe &AddUavDecl(RecipeUavDecl decl) {
    uavDecls_.push_back(std::move(decl));
    return *this;
  }

  const std::vector<RecipePrefilter> &GetPrefilters() const {
    return prefilters_;
  }

  const std::vector<RecipeStep> &GetSteps() const {
    return steps_;
  }

  const std::vector<RecipeTextureDecl> &GetTextureDecls() const {
    return textureDecls_;
  }

  const std::vector<RecipeTempDecl> &GetTempDecls() const {
    return tempDecls_;
  }

  const std::vector<RecipeInputDecl> &GetInputDecls() const {
    return inputDecls_;
  }

  const std::vector<RecipeOutputDecl> &GetOutputDecls() const {
    return outputDecls_;
  }

  const std::vector<RecipeCBufferDecl> &GetCBufferDecls() const {
    return cbufferDecls_;
  }

  const std::vector<RecipeSamplerDecl> &GetSamplerDecls() const {
    return samplerDecls_;
  }

  const std::vector<RecipeRawResourceDecl> &GetRawResourceDecls() const {
    return rawResourceDecls_;
  }

  const std::vector<RecipeStructuredResourceDecl> &GetStructuredResourceDecls() const {
    return structuredResourceDecls_;
  }

  const std::vector<RecipeUavDecl> &GetUavDecls() const {
    return uavDecls_;
  }

  uint32_t GetReservedTempRegisters() const {
    return reservedTempRegisters_;
  }

private:
  uint32_t reservedTempRegisters_ = 0;
  std::vector<RecipePrefilter> prefilters_;
  std::vector<RecipeStep> steps_;
  std::vector<RecipeTempDecl> tempDecls_;
  std::vector<RecipeInputDecl> inputDecls_;
  std::vector<RecipeOutputDecl> outputDecls_;
  std::vector<RecipeTextureDecl> textureDecls_;
  std::vector<RecipeRawResourceDecl> rawResourceDecls_;
  std::vector<RecipeStructuredResourceDecl> structuredResourceDecls_;
  std::vector<RecipeCBufferDecl> cbufferDecls_;
  std::vector<RecipeSamplerDecl> samplerDecls_;
  std::vector<RecipeUavDecl> uavDecls_;
};

bool ExecuteRecipe(Program &program,
                   const Recipe &recipe,
                   RecipeContext &context);

} // namespace dxp::sm5
