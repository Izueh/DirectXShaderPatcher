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

/// @brief Declares a texture binding to add or reference in a recipe.
struct RecipeTextureDecl {
  uint32_t BindPoint = 0;
  uint32_t Dimension = D3D10_SB_RESOURCE_DIMENSION_TEXTURE2D;
  std::string Handle;
  bool AutoBind = false;
};

/// @brief Declares a temporary register handle used by a recipe.
struct RecipeTempDecl {
  std::string Handle;
};

/// @brief Declares an input signature binding to add or reference.
struct RecipeInputDecl {
  uint32_t BindPoint = 0;
  uint32_t InterpolationMode = D3D10_SB_INTERPOLATION_LINEAR;
  std::string Handle;
  bool AutoBind = false;
};

/// @brief Declares an output signature binding to add or reference.
struct RecipeOutputDecl {
  uint32_t BindPoint = 0;
  std::string Handle;
  bool AutoBind = false;
};

/// @brief Declares a constant buffer binding to add or reference.
struct RecipeCBufferDecl {
  uint32_t BindPoint = 0;
  uint32_t Elements = 1;
  uint32_t AccessPattern = D3D10_SB_CONSTANT_BUFFER_IMMEDIATE_INDEXED;
  std::string Handle;
  bool AutoBind = false;
};

/// @brief Declares a sampler binding to add or reference.
struct RecipeSamplerDecl {
  uint32_t BindPoint = 0;
  uint32_t Mode = D3D10_SB_SAMPLER_MODE_DEFAULT;
  std::string Handle;
  bool AutoBind = false;
};

/// @brief Declares a raw resource binding to add or reference.
struct RecipeRawResourceDecl {
  uint32_t BindPoint = 0;
  std::string Handle;
  bool AutoBind = false;
};

/// @brief Declares a structured resource binding to add or reference.
struct RecipeStructuredResourceDecl {
  uint32_t BindPoint = 0;
  uint32_t StructureStride = 16;
  std::string Handle;
  bool AutoBind = false;
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
};

/// @brief Describes one instruction pattern for rule matching.
struct RecipeInstructionPattern {
  std::string Opcode;
  std::string Capture;
  std::string Saturate;
  std::string InterpolationMode;
  int32_t TestBoolean = -1;
  std::vector<RecipeOperandPattern> Operands;
};

/// @brief Describes one instruction emitted by a rewrite rule.
struct RecipeInstructionTemplate {
  std::string Opcode;
  std::string Saturate;
  std::string InterpolationMode;
  int32_t TestBoolean = -1;
  std::vector<RecipeOperandPattern> Operands;
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
};

/// @brief Describes a precondition that must be checked before running steps.
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

/// @brief Describes one declarative rewrite rule.
struct RecipeRule {
  RecipeMatchPattern Match;
  std::vector<RecipeInstructionTemplate> Emit;
  std::string Replace;
  RecipeRuleApplicationMode ApplicationMode = RecipeRuleApplicationMode::First;
  RecipeRuleRewriteMode RewriteMode = RecipeRuleRewriteMode::Replace;
  std::function<bool(RecipeContext &)> Predicate;
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
};

/// @brief Callable signature for custom recipe steps.
using RecipeStepExecutor = std::function<RecipeStepResult(RecipeContext &)>;

/// @brief Represents one executable step in a recipe.
struct RecipeStep {
  std::string Name;
  std::vector<RecipeRule> Rules;
  RecipeRuleApplicationMode ApplicationMode = RecipeRuleApplicationMode::First;
  bool Required = true;
  RecipeStepExecutor Execute;

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

  Recipe &AddPrefilter(RecipePrefilter prefilter) {
    prefilters_.push_back(std::move(prefilter));
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

  const std::vector<RecipePrefilter> &GetPrefilters() const {
    return prefilters_;
  }

  const std::vector<RecipeStep> &GetSteps() const { return steps_; }

  const std::vector<RecipeTempDecl> &GetTempDecls() const { return tempDecls_; }

  uint32_t GetReservedTempRegisters() const { return reservedTempRegisters_; }

private:
  uint32_t reservedTempRegisters_ = 0;
  std::vector<RecipePrefilter> prefilters_;
  std::vector<RecipeStep> steps_;
  std::vector<RecipeTempDecl> tempDecls_;
};

} // namespace dxp::sm5
