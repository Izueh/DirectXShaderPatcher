#pragma once

#include "dxp/PatchReport.h"
#include "Types.h"

#include <any>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace dxp::sm5 {

struct Program;

/// @brief Global capture store — reused across all matching, persists across steps.
///
/// Captured operands, instructions, and index immediates are stored as copies
/// (not pointers) so they survive rewrites. All capture names must be unique
/// across the entire recipe.
struct CaptureStore {
  /// Captured operands, keyed by their `capture` name.
  std::unordered_map<std::string, CapturedOperand> operands;
  /// Captured instructions (sequence matches), keyed by name.
  std::unordered_map<std::string, CapturedInstruction> instructions;
  /// Captured index immediates and instruction indices, keyed by name.
  std::unordered_map<std::string, uint32_t> indexValues;

  /// Clears all capture data. Called at the start of each recipe execution.
  void clear() {
    operands.clear();
    instructions.clear();
    indexValues.clear();
  }
};

/// @brief Carries mutable state across SM5 recipe execution.
///
/// Variables are set via `SetVariable()` and persist across steps.
/// State is set internally by rule execution and check steps; use
/// `FindState()` to read. External callers must not modify State directly.
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
  std::unordered_map<std::string, std::any> Variables;
  std::unordered_map<std::string, std::any> InitialVariables;  // internal reset baseline
  bool HasInitialVariablesSnapshot = false;
  std::unordered_map<std::string, std::any> State;

  /// Global capture store — reused across all matching, persists across steps.
  /// Cleared at the start of each recipe execution.
  CaptureStore captures;

  void AddDiagnostic(std::string message) {
    Diagnostics.push_back(std::move(message));
  }

  template <typename TValue>
  void SetVariable(const std::string &name, TValue value) {
    Variables[name] = std::any(std::move(value));
  }

  bool UnsetVariable(const std::string &name) {
    return Variables.erase(name) != 0;
  }

  bool HasVariable(const std::string &name) const {
    return Variables.find(name) != Variables.end();
  }

  template <typename TValue> TValue *FindVariable(const std::string &name) {
    auto it = Variables.find(name);
    if (it != Variables.end()) {
      return std::any_cast<TValue>(&it->second);
    }
    return nullptr;
  }

  template <typename TValue>
  const TValue *FindVariable(const std::string &name) const {
    auto it = Variables.find(name);
    if (it != Variables.end()) {
      return std::any_cast<TValue>(&it->second);
    }
    return nullptr;
  }

  const std::any *FindVariableAny(const std::string &name) const {
    auto it = Variables.find(name);
    if (it != Variables.end()) {
      return &it->second;
    }
    return nullptr;
  }

  void SnapshotInitialVariables() {
    if (!HasInitialVariablesSnapshot) {
      InitialVariables = Variables;
      HasInitialVariablesSnapshot = true;
    }
  }

  void ResetVariables() {
    if (!HasInitialVariablesSnapshot) {
      SnapshotInitialVariables();
    }
    Variables = InitialVariables;
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

/// @brief Declares a texture binding to add or reference in a recipe.
struct RecipeTextureDecl {
  uint32_t BindPoint = 0;
  uint32_t Dimension = 3u;
  std::string Handle;
  bool AutoBind = false;

  /// @brief Sets the register bind point for this texture declaration.
  RecipeTextureDecl &WithBindPoint(uint32_t bindPoint) & {
    BindPoint = bindPoint;
    return *this;
  }

  /// @brief Sets the register bind point for this texture declaration.
  RecipeTextureDecl &&WithBindPoint(uint32_t bindPoint) && {
    BindPoint = bindPoint;
    return std::move(*this);
  }

  /// @brief Sets the texture dimension (1, 2, 3, etc.).
  RecipeTextureDecl &WithDimension(uint32_t dimension) & {
    Dimension = dimension;
    return *this;
  }

  /// @brief Sets the texture dimension (1, 2, 3, etc.).
  RecipeTextureDecl &&WithDimension(uint32_t dimension) && {
    Dimension = dimension;
    return std::move(*this);
  }

  /// @brief Sets the recipe handle used to reference this texture.
  RecipeTextureDecl &WithHandle(std::string handle) & {
    Handle = std::move(handle);
    return *this;
  }

  /// @brief Sets the recipe handle used to reference this texture.
  RecipeTextureDecl &&WithHandle(std::string handle) && {
    Handle = std::move(handle);
    return std::move(*this);
  }

  /// @brief Requests automatic bind-point assignment to the next available slot.
  RecipeTextureDecl &AutoBindToNext(bool autoBind = true) & {
    AutoBind = autoBind;
    return *this;
  }

  /// @brief Requests automatic bind-point assignment to the next available slot.
  RecipeTextureDecl &&AutoBindToNext(bool autoBind = true) && {
    AutoBind = autoBind;
    return std::move(*this);
  }
};

/// @brief Declares a temporary register handle consumed by add_temp steps.
struct RecipeTempDecl {
  std::string Handle;

  /// @brief Sets the recipe handle for this temporary declaration.
  RecipeTempDecl &WithHandle(std::string handle) & {
    Handle = std::move(handle);
    return *this;
  }

  /// @brief Sets the recipe handle for this temporary declaration.
  RecipeTempDecl &&WithHandle(std::string handle) && {
    Handle = std::move(handle);
    return std::move(*this);
  }
};

/// @brief Declares an input signature binding to add or reference.
struct RecipeInputDecl {
  uint32_t BindPoint = 0;
  uint32_t InterpolationMode = 2u;
  std::string Handle;
  bool AutoBind = false;

  /// @brief Sets the register bind point for this input declaration.
  RecipeInputDecl &WithBindPoint(uint32_t bindPoint) & {
    BindPoint = bindPoint;
    return *this;
  }

  /// @brief Sets the register bind point for this input declaration.
  RecipeInputDecl &&WithBindPoint(uint32_t bindPoint) && {
    BindPoint = bindPoint;
    return std::move(*this);
  }

  /// @brief Sets the interpolation mode for this input declaration.
  RecipeInputDecl &WithInterpolationMode(uint32_t interpolationMode) & {
    InterpolationMode = interpolationMode;
    return *this;
  }

  /// @brief Sets the interpolation mode for this input declaration.
  RecipeInputDecl &&WithInterpolationMode(uint32_t interpolationMode) && {
    InterpolationMode = interpolationMode;
    return std::move(*this);
  }

  /// @brief Sets the recipe handle used to reference this input.
  RecipeInputDecl &WithHandle(std::string handle) & {
    Handle = std::move(handle);
    return *this;
  }

  /// @brief Sets the recipe handle used to reference this input.
  RecipeInputDecl &&WithHandle(std::string handle) && {
    Handle = std::move(handle);
    return std::move(*this);
  }

  /// @brief Requests automatic bind-point assignment to the next available slot.
  RecipeInputDecl &AutoBindToNext(bool autoBind = true) & {
    AutoBind = autoBind;
    return *this;
  }

  /// @brief Requests automatic bind-point assignment to the next available slot.
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

  /// @brief Sets the register bind point for this output declaration.
  RecipeOutputDecl &WithBindPoint(uint32_t bindPoint) & {
    BindPoint = bindPoint;
    return *this;
  }

  /// @brief Sets the register bind point for this output declaration.
  RecipeOutputDecl &&WithBindPoint(uint32_t bindPoint) && {
    BindPoint = bindPoint;
    return std::move(*this);
  }

  /// @brief Sets the recipe handle used to reference this output.
  RecipeOutputDecl &WithHandle(std::string handle) & {
    Handle = std::move(handle);
    return *this;
  }

  /// @brief Sets the recipe handle used to reference this output.
  RecipeOutputDecl &&WithHandle(std::string handle) && {
    Handle = std::move(handle);
    return std::move(*this);
  }

  /// @brief Requests automatic bind-point assignment to the next available slot.
  RecipeOutputDecl &AutoBindToNext(bool autoBind = true) & {
    AutoBind = autoBind;
    return *this;
  }

  /// @brief Requests automatic bind-point assignment to the next available slot.
  RecipeOutputDecl &&AutoBindToNext(bool autoBind = true) && {
    AutoBind = autoBind;
    return std::move(*this);
  }
};

/// @brief Declares a constant buffer binding to add or reference.
struct RecipeCBufferDecl {
  uint32_t BindPoint = 0;
  uint32_t Elements = 1;
  uint32_t AccessPattern = 0u;
  std::string Handle;
  bool AutoBind = false;

  /// @brief Sets the register bind point for this constant buffer declaration.
  RecipeCBufferDecl &WithBindPoint(uint32_t bindPoint) & {
    BindPoint = bindPoint;
    return *this;
  }

  /// @brief Sets the register bind point for this constant buffer declaration.
  RecipeCBufferDecl &&WithBindPoint(uint32_t bindPoint) && {
    BindPoint = bindPoint;
    return std::move(*this);
  }

  /// @brief Sets the number of elements in this constant buffer.
  RecipeCBufferDecl &WithElements(uint32_t elements) & {
    Elements = elements;
    return *this;
  }

  /// @brief Sets the number of elements in this constant buffer.
  RecipeCBufferDecl &&WithElements(uint32_t elements) && {
    Elements = elements;
    return std::move(*this);
  }

  /// @brief Sets the access pattern (ImmediateIndexed or DynamicIndexed).
  RecipeCBufferDecl &WithAccessPattern(uint32_t accessPattern) & {
    AccessPattern = accessPattern;
    return *this;
  }

  /// @brief Sets the access pattern (ImmediateIndexed or DynamicIndexed).
  RecipeCBufferDecl &&WithAccessPattern(uint32_t accessPattern) && {
    AccessPattern = accessPattern;
    return std::move(*this);
  }

  /// @brief Sets the recipe handle used to reference this constant buffer.
  RecipeCBufferDecl &WithHandle(std::string handle) & {
    Handle = std::move(handle);
    return *this;
  }

  /// @brief Sets the recipe handle used to reference this constant buffer.
  RecipeCBufferDecl &&WithHandle(std::string handle) && {
    Handle = std::move(handle);
    return std::move(*this);
  }

  /// @brief Requests automatic bind-point assignment to the next available slot.
  RecipeCBufferDecl &AutoBindToNext(bool autoBind = true) & {
    AutoBind = autoBind;
    return *this;
  }

  /// @brief Requests automatic bind-point assignment to the next available slot.
  RecipeCBufferDecl &&AutoBindToNext(bool autoBind = true) && {
    AutoBind = autoBind;
    return std::move(*this);
  }
};

/// @brief Declares a sampler binding to add or reference.
struct RecipeSamplerDecl {
  uint32_t BindPoint = 0;
  uint32_t Mode = 0u;
  std::string Handle;
  bool AutoBind = false;

  /// @brief Sets the register bind point for this sampler declaration.
  RecipeSamplerDecl &WithBindPoint(uint32_t bindPoint) & {
    BindPoint = bindPoint;
    return *this;
  }

  /// @brief Sets the register bind point for this sampler declaration.
  RecipeSamplerDecl &&WithBindPoint(uint32_t bindPoint) && {
    BindPoint = bindPoint;
    return std::move(*this);
  }

  /// @brief Sets the sampler mode (Default, Comparison, or Mono).
  RecipeSamplerDecl &WithMode(uint32_t mode) & {
    Mode = mode;
    return *this;
  }

  /// @brief Sets the sampler mode (Default, Comparison, or Mono).
  RecipeSamplerDecl &&WithMode(uint32_t mode) && {
    Mode = mode;
    return std::move(*this);
  }

  /// @brief Sets the recipe handle used to reference this sampler.
  RecipeSamplerDecl &WithHandle(std::string handle) & {
    Handle = std::move(handle);
    return *this;
  }

  /// @brief Sets the recipe handle used to reference this sampler.
  RecipeSamplerDecl &&WithHandle(std::string handle) && {
    Handle = std::move(handle);
    return std::move(*this);
  }

  /// @brief Requests automatic bind-point assignment to the next available slot.
  RecipeSamplerDecl &AutoBindToNext(bool autoBind = true) & {
    AutoBind = autoBind;
    return *this;
  }

  /// @brief Requests automatic bind-point assignment to the next available slot.
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

  /// @brief Sets the register bind point for this raw resource declaration.
  RecipeRawResourceDecl &WithBindPoint(uint32_t bindPoint) & {
    BindPoint = bindPoint;
    return *this;
  }

  /// @brief Sets the register bind point for this raw resource declaration.
  RecipeRawResourceDecl &&WithBindPoint(uint32_t bindPoint) && {
    BindPoint = bindPoint;
    return std::move(*this);
  }

  /// @brief Sets the recipe handle used to reference this raw resource.
  RecipeRawResourceDecl &WithHandle(std::string handle) & {
    Handle = std::move(handle);
    return *this;
  }

  /// @brief Sets the recipe handle used to reference this raw resource.
  RecipeRawResourceDecl &&WithHandle(std::string handle) && {
    Handle = std::move(handle);
    return std::move(*this);
  }

  /// @brief Requests automatic bind-point assignment to the next available slot.
  RecipeRawResourceDecl &AutoBindToNext(bool autoBind = true) & {
    AutoBind = autoBind;
    return *this;
  }

  /// @brief Requests automatic bind-point assignment to the next available slot.
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

  /// @brief Sets the register bind point for this structured resource declaration.
  RecipeStructuredResourceDecl &WithBindPoint(uint32_t bindPoint) & {
    BindPoint = bindPoint;
    return *this;
  }

  /// @brief Sets the register bind point for this structured resource declaration.
  RecipeStructuredResourceDecl &&WithBindPoint(uint32_t bindPoint) && {
    BindPoint = bindPoint;
    return std::move(*this);
  }

  /// @brief Sets the structure stride in bytes for this structured resource.
  RecipeStructuredResourceDecl &WithStructureStride(uint32_t structureStride) & {
    StructureStride = structureStride;
    return *this;
  }

  /// @brief Sets the structure stride in bytes for this structured resource.
  RecipeStructuredResourceDecl &&WithStructureStride(uint32_t structureStride) && {
    StructureStride = structureStride;
    return std::move(*this);
  }

  /// @brief Sets the recipe handle used to reference this structured resource.
  RecipeStructuredResourceDecl &WithHandle(std::string handle) & {
    Handle = std::move(handle);
    return *this;
  }

  /// @brief Sets the recipe handle used to reference this structured resource.
  RecipeStructuredResourceDecl &&WithHandle(std::string handle) && {
    Handle = std::move(handle);
    return std::move(*this);
  }

  /// @brief Requests automatic bind-point assignment to the next available slot.
  RecipeStructuredResourceDecl &AutoBindToNext(bool autoBind = true) & {
    AutoBind = autoBind;
    return *this;
  }

  /// @brief Requests automatic bind-point assignment to the next available slot.
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
  uint32_t Dimension = 3u;
  uint32_t StructureStride = 16;
  bool GloballyCoherent = false;
  bool HasOrderPreservingCounter = false;
  std::string Handle;
  bool AutoBind = false;

  /// @brief Sets the register bind point for this UAV declaration.
  RecipeUavDecl &WithBindPoint(uint32_t bindPoint) & {
    BindPoint = bindPoint;
    return *this;
  }

  /// @brief Sets the register bind point for this UAV declaration.
  RecipeUavDecl &&WithBindPoint(uint32_t bindPoint) && {
    BindPoint = bindPoint;
    return std::move(*this);
  }

  /// @brief Sets the UAV kind (Typed, Raw, or Structured).
  RecipeUavDecl &WithKind(RecipeUavKind kind) & {
    Kind = kind;
    return *this;
  }

  /// @brief Sets the UAV kind (Typed, Raw, or Structured).
  RecipeUavDecl &&WithKind(RecipeUavKind kind) && {
    Kind = kind;
    return std::move(*this);
  }

  /// @brief Sets the texture dimension for this UAV.
  RecipeUavDecl &WithDimension(uint32_t dimension) & {
    Dimension = dimension;
    return *this;
  }

  /// @brief Sets the texture dimension for this UAV.
  RecipeUavDecl &&WithDimension(uint32_t dimension) && {
    Dimension = dimension;
    return std::move(*this);
  }

  /// @brief Sets the structure stride in bytes for this structured UAV.
  RecipeUavDecl &WithStructureStride(uint32_t structureStride) & {
    StructureStride = structureStride;
    return *this;
  }

  /// @brief Sets the structure stride in bytes for this structured UAV.
  RecipeUavDecl &&WithStructureStride(uint32_t structureStride) && {
    StructureStride = structureStride;
    return std::move(*this);
  }

  /// @brief Enables globally coherent memory semantics for this UAV.
  RecipeUavDecl &WithGloballyCoherent(bool globallyCoherent = true) & {
    GloballyCoherent = globallyCoherent;
    return *this;
  }

  /// @brief Enables globally coherent memory semantics for this UAV.
  RecipeUavDecl &&WithGloballyCoherent(bool globallyCoherent = true) && {
    GloballyCoherent = globallyCoherent;
    return std::move(*this);
  }

  /// @brief Enables an order-preserving counter for this UAV.
  RecipeUavDecl &WithOrderPreservingCounter(bool hasCounter = true) & {
    HasOrderPreservingCounter = hasCounter;
    return *this;
  }

  /// @brief Enables an order-preserving counter for this UAV.
  RecipeUavDecl &&WithOrderPreservingCounter(bool hasCounter = true) && {
    HasOrderPreservingCounter = hasCounter;
    return std::move(*this);
  }

  /// @brief Sets the recipe handle used to reference this UAV.
  RecipeUavDecl &WithHandle(std::string handle) & {
    Handle = std::move(handle);
    return *this;
  }

  /// @brief Sets the recipe handle used to reference this UAV.
  RecipeUavDecl &&WithHandle(std::string handle) && {
    Handle = std::move(handle);
    return std::move(*this);
  }

  /// @brief Requests automatic bind-point assignment to the next available slot.
  RecipeUavDecl &AutoBindToNext(bool autoBind = true) & {
    AutoBind = autoBind;
    return *this;
  }

  /// @brief Requests automatic bind-point assignment to the next available slot.
  RecipeUavDecl &&AutoBindToNext(bool autoBind = true) && {
    AutoBind = autoBind;
    return std::move(*this);
  }
};

/// @brief Encoding used for one index slot in a recipe operand pattern.
///
/// Corresponds directly to `Operand::IndexRepresentation` in the decoded
/// model. Both match and emit patterns record this to allow precise
/// representation filtering during matching and correct token serialization
/// during emit.
enum class RecipeOperandIndexRepresentation {
  Immediate32,             ///< 32-bit immediate index
  Immediate64,             ///< 64-bit immediate index (two DWORDs)
  Relative,                ///< Relative addressing via a sub-operand
  Immediate32PlusRelative, ///< 32-bit immediate plus relative
  Immediate64PlusRelative, ///< 64-bit immediate plus relative
};

/// @brief Shorthand family used when resolving variable-backed immediates.
enum class RecipeImmediateFamily {
  None = 0,
  U32 = 1,
  U64 = 2,
  I32 = 3,
  I64 = 4,
  F32 = 5,
  F64 = 6,
};

struct RecipeOperandPattern;

/// @brief Selects which fields of a captured operand participate in
/// projected match/replay operations.
///
/// When all fields are false, operations fall back to full operand semantics.
/// These helpers mirror the YAML `capture_fields` and `match_capture_fields`
/// replay-object forms.
struct RecipeOperandCaptureFields {
  bool Type = false;
  bool Components = false;
  bool Modifier = false;
  bool Indices = false;
  bool Immediates = false;

  /// @brief Enables type field projection for captured operands.
  RecipeOperandCaptureFields &WithType(bool enabled = true) & {
    Type = enabled;
    return *this;
  }

  /// @brief Enables type field projection for captured operands.
  RecipeOperandCaptureFields &&WithType(bool enabled = true) && {
    Type = enabled;
    return std::move(*this);
  }

  /// @brief Enables component field projection for captured operands.
  RecipeOperandCaptureFields &WithComponents(bool enabled = true) & {
    Components = enabled;
    return *this;
  }

  /// @brief Enables component field projection for captured operands.
  RecipeOperandCaptureFields &&WithComponents(bool enabled = true) && {
    Components = enabled;
    return std::move(*this);
  }

  /// @brief Enables modifier field projection for captured operands.
  RecipeOperandCaptureFields &WithModifier(bool enabled = true) & {
    Modifier = enabled;
    return *this;
  }

  /// @brief Enables modifier field projection for captured operands.
  RecipeOperandCaptureFields &&WithModifier(bool enabled = true) && {
    Modifier = enabled;
    return std::move(*this);
  }

  /// @brief Enables index field projection for captured operands.
  RecipeOperandCaptureFields &WithIndices(bool enabled = true) & {
    Indices = enabled;
    return *this;
  }

  /// @brief Enables index field projection for captured operands.
  RecipeOperandCaptureFields &&WithIndices(bool enabled = true) && {
    Indices = enabled;
    return std::move(*this);
  }

  /// @brief Enables immediate field projection for captured operands.
  RecipeOperandCaptureFields &WithImmediates(bool enabled = true) & {
    Immediates = enabled;
    return *this;
  }

  /// @brief Enables immediate field projection for captured operands.
  RecipeOperandCaptureFields &&WithImmediates(bool enabled = true) && {
    Immediates = enabled;
    return std::move(*this);
  }

  bool AnySelected() const {
    return Type || Components || Modifier || Indices || Immediates;
  }
};

class RecipeOperandCaptureFieldsBuilder {
public:
  RecipeOperandCaptureFieldsBuilder() = default;
  explicit RecipeOperandCaptureFieldsBuilder(
      RecipeOperandCaptureFields fields)
      : fields_(std::move(fields)) {}

  /// @brief Enables type field projection for captured operands.
  RecipeOperandCaptureFieldsBuilder &WithType(bool enabled = true) {
    fields_.Type = enabled;
    return *this;
  }

  /// @brief Enables component field projection for captured operands.
  RecipeOperandCaptureFieldsBuilder &WithComponents(bool enabled = true) {
    fields_.Components = enabled;
    return *this;
  }

  /// @brief Enables modifier field projection for captured operands.
  RecipeOperandCaptureFieldsBuilder &WithModifier(bool enabled = true) {
    fields_.Modifier = enabled;
    return *this;
  }

  /// @brief Enables index field projection for captured operands.
  RecipeOperandCaptureFieldsBuilder &WithIndices(bool enabled = true) {
    fields_.Indices = enabled;
    return *this;
  }

  /// @brief Enables immediate field projection for captured operands.
  RecipeOperandCaptureFieldsBuilder &WithImmediates(bool enabled = true) {
    fields_.Immediates = enabled;
    return *this;
  }

  RecipeOperandCaptureFields Build() const { return fields_; }

  operator RecipeOperandCaptureFields() const { return Build(); }

private:
  RecipeOperandCaptureFields fields_;
};

/// @brief Describes one ordered index slot in a recipe operand pattern.
///
/// Both `match` operands and `emit` operand templates carry an ordered list of
/// these objects, one per expected index slot. The lists are evaluated and
/// instantiated in order.
///
/// **Match semantics** (used in `RecipeMatchPattern::Operands[].IndexPatterns`):
///  - `Any = true`            — wildcard; the slot is accepted without checks.
///  - `Representation`        — when present, the encoded index type must match.
///  - `ImmediateLo/Hi`        — when set, the immediate value must equal.
///  - `Capture`               — on match, stores the slot's immediate in
///                             `context.captures.indexValues` for later use.
///  - `MatchCapture`          — the slot's immediate must equal a previously
///                             captured value (may reference any step).
///
/// **Emit semantics** (used in `RecipeInstructionTemplate::Operands[].IndexPatterns`):
///  - `ImmediateLo/Hi`        — constant immediate value to write.
///  - `MatchCapture`          — resolves the immediate from `context.captures.indexValues`.
///                             May reference captures from any rule in any step.
///  - `Any` and `Capture`     — invalid on emit entries; rejected at compile time.
struct RecipeOperandIndexPattern {
  /// When `true`, this slot is a wildcard (match only). All value fields are
  /// ignored. Invalid on emit entries.
  bool Any = false;
  /// Encoding for this index slot. Defaults to `Immediate32`.
  RecipeOperandIndexRepresentation Representation =
      RecipeOperandIndexRepresentation::Immediate32;
  bool HasImmediateLo = false;
  uint32_t ImmediateLo = 0; ///< Low 32-bit immediate value
  bool HasImmediateHi = false;
  uint32_t ImmediateHi = 0;    ///< High 32-bit immediate (64-bit indices)
  /// Sub-operand for relative addressing; null when not relative.
  std::shared_ptr<RecipeOperandPattern> RelativeOperand;
  /// Match only: capture key for the matched immediate value. Invalid on emit.
  std::string Capture;
  /// Match: compare against a previously captured index value.
  /// Emit: resolve the emitted immediate from a previously captured value.
  std::string MatchCapture;
  /// Emit: resolve immediate_lo from a runtime variable/input key.
  std::string ImmediateLoVariable;
  /// Emit: resolve immediate_hi from a runtime variable/input key.
  std::string ImmediateHiVariable;
  /// Emit: strict conversion family for variable-backed immediates.
  RecipeImmediateFamily ImmediateFamily = RecipeImmediateFamily::None;
};

/// @brief Fluent builder for `RecipeOperandIndexPattern`.
///
/// Used when constructing recipe rules in code. The resulting pattern is added
/// to a `RecipeOperandPattern` via `AddIndexPattern` or `WithIndexPatterns`.
///
/// Example — capture a temp register number and match it again later:
/// @code
/// auto matchIdx = RecipeOperandIndexPatternBuilder{}
///     .WithRepresentation(RecipeOperandIndexRepresentation::Immediate32)
///     .CaptureAs("my_reg")
///     .Build();
///
/// auto emitIdx = RecipeOperandIndexPatternBuilder{}
///     .WithRepresentation(RecipeOperandIndexRepresentation::Immediate32)
///     .WithMatchCapture("my_reg")
///     .Build();
/// @endcode
class RecipeOperandIndexPatternBuilder {
public:
  RecipeOperandIndexPatternBuilder() = default;
  explicit RecipeOperandIndexPatternBuilder(RecipeOperandIndexPattern pattern)
      : pattern_(std::move(pattern)) {}

  /// @brief Sets the wildcard flag. Valid on match patterns only.
  RecipeOperandIndexPatternBuilder &WithAny(bool any = true) {
    pattern_.Any = any;
    return *this;
  }

  /// @brief Sets the expected index encoding for this slot.
  RecipeOperandIndexPatternBuilder &
  WithRepresentation(RecipeOperandIndexRepresentation representation) {
    pattern_.Representation = representation;
    return *this;
  }

  /// @brief Requires (match) or emits (emit) this 32-bit immediate value.
  RecipeOperandIndexPatternBuilder &WithImmediateLo(uint32_t value) {
    pattern_.HasImmediateLo = true;
    pattern_.ImmediateLo = value;
    return *this;
  }

  /// @brief Requires (match) or emits (emit) this high 32-bit immediate value.
  RecipeOperandIndexPatternBuilder &WithImmediateHi(uint32_t value) {
    pattern_.HasImmediateHi = true;
    pattern_.ImmediateHi = value;
    return *this;
  }

  /// @brief Sets a relative sub-operand for relative-addressing index slots.
  RecipeOperandIndexPatternBuilder &
  WithRelativeOperand(RecipeOperandPattern relativeOperand);

  /// @brief Stores the matched immediate in `CapturedOperandIndexValues` under
  /// `capture`. Valid on match patterns only; rejected on emit patterns.
  RecipeOperandIndexPatternBuilder &CaptureAs(std::string capture) {
    pattern_.Capture = std::move(capture);
    return *this;
  }

  /// @brief Match: requires the slot's immediate to equal the captured value
  /// named `matchCapture`. Emit: resolves the emitted immediate from that capture.
  RecipeOperandIndexPatternBuilder &WithMatchCapture(std::string matchCapture) {
    pattern_.MatchCapture = std::move(matchCapture);
    return *this;
  }

  RecipeOperandIndexPattern Build() const { return pattern_; }

  operator RecipeOperandIndexPattern() const { return Build(); }

private:
  RecipeOperandIndexPattern pattern_;
};

/// @brief Describes one operand in a declarative recipe pattern or template.
///
/// Used for both match-side patterns (inside `RecipeInstructionPattern`) and
/// emit-side templates (inside `RecipeInstructionTemplate`). Which fields are
/// valid depends on context:
///
/// **Match fields**: `Any`, `Type`, `IndexPatterns`, `Capture`, `MatchCapture`,
/// `Mask/Swizzle/Select`, `NumComponents`, `Modifier`.
///
/// **Emit fields**: `Capture` (copy a captured operand wholesale),
/// `FromHandle`, `Type`, `IndexPatterns`, `Mask/Swizzle/Select`,
/// `NumComponents`, `Modifier`.
///
/// `IndexPatterns` carries an ordered list of `RecipeOperandIndexPattern`
/// objects, one per expected index slot.
///
/// All named identifiers (captures, match-captures, from-handles, variables,
/// step names, rule names) share a single global namespace and must be unique.
///
/// YAML selector note: `components.kind`/`components.value` are normalized to
/// `Mask`, `Swizzle`, or `Select` during parsing.
///
/// YAML note: emit operands may use either explicit `indices` entries or the
/// operand-level shorthand arrays `immediates_u32` / `immediates_u64` /
/// `immediates_i32` / `immediates_i64` / `immediates_f32` /
/// `immediates_f64`.
/// Shorthands are normalized into `IndexPatterns` during parsing and cannot be
/// combined with explicit `indices` on the same operand. Shorthands are emit
/// only; match operands must use explicit `indices`.
struct RecipeOperandPattern {
  /// When `true`, this operand is a wildcard (match only) and all other fields
  /// are ignored. Invalid on emit operands.
  bool Any = false;
  /// Token name for the operand type (e.g., `"temp"`, `"resource"`).
  std::string Type;
  /// Ordered per-slot index patterns.
  ///
  /// For YAML-authored emit operands, this is also the normalized target for
  /// `immediates_u32`, `immediates_u64`, `immediates_i32`, `immediates_i64`,
  /// `immediates_f32`, and `immediates_f64` shorthand arrays.
  std::vector<RecipeOperandIndexPattern> IndexPatterns;
  std::string FromHandle;
  std::string Mask;
  std::string Swizzle;
  std::string Select;
  int32_t NumComponents = -1;
  std::string Modifier;
  /// YAML replay-object shorthand: `capture: { from: ... }`.
  std::string Capture;
  /// YAML replay-object shorthand: `match_capture: { from: ... }`.
  std::string MatchCapture;
  RecipeOperandCaptureFields CaptureFields;
  RecipeOperandCaptureFields MatchCaptureFields;

  /// @brief Sets the wildcard flag. Valid on match patterns only.
  RecipeOperandPattern &WithAny(bool any = true) & {
    Any = any;
    return *this;
  }

  /// @brief Sets the wildcard flag. Valid on match patterns only.
  RecipeOperandPattern &&WithAny(bool any = true) && {
    Any = any;
    return std::move(*this);
  }

  /// @brief Sets the operand type name (e.g., "temp", "resource").
  RecipeOperandPattern &WithType(std::string type) & {
    Type = std::move(type);
    return *this;
  }

  /// @brief Sets the operand type name (e.g., "temp", "resource").
  RecipeOperandPattern &&WithType(std::string type) && {
    Type = std::move(type);
    return std::move(*this);
  }

  /// @brief Sets the ordered list of index patterns for this operand.
  RecipeOperandPattern &
  WithIndexPatterns(std::vector<RecipeOperandIndexPattern> indexPatterns) & {
    IndexPatterns = std::move(indexPatterns);
    return *this;
  }

  /// @brief Sets the ordered list of index patterns for this operand.
  RecipeOperandPattern &&
  WithIndexPatterns(std::vector<RecipeOperandIndexPattern> indexPatterns) && {
    IndexPatterns = std::move(indexPatterns);
    return std::move(*this);
  }

  /// @brief Appends an index pattern to this operand.
  RecipeOperandPattern &AddIndexPattern(RecipeOperandIndexPattern pattern) & {
    IndexPatterns.push_back(std::move(pattern));
    return *this;
  }

  /// @brief Appends an index pattern to this operand.
  RecipeOperandPattern &&AddIndexPattern(RecipeOperandIndexPattern pattern) && {
    IndexPatterns.push_back(std::move(pattern));
    return std::move(*this);
  }

  /// @brief Sets the handle to copy an operand from in emit templates.
  RecipeOperandPattern &WithFromHandle(std::string fromHandle) & {
    FromHandle = std::move(fromHandle);
    return *this;
  }

  /// @brief Sets the handle to copy an operand from in emit templates.
  RecipeOperandPattern &&WithFromHandle(std::string fromHandle) && {
    FromHandle = std::move(fromHandle);
    return std::move(*this);
  }

  /// @brief Sets the mask string for this operand.
  RecipeOperandPattern &WithMask(std::string mask) & {
    Mask = std::move(mask);
    return *this;
  }

  /// @brief Sets the mask string for this operand.
  RecipeOperandPattern &&WithMask(std::string mask) && {
    Mask = std::move(mask);
    return std::move(*this);
  }

  /// @brief Sets the swizzle string for this operand.
  RecipeOperandPattern &WithSwizzle(std::string swizzle) & {
    Swizzle = std::move(swizzle);
    return *this;
  }

  /// @brief Sets the swizzle string for this operand.
  RecipeOperandPattern &&WithSwizzle(std::string swizzle) && {
    Swizzle = std::move(swizzle);
    return std::move(*this);
  }

  /// @brief Sets the select string for this operand.
  RecipeOperandPattern &WithSelect(std::string select) & {
    Select = std::move(select);
    return *this;
  }

  /// @brief Sets the select string for this operand.
  RecipeOperandPattern &&WithSelect(std::string select) && {
    Select = std::move(select);
    return std::move(*this);
  }

  /// @brief Sets the number of components for this operand.
  RecipeOperandPattern &WithNumComponents(int32_t numComponents) & {
    NumComponents = numComponents;
    return *this;
  }

  /// @brief Sets the number of components for this operand.
  RecipeOperandPattern &&WithNumComponents(int32_t numComponents) && {
    NumComponents = numComponents;
    return std::move(*this);
  }

  /// @brief Sets the operand modifier string.
  RecipeOperandPattern &WithModifier(std::string modifier) & {
    Modifier = std::move(modifier);
    return *this;
  }

  /// @brief Sets the operand modifier string.
  RecipeOperandPattern &&WithModifier(std::string modifier) && {
    Modifier = std::move(modifier);
    return std::move(*this);
  }

  /// @brief Sets the capture name for this operand.
  RecipeOperandPattern &CaptureAs(std::string capture) & {
    Capture = std::move(capture);
    return *this;
  }

  /// @brief Sets the capture name for this operand.
  RecipeOperandPattern &&CaptureAs(std::string capture) && {
    Capture = std::move(capture);
    return std::move(*this);
  }

  /// @brief Sets the match-capture name to compare this operand against.
  RecipeOperandPattern &WithMatchCapture(std::string matchCapture) & {
    MatchCapture = std::move(matchCapture);
    return *this;
  }

  /// @brief Sets the match-capture name to compare this operand against.
  RecipeOperandPattern &&WithMatchCapture(std::string matchCapture) && {
    MatchCapture = std::move(matchCapture);
    return std::move(*this);
  }

  /// @brief Sets the capture fields projection configuration.
  RecipeOperandPattern &WithCaptureFields(
      RecipeOperandCaptureFields captureFields) & {
    CaptureFields = captureFields;
    return *this;
  }

  /// @brief Sets the capture fields projection configuration.
  RecipeOperandPattern &&WithCaptureFields(
      RecipeOperandCaptureFields captureFields) && {
    CaptureFields = captureFields;
    return std::move(*this);
  }

  /// @brief Captures and replays the type field from a named capture.
  RecipeOperandPattern &ReplayTypeFrom(std::string capture) & {
    Capture = std::move(capture);
    CaptureFields.Type = true;
    return *this;
  }

  /// @brief Captures and replays the type field from a named capture.
  RecipeOperandPattern &&ReplayTypeFrom(std::string capture) && {
    Capture = std::move(capture);
    CaptureFields.Type = true;
    return std::move(*this);
  }

  /// @brief Captures and replays the component field from a named capture.
  RecipeOperandPattern &ReplayComponentsFrom(std::string capture) & {
    Capture = std::move(capture);
    CaptureFields.Components = true;
    return *this;
  }

  /// @brief Captures and replays the component field from a named capture.
  RecipeOperandPattern &&ReplayComponentsFrom(std::string capture) && {
    Capture = std::move(capture);
    CaptureFields.Components = true;
    return std::move(*this);
  }

  /// @brief Captures and replays the modifier field from a named capture.
  RecipeOperandPattern &ReplayModifierFrom(std::string capture) & {
    Capture = std::move(capture);
    CaptureFields.Modifier = true;
    return *this;
  }

  /// @brief Captures and replays the modifier field from a named capture.
  RecipeOperandPattern &&ReplayModifierFrom(std::string capture) && {
    Capture = std::move(capture);
    CaptureFields.Modifier = true;
    return std::move(*this);
  }

  /// @brief Captures and replays the index field from a named capture.
  RecipeOperandPattern &ReplayIndicesFrom(std::string capture) & {
    Capture = std::move(capture);
    CaptureFields.Indices = true;
    return *this;
  }

  /// @brief Captures and replays the index field from a named capture.
  RecipeOperandPattern &&ReplayIndicesFrom(std::string capture) && {
    Capture = std::move(capture);
    CaptureFields.Indices = true;
    return std::move(*this);
  }

  /// @brief Captures and replays the immediate field from a named capture.
  RecipeOperandPattern &ReplayImmediatesFrom(std::string capture) & {
    Capture = std::move(capture);
    CaptureFields.Immediates = true;
    return *this;
  }

  /// @brief Captures and replays the immediate field from a named capture.
  RecipeOperandPattern &&ReplayImmediatesFrom(std::string capture) && {
    Capture = std::move(capture);
    CaptureFields.Immediates = true;
    return std::move(*this);
  }

  /// @brief Sets match-capture fields projection configuration.
  RecipeOperandPattern &WithMatchCaptureFields(
      RecipeOperandCaptureFields matchCaptureFields) & {
    MatchCaptureFields = matchCaptureFields;
    return *this;
  }

  /// @brief Sets match-capture fields projection configuration.
  RecipeOperandPattern &&WithMatchCaptureFields(
      RecipeOperandCaptureFields matchCaptureFields) && {
    MatchCaptureFields = matchCaptureFields;
    return std::move(*this);
  }

};

inline RecipeOperandIndexPatternBuilder &
RecipeOperandIndexPatternBuilder::WithRelativeOperand(
    RecipeOperandPattern relativeOperand) {
  pattern_.RelativeOperand =
      std::make_shared<RecipeOperandPattern>(std::move(relativeOperand));
  return *this;
}

class RecipeOperandPatternBuilder {
public:
  RecipeOperandPatternBuilder() = default;
  explicit RecipeOperandPatternBuilder(RecipeOperandPattern pattern)
      : pattern_(std::move(pattern)) {}

  /// @brief Sets the wildcard flag. Valid on match patterns only.
  RecipeOperandPatternBuilder &WithAny(bool any = true) {
    pattern_.Any = any;
    return *this;
  }

  /// @brief Sets the operand type name (e.g., "temp", "resource").
  RecipeOperandPatternBuilder &WithType(std::string type) {
    pattern_.Type = std::move(type);
    return *this;
  }

  /// @brief Appends an index pattern to this operand.
  RecipeOperandPatternBuilder &
  AddIndex(RecipeOperandIndexPattern indexPattern) {
    pattern_.IndexPatterns.push_back(std::move(indexPattern));
    return *this;
  }

  /// @brief Sets the handle to copy an operand from in emit templates.
  RecipeOperandPatternBuilder &WithFromHandle(std::string fromHandle) {
    pattern_.FromHandle = std::move(fromHandle);
    return *this;
  }

  /// @brief Sets the mask string for this operand.
  RecipeOperandPatternBuilder &WithMask(std::string mask) {
    pattern_.Mask = std::move(mask);
    return *this;
  }

  /// @brief Sets the swizzle string for this operand.
  RecipeOperandPatternBuilder &WithSwizzle(std::string swizzle) {
    pattern_.Swizzle = std::move(swizzle);
    return *this;
  }

  /// @brief Sets the select string for this operand.
  RecipeOperandPatternBuilder &WithSelect(std::string select) {
    pattern_.Select = std::move(select);
    return *this;
  }

  /// @brief Sets the number of components for this operand.
  RecipeOperandPatternBuilder &WithNumComponents(int32_t numComponents) {
    pattern_.NumComponents = numComponents;
    return *this;
  }

  /// @brief Sets the operand modifier string.
  RecipeOperandPatternBuilder &WithModifier(std::string modifier) {
    pattern_.Modifier = std::move(modifier);
    return *this;
  }

  /// @brief Sets the capture name for this operand.
  RecipeOperandPatternBuilder &CaptureAs(std::string capture) {
    pattern_.Capture = std::move(capture);
    return *this;
  }

  /// @brief Sets the match-capture name to compare this operand against.
  RecipeOperandPatternBuilder &WithMatchCapture(std::string matchCapture) {
    pattern_.MatchCapture = std::move(matchCapture);
    return *this;
  }

  /// @brief Sets the capture fields projection configuration.
  RecipeOperandPatternBuilder &WithCaptureFields(
      RecipeOperandCaptureFields captureFields) {
    pattern_.CaptureFields = captureFields;
    return *this;
  }

  /// @brief Enables type field projection for captured operands.
  RecipeOperandPatternBuilder &WithCaptureFieldType(bool enabled = true) {
    pattern_.CaptureFields.Type = enabled;
    return *this;
  }

  /// @brief Enables component field projection for captured operands.
  RecipeOperandPatternBuilder &WithCaptureFieldComponents(bool enabled = true) {
    pattern_.CaptureFields.Components = enabled;
    return *this;
  }

  /// @brief Enables modifier field projection for captured operands.
  RecipeOperandPatternBuilder &WithCaptureFieldModifier(bool enabled = true) {
    pattern_.CaptureFields.Modifier = enabled;
    return *this;
  }

  /// @brief Enables index field projection for captured operands.
  RecipeOperandPatternBuilder &WithCaptureFieldIndices(bool enabled = true) {
    pattern_.CaptureFields.Indices = enabled;
    return *this;
  }

  /// @brief Enables immediate field projection for captured operands.
  RecipeOperandPatternBuilder &WithCaptureFieldImmediates(bool enabled = true) {
    pattern_.CaptureFields.Immediates = enabled;
    return *this;
  }

  /// @brief Captures and replays the type field from a named capture.
  RecipeOperandPatternBuilder &ReplayTypeFrom(std::string capture) {
    pattern_.Capture = std::move(capture);
    pattern_.CaptureFields.Type = true;
    return *this;
  }

  /// @brief Captures and replays the component field from a named capture.
  RecipeOperandPatternBuilder &ReplayComponentsFrom(std::string capture) {
    pattern_.Capture = std::move(capture);
    pattern_.CaptureFields.Components = true;
    return *this;
  }

  /// @brief Captures and replays the modifier field from a named capture.
  RecipeOperandPatternBuilder &ReplayModifierFrom(std::string capture) {
    pattern_.Capture = std::move(capture);
    pattern_.CaptureFields.Modifier = true;
    return *this;
  }

  /// @brief Captures and replays the index field from a named capture.
  RecipeOperandPatternBuilder &ReplayIndicesFrom(std::string capture) {
    pattern_.Capture = std::move(capture);
    pattern_.CaptureFields.Indices = true;
    return *this;
  }

  /// @brief Captures and replays the immediate field from a named capture.
  RecipeOperandPatternBuilder &ReplayImmediatesFrom(std::string capture) {
    pattern_.Capture = std::move(capture);
    pattern_.CaptureFields.Immediates = true;
    return *this;
  }

  /// @brief Sets match-capture fields projection configuration.
  RecipeOperandPatternBuilder &WithMatchCaptureFields(
      RecipeOperandCaptureFields matchCaptureFields) {
    pattern_.MatchCaptureFields = matchCaptureFields;
    return *this;
  }

  /// @brief Enables type field projection for match-capture comparisons.
  RecipeOperandPatternBuilder &
  WithMatchCaptureFieldType(bool enabled = true) {
    pattern_.MatchCaptureFields.Type = enabled;
    return *this;
  }

  /// @brief Enables component field projection for match-capture comparisons.
  RecipeOperandPatternBuilder &
  WithMatchCaptureFieldComponents(bool enabled = true) {
    pattern_.MatchCaptureFields.Components = enabled;
    return *this;
  }

  /// @brief Enables modifier field projection for match-capture comparisons.
  RecipeOperandPatternBuilder &
  WithMatchCaptureFieldModifier(bool enabled = true) {
    pattern_.MatchCaptureFields.Modifier = enabled;
    return *this;
  }

  /// @brief Enables index field projection for match-capture comparisons.
  RecipeOperandPatternBuilder &
  WithMatchCaptureFieldIndices(bool enabled = true) {
    pattern_.MatchCaptureFields.Indices = enabled;
    return *this;
  }

  /// @brief Enables immediate field projection for match-capture comparisons.
  RecipeOperandPatternBuilder &
  WithMatchCaptureFieldImmediates(bool enabled = true) {
    pattern_.MatchCaptureFields.Immediates = enabled;
    return *this;
  }

  RecipeOperandPattern Build() const { return pattern_; }

  operator RecipeOperandPattern() const { return Build(); }

private:
  RecipeOperandPattern pattern_;
};

/// @brief Describes one instruction pattern for rule matching.
struct RecipeInstructionPattern {
  std::string Opcode;
  std::string Capture;
  std::string Saturate;
  std::string InterpolationMode;
  int32_t TestBoolean = -1;
  std::vector<RecipeOperandPattern> Operands;

  /// @brief Sets the opcode name for this instruction pattern.
  RecipeInstructionPattern &WithOpcode(std::string opcode) & {
    Opcode = std::move(opcode);
    return *this;
  }

  /// @brief Sets the opcode name for this instruction pattern.
  RecipeInstructionPattern &&WithOpcode(std::string opcode) && {
    Opcode = std::move(opcode);
    return std::move(*this);
  }

  /// @brief Sets the capture name for this instruction pattern.
  RecipeInstructionPattern &CaptureAs(std::string capture) & {
    Capture = std::move(capture);
    return *this;
  }

  /// @brief Sets the capture name for this instruction pattern.
  RecipeInstructionPattern &&CaptureAs(std::string capture) && {
    Capture = std::move(capture);
    return std::move(*this);
  }

  /// @brief Sets the saturate modifier for this instruction pattern.
  RecipeInstructionPattern &WithSaturate(std::string saturate) & {
    Saturate = std::move(saturate);
    return *this;
  }

  /// @brief Sets the saturate modifier for this instruction pattern.
  RecipeInstructionPattern &&WithSaturate(std::string saturate) && {
    Saturate = std::move(saturate);
    return std::move(*this);
  }

  /// @brief Sets the interpolation mode for this instruction pattern.
  RecipeInstructionPattern &WithInterpolationMode(std::string interpolationMode) & {
    InterpolationMode = std::move(interpolationMode);
    return *this;
  }

  /// @brief Sets the interpolation mode for this instruction pattern.
  RecipeInstructionPattern &&WithInterpolationMode(std::string interpolationMode) && {
    InterpolationMode = std::move(interpolationMode);
    return std::move(*this);
  }

  /// @brief Sets the test_boolean control value for this instruction pattern.
  RecipeInstructionPattern &WithTestBoolean(int32_t testBoolean) & {
    TestBoolean = testBoolean;
    return *this;
  }

  /// @brief Sets the test_boolean control value for this instruction pattern.
  RecipeInstructionPattern &&WithTestBoolean(int32_t testBoolean) && {
    TestBoolean = testBoolean;
    return std::move(*this);
  }

  /// @brief Appends an operand pattern to this instruction.
  RecipeInstructionPattern &AddOperand(RecipeOperandPattern operand) & {
    Operands.push_back(std::move(operand));
    return *this;
  }

  /// @brief Appends an operand pattern to this instruction.
  RecipeInstructionPattern &&AddOperand(RecipeOperandPattern operand) && {
    Operands.push_back(std::move(operand));
    return std::move(*this);
  }
};

/// @brief Controls which fields of a captured instruction participate in
/// projected match/replay operations.
///
/// When all fields are false, operations fall back to full instruction semantics.
/// Mirrors the operand-level `RecipeOperandCaptureFields`.
struct RecipeInstructionCaptureFields {
  bool Opcode = false;
  bool Saturate = false;
  bool TestBoolean = false;
  bool Operands = false;
  bool Immediates = false;

  /// @brief Enables opcode field projection for captured instructions.
  RecipeInstructionCaptureFields &WithOpcode(bool enabled = true) & {
    Opcode = enabled;
    return *this;
  }

  /// @brief Enables opcode field projection for captured instructions.
  RecipeInstructionCaptureFields &&WithOpcode(bool enabled = true) && {
    Opcode = enabled;
    return std::move(*this);
  }

  /// @brief Enables saturate field projection for captured instructions.
  RecipeInstructionCaptureFields &WithSaturate(bool enabled = true) & {
    Saturate = enabled;
    return *this;
  }

  /// @brief Enables saturate field projection for captured instructions.
  RecipeInstructionCaptureFields &&WithSaturate(bool enabled = true) && {
    Saturate = enabled;
    return std::move(*this);
  }

  /// @brief Enables test_boolean field projection for captured instructions.
  RecipeInstructionCaptureFields &WithTestBoolean(bool enabled = true) & {
    TestBoolean = enabled;
    return *this;
  }

  /// @brief Enables test_boolean field projection for captured instructions.
  RecipeInstructionCaptureFields &&WithTestBoolean(bool enabled = true) && {
    TestBoolean = enabled;
    return std::move(*this);
  }

  /// @brief Enables operand field projection for captured instructions.
  RecipeInstructionCaptureFields &WithOperands(bool enabled = true) & {
    Operands = enabled;
    return *this;
  }

  /// @brief Enables operand field projection for captured instructions.
  RecipeInstructionCaptureFields &&WithOperands(bool enabled = true) && {
    Operands = enabled;
    return std::move(*this);
  }

  /// @brief Enables immediate field projection for captured instructions.
  RecipeInstructionCaptureFields &WithImmediates(bool enabled = true) & {
    Immediates = enabled;
    return *this;
  }

  /// @brief Enables immediate field projection for captured instructions.
  RecipeInstructionCaptureFields &&WithImmediates(bool enabled = true) && {
    Immediates = enabled;
    return std::move(*this);
  }

  bool AnySelected() const {
    return Opcode || Saturate || TestBoolean || Operands || Immediates;
  }
};

/// @brief Describes one instruction emitted by a rewrite rule.
///
/// An emit instruction may either be constructed from scratch (opcode + operands)
/// or replayed from a previously captured instruction via `Capture` +
/// `CaptureFields`. When `Capture` is set, the instruction is looked up in
/// `context.captures.instructions` and emitted as a raw copy (unless
/// `CaptureFields` projects specific fields).
struct RecipeInstructionTemplate {
  std::string Opcode;
  std::string Saturate;
  std::string InterpolationMode;
  int32_t TestBoolean = -1;
  std::vector<RecipeOperandPattern> Operands;
  /// Capture name for replaying a previously captured instruction wholesale.
  /// Invalid when `Opcode` is also set.
  std::string Capture;
  /// Field projection configuration for captured instruction replay.
  RecipeInstructionCaptureFields CaptureFields;

  /// @brief Sets the opcode name for this instruction template.
  RecipeInstructionTemplate &WithOpcode(std::string opcode) & {
    Opcode = std::move(opcode);
    return *this;
  }

  /// @brief Sets the opcode name for this instruction template.
  RecipeInstructionTemplate &&WithOpcode(std::string opcode) && {
    Opcode = std::move(opcode);
    return std::move(*this);
  }

  /// @brief Sets the saturate modifier for this instruction template.
  RecipeInstructionTemplate &WithSaturate(std::string saturate) & {
    Saturate = std::move(saturate);
    return *this;
  }

  /// @brief Sets the saturate modifier for this instruction template.
  RecipeInstructionTemplate &&WithSaturate(std::string saturate) && {
    Saturate = std::move(saturate);
    return std::move(*this);
  }

  /// @brief Sets the interpolation mode for this instruction template.
  RecipeInstructionTemplate &WithInterpolationMode(std::string interpolationMode) & {
    InterpolationMode = std::move(interpolationMode);
    return *this;
  }

  /// @brief Sets the interpolation mode for this instruction template.
  RecipeInstructionTemplate &&WithInterpolationMode(std::string interpolationMode) && {
    InterpolationMode = std::move(interpolationMode);
    return std::move(*this);
  }

  /// @brief Sets the test_boolean control value for this instruction template.
  RecipeInstructionTemplate &WithTestBoolean(int32_t testBoolean) & {
    TestBoolean = testBoolean;
    return *this;
  }

  /// @brief Sets the test_boolean control value for this instruction template.
  RecipeInstructionTemplate &&WithTestBoolean(int32_t testBoolean) && {
    TestBoolean = testBoolean;
    return std::move(*this);
  }

  /// @brief Appends an operand pattern to this instruction template.
  RecipeInstructionTemplate &AddOperand(RecipeOperandPattern operand) & {
    Operands.push_back(std::move(operand));
    return *this;
  }

  /// @brief Appends an operand pattern to this instruction template.
  RecipeInstructionTemplate &&AddOperand(RecipeOperandPattern operand) && {
    Operands.push_back(std::move(operand));
    return std::move(*this);
  }

  /// @brief Sets the capture name for this instruction template.
  RecipeInstructionTemplate &CaptureAs(std::string capture) & {
    Capture = std::move(capture);
    return *this;
  }

  /// @brief Sets the capture name for this instruction template.
  RecipeInstructionTemplate &&CaptureAs(std::string capture) && {
    Capture = std::move(capture);
    return std::move(*this);
  }

  /// @brief Sets the capture fields projection configuration.
  RecipeInstructionTemplate &WithCaptureFields(
      RecipeInstructionCaptureFields captureFields) & {
    CaptureFields = captureFields;
    return *this;
  }

  /// @brief Sets the capture fields projection configuration.
  RecipeInstructionTemplate &&WithCaptureFields(
      RecipeInstructionCaptureFields captureFields) && {
    CaptureFields = std::move(captureFields);
    return std::move(*this);
  }

  /// @brief Captures and replays the opcode field from a named capture.
  RecipeInstructionTemplate &ReplayOpcodeFrom(std::string capture) & {
    Capture = std::move(capture);
    CaptureFields.Opcode = true;
    return *this;
  }

  /// @brief Captures and replays the opcode field from a named capture.
  RecipeInstructionTemplate &&ReplayOpcodeFrom(std::string capture) && {
    Capture = std::move(capture);
    CaptureFields.Opcode = true;
    return std::move(*this);
  }

  /// @brief Captures and replays the saturate field from a named capture.
  RecipeInstructionTemplate &ReplaySaturateFrom(std::string capture) & {
    Capture = std::move(capture);
    CaptureFields.Saturate = true;
    return *this;
  }

  /// @brief Captures and replays the saturate field from a named capture.
  RecipeInstructionTemplate &&ReplaySaturateFrom(std::string capture) && {
    Capture = std::move(capture);
    CaptureFields.Saturate = true;
    return std::move(*this);
  }

  /// @brief Captures and replays the operands field from a named capture.
  RecipeInstructionTemplate &ReplayOperandsFrom(std::string capture) & {
    Capture = std::move(capture);
    CaptureFields.Operands = true;
    return *this;
  }

  /// @brief Captures and replays the operands field from a named capture.
  RecipeInstructionTemplate &&ReplayOperandsFrom(std::string capture) && {
    Capture = std::move(capture);
    CaptureFields.Operands = true;
    return std::move(*this);
  }
};

/// @brief Describes the top-level match criteria for a recipe rule.
///
/// Captures are stored in `context.captures` and persist across steps.
struct RecipeMatchPattern {
  std::string Opcode;
  std::string Capture;
  std::string Saturate;
  std::string InterpolationMode;
  int32_t TestBoolean = -1;
  std::vector<RecipeOperandPattern> Operands;
  std::vector<RecipeInstructionPattern> Sequence;

  /// @brief Sets the opcode name for this match pattern.
  RecipeMatchPattern &WithOpcode(std::string opcode) & {
    Opcode = std::move(opcode);
    return *this;
  }

  /// @brief Sets the opcode name for this match pattern.
  RecipeMatchPattern &&WithOpcode(std::string opcode) && {
    Opcode = std::move(opcode);
    return std::move(*this);
  }

  /// @brief Sets the capture name for this match pattern.
  RecipeMatchPattern &CaptureAs(std::string capture) & {
    Capture = std::move(capture);
    return *this;
  }

  /// @brief Sets the capture name for this match pattern.
  RecipeMatchPattern &&CaptureAs(std::string capture) && {
    Capture = std::move(capture);
    return std::move(*this);
  }

  /// @brief Sets the saturate modifier for this match pattern.
  RecipeMatchPattern &WithSaturate(std::string saturate) & {
    Saturate = std::move(saturate);
    return *this;
  }

  /// @brief Sets the saturate modifier for this match pattern.
  RecipeMatchPattern &&WithSaturate(std::string saturate) && {
    Saturate = std::move(saturate);
    return std::move(*this);
  }

  /// @brief Sets the interpolation mode for this match pattern.
  RecipeMatchPattern &WithInterpolationMode(std::string interpolationMode) & {
    InterpolationMode = std::move(interpolationMode);
    return *this;
  }

  /// @brief Sets the interpolation mode for this match pattern.
  RecipeMatchPattern &&WithInterpolationMode(std::string interpolationMode) && {
    InterpolationMode = std::move(interpolationMode);
    return std::move(*this);
  }

  /// @brief Sets the test_boolean control value for this match pattern.
  RecipeMatchPattern &WithTestBoolean(int32_t testBoolean) & {
    TestBoolean = testBoolean;
    return *this;
  }

  /// @brief Sets the test_boolean control value for this match pattern.
  RecipeMatchPattern &&WithTestBoolean(int32_t testBoolean) && {
    TestBoolean = testBoolean;
    return std::move(*this);
  }

  /// @brief Appends an operand pattern to this match pattern.
  RecipeMatchPattern &AddOperand(RecipeOperandPattern operand) & {
    Operands.push_back(std::move(operand));
    return *this;
  }

  /// @brief Appends an operand pattern to this match pattern.
  RecipeMatchPattern &&AddOperand(RecipeOperandPattern operand) && {
    Operands.push_back(std::move(operand));
    return std::move(*this);
  }

  /// @brief Appends an instruction pattern to this match sequence.
  RecipeMatchPattern &AddInstruction(RecipeInstructionPattern instruction) & {
    Sequence.push_back(std::move(instruction));
    return *this;
  }

  /// @brief Appends an instruction pattern to this match sequence.
  RecipeMatchPattern &&AddInstruction(RecipeInstructionPattern instruction) && {
    Sequence.push_back(std::move(instruction));
    return std::move(*this);
  }
};

/// @brief Stores one callback-supplied SM5 rule match and its captures.
///
/// Callback matches are normalized into the same runtime rewrite flow used by
/// declarative rules. Captures are stored in `context.captures` for declarative
/// matching; this struct's capture maps are populated only by code callbacks.
///
/// Callbacks may read captures via `GetCapturedOperand`/`GetCapturedInstruction`
/// and write captures via `SetCapturedOperand`/`SetCapturedInstruction` so that
/// subsequent declarative emit templates can reference them by capture name.
struct RecipeRuleMatch {
  uint32_t InstructionIndex = 0;
  uint32_t RangeStartIndex = 0;
  uint32_t RangeEndIndex = 0;
  /// Operands captured by name. Only populated by code callbacks.
  std::unordered_map<std::string, CapturedOperand> CapturedOperands;
  /// Instructions captured by name. Only populated by code callbacks.
  std::unordered_map<std::string, CapturedInstruction> CapturedInstructions;

  /// Per-slot index immediates captured via `RecipeOperandIndexPattern::Capture`.
  /// Only populated by code callbacks.
  std::unordered_map<std::string, uint32_t> CapturedOperandIndexValues;

  /// @brief Looks up a captured operand by name.
  /// @param name Capture name.
  /// @return Pointer to the captured operand, or `nullptr` when absent.
  const CapturedOperand *GetCapturedOperand(const std::string &name) const {
    const auto it = CapturedOperands.find(name);
    return it == CapturedOperands.end() ? nullptr : &it->second;
  }

  /// @brief Looks up a captured instruction by name.
  /// @param name Capture name.
  /// @return Pointer to the captured instruction, or `nullptr` when absent.
  const CapturedInstruction *GetCapturedInstruction(const std::string &name) const {
    const auto it = CapturedInstructions.find(name);
    return it == CapturedInstructions.end() ? nullptr : &it->second;
  }

  /// @brief Stores a captured operand for declarative emit resolution.
  /// @param name Capture name.
  /// @param operand Captured operand data.
  void SetCapturedOperand(std::string_view name, CapturedOperand operand) {
    CapturedOperands[std::string(name)] = std::move(operand);
  }

  /// @brief Stores a captured instruction for declarative emit resolution.
  /// @param name Capture name.
  /// @param instruction Captured instruction data.
  void SetCapturedInstruction(std::string_view name, CapturedInstruction instruction) {
    CapturedInstructions[std::string(name)] = std::move(instruction);
  }

  /// @brief Looks up a per-slot index immediate captured during matching.
  /// @param name Capture name set on a `RecipeOperandIndexPattern`.
  /// @return Pointer to the 32-bit immediate, or `nullptr` when absent.
  const uint32_t *GetCapturedOperandIndexValue(const std::string &name) const {
    const auto it = CapturedOperandIndexValues.find(name);
    return it == CapturedOperandIndexValues.end() ? nullptr : &it->second;
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
/// use declarative `emit` and `match.rewrite_mode` fields instead.
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
/// Callback rewriting is mutually exclusive with declarative `Emit` and
/// `RewriteMode` fields on the same rule.
using RecipeRewriteCallback = std::function<std::vector<RecipeRewriteAction>(
    const Program &, const RecipeRuleMatch &, RecipeContext &)>;

/// @brief Describes one SM5 rewrite rule.
///
/// A rule may be fully declarative through `Match`, `Emit`, and `RewriteMode`,
/// or it may use callback overloads for matching and/or
/// rewriting. Callback and declarative forms are compiled through the same
/// runtime path, but they must not be mixed for the same stage.
///
/// In YAML schema v1, rule names are required and rule outcomes are published
/// into recipe context state under `Name`.
struct RecipeRule {
  std::string Name;
  RecipeMatchPattern Match;
  RecipeMatchCallback MatchCallback;
  std::vector<RecipeInstructionTemplate> Emit;
  int32_t RangeStartOffset = 0;
  int32_t RangeEndOffset = -1;
  int32_t InsertRelativeIndex = -1;
  bool RequiredMatch = false;
  RecipeRuleApplicationMode ApplicationMode = RecipeRuleApplicationMode::First;
  RecipeRuleRewriteMode RewriteMode = RecipeRuleRewriteMode::Replace;
  RecipeRulePredicate Predicate;
  RecipeRewriteCallback RewriteCallback;
  bool RefreshDeclarations = false;

  RecipeRule &Named(std::string name) & {
    Name = std::move(name);
    return *this;
  }

  RecipeRule &&Named(std::string name) && {
    Name = std::move(name);
    return std::move(*this);
  }

  RecipeRule &WithMatch(RecipeMatchPattern match) & {
    Match = std::move(match);
    MatchCallback = {};
    return *this;
  }

  RecipeRule &&WithMatch(RecipeMatchPattern match) && {
    Match = std::move(match);
    MatchCallback = {};
    return std::move(*this);
  }

  RecipeRule &WithMatch(RecipeMatchCallback callback) & {
    Match = RecipeMatchPattern{};
    MatchCallback = std::move(callback);
    return *this;
  }

  RecipeRule &&WithMatch(RecipeMatchCallback callback) && {
    Match = RecipeMatchPattern{};
    MatchCallback = std::move(callback);
    return std::move(*this);
  }

  RecipeRule &AddEmit(RecipeInstructionTemplate instruction) & {
    RewriteCallback = {};
    Emit.push_back(std::move(instruction));
    return *this;
  }

  RecipeRule &&AddEmit(RecipeInstructionTemplate instruction) && {
    RewriteCallback = {};
    Emit.push_back(std::move(instruction));
    return std::move(*this);
  }

  RecipeRule &RangeStart(int32_t offset) & {
    RewriteCallback = {};
    RangeStartOffset = offset;
    return *this;
  }

  RecipeRule &&RangeStart(int32_t offset) && {
    RewriteCallback = {};
    RangeStartOffset = offset;
    return std::move(*this);
  }

  RecipeRule &RangeEnd(int32_t offset) & {
    RewriteCallback = {};
    RangeEndOffset = offset;
    return *this;
  }

  RecipeRule &&RangeEnd(int32_t offset) && {
    RewriteCallback = {};
    RangeEndOffset = offset;
    return std::move(*this);
  }

  RecipeRule &RangeOffsets(int32_t startOffset, int32_t endOffset) & {
    RewriteCallback = {};
    RangeStartOffset = startOffset;
    RangeEndOffset = endOffset;
    return *this;
  }

  RecipeRule &&RangeOffsets(int32_t startOffset, int32_t endOffset) && {
    RewriteCallback = {};
    RangeStartOffset = startOffset;
    RangeEndOffset = endOffset;
    return std::move(*this);
  }

  RecipeRule &InsertAfterRelativeIndex(int32_t index) & {
    RewriteCallback = {};
    InsertRelativeIndex = index;
    return *this;
  }

  RecipeRule &&InsertAfterRelativeIndex(int32_t index) && {
    RewriteCallback = {};
    InsertRelativeIndex = index;
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

  RecipeRule &RequireMatch(bool requiredMatch = true) & {
    RequiredMatch = requiredMatch;
    return *this;
  }

  RecipeRule &&RequireMatch(bool requiredMatch = true) && {
    RequiredMatch = requiredMatch;
    return std::move(*this);
  }

  RecipeRule &RewriteAs(RecipeRuleRewriteMode rewriteMode) & {
    RewriteCallback = {};
    RewriteMode = rewriteMode;
    return *this;
  }

  RecipeRule &&RewriteAs(RecipeRuleRewriteMode rewriteMode) && {
    RewriteCallback = {};
    RewriteMode = rewriteMode;
    return std::move(*this);
  }

  RecipeRule &Rewrite(RecipeRewriteCallback callback) & {
    Emit.clear();
    RangeStartOffset = 0;
    RangeEndOffset = -1;
    InsertRelativeIndex = -1;
    RewriteMode = RecipeRuleRewriteMode::Replace;
    RewriteCallback = std::move(callback);
    return *this;
  }

  RecipeRule &&Rewrite(RecipeRewriteCallback callback) && {
    Emit.clear();
    RangeStartOffset = 0;
    RangeEndOffset = -1;
    InsertRelativeIndex = -1;
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

enum class RecipeConditionCompareOp {
  None,
  Eq,
  Ne,
  Gt,
  Gte,
  Lt,
  Lte,
};

struct RecipeStepComparison {
  std::string State;
  std::string Input;
  std::string Value;

  RecipeStepComparison &FromState(std::string state) & {
    State = std::move(state);
    Input.clear();
    return *this;
  }

  RecipeStepComparison &&FromState(std::string state) && {
    State = std::move(state);
    Input.clear();
    return std::move(*this);
  }

  RecipeStepComparison &FromInput(std::string input) & {
    Input = std::move(input);
    State.clear();
    return *this;
  }

  RecipeStepComparison &&FromInput(std::string input) && {
    Input = std::move(input);
    State.clear();
    return std::move(*this);
  }

  RecipeStepComparison &WithValue(std::string value) & {
    Value = std::move(value);
    return *this;
  }

  RecipeStepComparison &&WithValue(std::string value) && {
    Value = std::move(value);
    return std::move(*this);
  }

  bool IsSet() const {
    return !State.empty() || !Input.empty() || !Value.empty();
  }
};

/// @brief Describes a generic step guard based on recipe context state.
///
/// This is the programmatic equivalent of YAML `if` conditions. Exactly one of
/// `State`, `Input`, `All`, `Any`, or `CompareOp`/`Compare` should be set.
struct RecipeStepCondition {
  std::string State;
  std::string Input;
  std::vector<RecipeStepCondition> All;
  std::vector<RecipeStepCondition> Any;
  RecipeConditionCompareOp CompareOp = RecipeConditionCompareOp::None;
  RecipeStepComparison Compare;
  bool Negate = false;

  static RecipeStepCondition FromState(std::string state,
                                       bool negate = false) {
    RecipeStepCondition condition;
    condition.State = std::move(state);
    condition.Negate = negate;
    return condition;
  }

  static RecipeStepCondition FromInput(std::string input,
                                       bool negate = false) {
    RecipeStepCondition condition;
    condition.Input = std::move(input);
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

  static RecipeStepCondition CompareValue(RecipeConditionCompareOp op,
                                          RecipeStepComparison comparison,
                                          bool negate = false) {
    RecipeStepCondition condition;
    condition.CompareOp = op;
    condition.Compare = std::move(comparison);
    condition.Negate = negate;
    return condition;
  }

  bool IsSet() const {
    return !State.empty() || !Input.empty() || !All.empty() || !Any.empty() ||
           CompareOp != RecipeConditionCompareOp::None;
  }
};

/// @brief Callable signature for custom recipe steps.
using RecipeStepExecutor = std::function<RecipeStepResult(RecipeContext &)>;
using RecipeStepPredicate = std::function<bool(RecipeContext &)>;

/// @brief Represents one executable step in a recipe.
///
/// In YAML schema v1, step names are required and step outcomes are published
/// into recipe context state under `Name`. `AbortOnFailure` is the step-level
/// fail-stop flag formerly exposed as `required` in YAML.
struct RecipeStep {
  std::string Name;
  std::vector<RecipeRule> Rules;
  RecipeRuleApplicationMode ApplicationMode = RecipeRuleApplicationMode::First;
  bool AbortOnFailure = true;
  RecipeStepCondition If;
  RecipeStepExecutor Execute;
  RecipeStepPredicate Predicate;

  RecipeStep &AbortOnFailureFlag(bool abortOnFailure) & {
    AbortOnFailure = abortOnFailure;
    return *this;
  }

  RecipeStep &&AbortOnFailureFlag(bool abortOnFailure) && {
    AbortOnFailure = abortOnFailure;
    return std::move(*this);
  }

  RecipeStep &When(RecipeStepCondition condition) & {
    If = std::move(condition);
    return *this;
  }

  RecipeStep &&When(RecipeStepCondition condition) && {
    If = std::move(condition);
    return std::move(*this);
  }

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

RecipeStepResult MakeRecipeStepSuccess(bool changed = false,
                                       uint32_t matchCount = 0,
                                       bool stopRecipe = false);

RecipeStepResult MakeRecipeStepFailure(RecipeContext &context,
                                       std::string message);

RecipeStep MakeCustomRecipeStep(std::string name, RecipeStepExecutor execute);

RecipeStep MakeRewriteRulesStep(
    std::string name, std::vector<RecipeRule> rules,
    RecipeRuleApplicationMode mode = RecipeRuleApplicationMode::First,
  bool abortOnFailure = true);

RecipeStep MakeCheckShaderVersionStep(std::string name, uint32_t majorVersion,
                                      uint32_t minorVersion,
                                      bool abortOnFailure = true);

RecipeStep MakeCheckOpcodeCountStep(std::string name, std::string opcode,
                                    int32_t expectedCount,
                                    bool abortOnFailure = true);

RecipeStep MakeCheckResourceCountStep(std::string name,
                                      int32_t expectedResourceCount,
                                      bool abortOnFailure = true);

RecipeStep MakeAddTempStep(std::string id, RecipeTempDecl decl);

RecipeStep MakeAddInputStep(std::string id, RecipeInputDecl decl);

RecipeStep MakeAddOutputStep(std::string id, RecipeOutputDecl decl);

RecipeStep MakeAddTextureStep(std::string id, RecipeTextureDecl decl);

RecipeStep MakeAddRawResourceStep(std::string id, RecipeRawResourceDecl decl);

RecipeStep MakeAddStructuredResourceStep(std::string id,
                                         RecipeStructuredResourceDecl decl);

RecipeStep MakeAddCBufferStep(std::string id, RecipeCBufferDecl decl);

RecipeStep MakeAddSamplerStep(std::string id, RecipeSamplerDecl decl);

RecipeStep MakeAddUavStep(std::string id, RecipeUavDecl decl);

/// @brief Declares one typed export from a recipe execution.
struct RecipeExport {
  /// @brief Export data category.
  enum class Kind {
    CapturedOperands,     ///< All captured operands
    CapturedInstructions, ///< All captured instructions
    CapturedIndexValues,  ///< All captured index/immediate values
    Variables,            ///< Recipe variables
    State,                ///< Recipe context state
  };

  Kind kind = Kind::CapturedOperands;
  std::vector<std::string> keys;  ///< Empty = export all; non-empty = filter to these keys

  RecipeExport &Keys(std::vector<std::string> k) & {
    keys = std::move(k);
    return *this;
  }
  RecipeExport &&Keys(std::vector<std::string> k) && {
    keys = std::move(k);
    return std::move(*this);
  }
  RecipeExport &Keys(std::initializer_list<std::string> k) & {
    keys = k;
    return *this;
  }
  RecipeExport &&Keys(std::initializer_list<std::string> k) && {
    keys = k;
    return std::move(*this);
  }
};

/// @brief Owns the declarative SM5 recipe definition.
class Recipe {
public:
  /// @brief Appends a step to the recipe.
  Recipe &AddStep(RecipeStep step) {
    steps_.push_back(std::move(step));
    return *this;
  }

  /// @brief Returns the list of recipe steps in execution order.
  const std::vector<RecipeStep> &GetSteps() const { return steps_; }

  /// @brief Appends a typed export to the recipe.
  Recipe &AddExport(RecipeExport export_) & {
    exports_.push_back(std::move(export_));
    return *this;
  }
  Recipe &&AddExport(RecipeExport export_) && {
    exports_.push_back(std::move(export_));
    return std::move(*this);
  }

  /// @brief Returns the list of recipe exports.
  const std::vector<RecipeExport> &GetExports() const { return exports_; }

private:
  std::vector<RecipeStep> steps_;
  std::vector<RecipeExport> exports_;
};

} // namespace dxp::sm5
