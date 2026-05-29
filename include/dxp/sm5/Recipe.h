#pragma once

#include "dxp/PatchReport.h"
#include "dxp/sm5/Model.h"

#include <any>
#include <cstdint>
#include <functional>
#include <memory>
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
  uint32_t Dimension = 3u;
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
  uint32_t InterpolationMode = 2u;
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
  uint32_t AccessPattern = 0u;
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
  uint32_t Mode = 0u;
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
  uint32_t Dimension = 3u;
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

  RecipeOperandCaptureFields &WithType(bool enabled = true) & {
    Type = enabled;
    return *this;
  }

  RecipeOperandCaptureFields &&WithType(bool enabled = true) && {
    Type = enabled;
    return std::move(*this);
  }

  RecipeOperandCaptureFields &WithComponents(bool enabled = true) & {
    Components = enabled;
    return *this;
  }

  RecipeOperandCaptureFields &&WithComponents(bool enabled = true) && {
    Components = enabled;
    return std::move(*this);
  }

  RecipeOperandCaptureFields &WithModifier(bool enabled = true) & {
    Modifier = enabled;
    return *this;
  }

  RecipeOperandCaptureFields &&WithModifier(bool enabled = true) && {
    Modifier = enabled;
    return std::move(*this);
  }

  RecipeOperandCaptureFields &WithIndices(bool enabled = true) & {
    Indices = enabled;
    return *this;
  }

  RecipeOperandCaptureFields &&WithIndices(bool enabled = true) && {
    Indices = enabled;
    return std::move(*this);
  }

  RecipeOperandCaptureFields &WithImmediates(bool enabled = true) & {
    Immediates = enabled;
    return *this;
  }

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

  RecipeOperandCaptureFieldsBuilder &WithType(bool enabled = true) {
    fields_.Type = enabled;
    return *this;
  }

  RecipeOperandCaptureFieldsBuilder &WithComponents(bool enabled = true) {
    fields_.Components = enabled;
    return *this;
  }

  RecipeOperandCaptureFieldsBuilder &WithModifier(bool enabled = true) {
    fields_.Modifier = enabled;
    return *this;
  }

  RecipeOperandCaptureFieldsBuilder &WithIndices(bool enabled = true) {
    fields_.Indices = enabled;
    return *this;
  }

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
///  - `Capture`               — on a successful match, the slot's immediate is
///                             stored in `CapturedOperandIndexValues` under this
///                             name for use in later operands or emit templates.
///  - `MatchCapture`          — the slot's immediate must equal the value
///                             previously stored under this capture name.
///
/// **Emit semantics** (used in `RecipeInstructionTemplate::Operands[].IndexPatterns`):
///  - `ImmediateLo/Hi`        — constant immediate value to write.
///  - `MatchCapture`          — resolves the immediate from a captured index
///                             value.
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

  /// Sets the wildcard flag. Valid on match patterns only.
  RecipeOperandIndexPatternBuilder &WithAny(bool any = true) {
    pattern_.Any = any;
    return *this;
  }

  /// Sets the expected index encoding for this slot.
  RecipeOperandIndexPatternBuilder &
  WithRepresentation(RecipeOperandIndexRepresentation representation) {
    pattern_.Representation = representation;
    return *this;
  }

  /// Requires (match) or emits (emit) this 32-bit immediate value.
  RecipeOperandIndexPatternBuilder &WithImmediateLo(uint32_t value) {
    pattern_.HasImmediateLo = true;
    pattern_.ImmediateLo = value;
    return *this;
  }

  /// Requires (match) or emits (emit) this high 32-bit immediate value.
  RecipeOperandIndexPatternBuilder &WithImmediateHi(uint32_t value) {
    pattern_.HasImmediateHi = true;
    pattern_.ImmediateHi = value;
    return *this;
  }

  /// Sets a relative sub-operand for relative-addressing index slots.
  RecipeOperandIndexPatternBuilder &
  WithRelativeOperand(RecipeOperandPattern relativeOperand);

  /// Stores the matched immediate in `CapturedOperandIndexValues` under
  /// `capture`. Valid on match patterns only; rejected on emit patterns.
  RecipeOperandIndexPatternBuilder &CaptureAs(std::string capture) {
    pattern_.Capture = std::move(capture);
    return *this;
  }

  /// Match: requires the slot's immediate to equal the captured value named
  /// `matchCapture`. Emit: resolves the emitted immediate from that capture.
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
/// `BindHandle`, `Type`, `IndexPatterns`, `Mask/Swizzle/Select`,
/// `NumComponents`, `Modifier`.
///
/// `IndexPatterns` carries an ordered list of `RecipeOperandIndexPattern`
/// objects, one per expected index slot.
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
  std::string BindHandle;
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

  RecipeOperandPattern &WithAny(bool any = true) & {
    Any = any;
    return *this;
  }

  RecipeOperandPattern &&WithAny(bool any = true) && {
    Any = any;
    return std::move(*this);
  }

  RecipeOperandPattern &WithType(std::string type) & {
    Type = std::move(type);
    return *this;
  }

  RecipeOperandPattern &&WithType(std::string type) && {
    Type = std::move(type);
    return std::move(*this);
  }

  RecipeOperandPattern &
  WithIndexPatterns(std::vector<RecipeOperandIndexPattern> indexPatterns) & {
    IndexPatterns = std::move(indexPatterns);
    return *this;
  }

  RecipeOperandPattern &&
  WithIndexPatterns(std::vector<RecipeOperandIndexPattern> indexPatterns) && {
    IndexPatterns = std::move(indexPatterns);
    return std::move(*this);
  }

  RecipeOperandPattern &AddIndexPattern(RecipeOperandIndexPattern pattern) & {
    IndexPatterns.push_back(std::move(pattern));
    return *this;
  }

  RecipeOperandPattern &&AddIndexPattern(RecipeOperandIndexPattern pattern) && {
    IndexPatterns.push_back(std::move(pattern));
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

  RecipeOperandPattern &WithCaptureFields(
      RecipeOperandCaptureFields captureFields) & {
    CaptureFields = captureFields;
    return *this;
  }

  RecipeOperandPattern &&WithCaptureFields(
      RecipeOperandCaptureFields captureFields) && {
    CaptureFields = captureFields;
    return std::move(*this);
  }

  RecipeOperandPattern &ReplayTypeFrom(std::string capture) & {
    Capture = std::move(capture);
    CaptureFields.Type = true;
    return *this;
  }

  RecipeOperandPattern &&ReplayTypeFrom(std::string capture) && {
    Capture = std::move(capture);
    CaptureFields.Type = true;
    return std::move(*this);
  }

  RecipeOperandPattern &ReplayComponentsFrom(std::string capture) & {
    Capture = std::move(capture);
    CaptureFields.Components = true;
    return *this;
  }

  RecipeOperandPattern &&ReplayComponentsFrom(std::string capture) && {
    Capture = std::move(capture);
    CaptureFields.Components = true;
    return std::move(*this);
  }

  RecipeOperandPattern &ReplayModifierFrom(std::string capture) & {
    Capture = std::move(capture);
    CaptureFields.Modifier = true;
    return *this;
  }

  RecipeOperandPattern &&ReplayModifierFrom(std::string capture) && {
    Capture = std::move(capture);
    CaptureFields.Modifier = true;
    return std::move(*this);
  }

  RecipeOperandPattern &ReplayIndicesFrom(std::string capture) & {
    Capture = std::move(capture);
    CaptureFields.Indices = true;
    return *this;
  }

  RecipeOperandPattern &&ReplayIndicesFrom(std::string capture) && {
    Capture = std::move(capture);
    CaptureFields.Indices = true;
    return std::move(*this);
  }

  RecipeOperandPattern &ReplayImmediatesFrom(std::string capture) & {
    Capture = std::move(capture);
    CaptureFields.Immediates = true;
    return *this;
  }

  RecipeOperandPattern &&ReplayImmediatesFrom(std::string capture) && {
    Capture = std::move(capture);
    CaptureFields.Immediates = true;
    return std::move(*this);
  }

  RecipeOperandPattern &WithMatchCaptureFields(
      RecipeOperandCaptureFields matchCaptureFields) & {
    MatchCaptureFields = matchCaptureFields;
    return *this;
  }

  /// Compare only selected fields against the named capture. This is the
  /// fluent API equivalent of the YAML `match_capture: { from: ... }` form.

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

  RecipeOperandPatternBuilder &WithAny(bool any = true) {
    pattern_.Any = any;
    return *this;
  }

  RecipeOperandPatternBuilder &WithType(std::string type) {
    pattern_.Type = std::move(type);
    return *this;
  }

  RecipeOperandPatternBuilder &
  AddIndex(RecipeOperandIndexPattern indexPattern) {
    pattern_.IndexPatterns.push_back(std::move(indexPattern));
    return *this;
  }

  RecipeOperandPatternBuilder &WithBindHandle(std::string bindHandle) {
    pattern_.BindHandle = std::move(bindHandle);
    return *this;
  }

  RecipeOperandPatternBuilder &WithMask(std::string mask) {
    pattern_.Mask = std::move(mask);
    return *this;
  }

  RecipeOperandPatternBuilder &WithSwizzle(std::string swizzle) {
    pattern_.Swizzle = std::move(swizzle);
    return *this;
  }

  RecipeOperandPatternBuilder &WithSelect(std::string select) {
    pattern_.Select = std::move(select);
    return *this;
  }

  RecipeOperandPatternBuilder &WithNumComponents(int32_t numComponents) {
    pattern_.NumComponents = numComponents;
    return *this;
  }

  RecipeOperandPatternBuilder &WithModifier(std::string modifier) {
    pattern_.Modifier = std::move(modifier);
    return *this;
  }

  RecipeOperandPatternBuilder &CaptureAs(std::string capture) {
    pattern_.Capture = std::move(capture);
    return *this;
  }

  RecipeOperandPatternBuilder &WithMatchCapture(std::string matchCapture) {
    pattern_.MatchCapture = std::move(matchCapture);
    return *this;
  }

  RecipeOperandPatternBuilder &WithCaptureFields(
      RecipeOperandCaptureFields captureFields) {
    pattern_.CaptureFields = captureFields;
    return *this;
  }

  RecipeOperandPatternBuilder &WithCaptureFieldType(bool enabled = true) {
    pattern_.CaptureFields.Type = enabled;
    return *this;
  }

  RecipeOperandPatternBuilder &WithCaptureFieldComponents(bool enabled = true) {
    pattern_.CaptureFields.Components = enabled;
    return *this;
  }

  RecipeOperandPatternBuilder &WithCaptureFieldModifier(bool enabled = true) {
    pattern_.CaptureFields.Modifier = enabled;
    return *this;
  }

  RecipeOperandPatternBuilder &WithCaptureFieldIndices(bool enabled = true) {
    pattern_.CaptureFields.Indices = enabled;
    return *this;
  }

  RecipeOperandPatternBuilder &WithCaptureFieldImmediates(bool enabled = true) {
    pattern_.CaptureFields.Immediates = enabled;
    return *this;
  }

  RecipeOperandPatternBuilder &ReplayTypeFrom(std::string capture) {
    pattern_.Capture = std::move(capture);
    pattern_.CaptureFields.Type = true;
    return *this;
  }

  RecipeOperandPatternBuilder &ReplayComponentsFrom(std::string capture) {
    pattern_.Capture = std::move(capture);
    pattern_.CaptureFields.Components = true;
    return *this;
  }

  RecipeOperandPatternBuilder &ReplayModifierFrom(std::string capture) {
    pattern_.Capture = std::move(capture);
    pattern_.CaptureFields.Modifier = true;
    return *this;
  }

  RecipeOperandPatternBuilder &ReplayIndicesFrom(std::string capture) {
    pattern_.Capture = std::move(capture);
    pattern_.CaptureFields.Indices = true;
    return *this;
  }

  RecipeOperandPatternBuilder &ReplayImmediatesFrom(std::string capture) {
    pattern_.Capture = std::move(capture);
    pattern_.CaptureFields.Immediates = true;
    return *this;
  }

  RecipeOperandPatternBuilder &WithMatchCaptureFields(
      RecipeOperandCaptureFields matchCaptureFields) {
    pattern_.MatchCaptureFields = matchCaptureFields;
    return *this;
  }

  RecipeOperandPatternBuilder &
  WithMatchCaptureFieldType(bool enabled = true) {
    pattern_.MatchCaptureFields.Type = enabled;
    return *this;
  }

  RecipeOperandPatternBuilder &
  WithMatchCaptureFieldComponents(bool enabled = true) {
    pattern_.MatchCaptureFields.Components = enabled;
    return *this;
  }

  RecipeOperandPatternBuilder &
  WithMatchCaptureFieldModifier(bool enabled = true) {
    pattern_.MatchCaptureFields.Modifier = enabled;
    return *this;
  }

  RecipeOperandPatternBuilder &
  WithMatchCaptureFieldIndices(bool enabled = true) {
    pattern_.MatchCaptureFields.Indices = enabled;
    return *this;
  }

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
  /// Operands captured by name during matching or supplied by the callback.
  std::unordered_map<std::string, const Operand *> CapturedOperands;
  /// Instructions captured by name (used with sequence matches).
  std::unordered_map<std::string, const Instruction *> CapturedInstructions;
  /// Program-relative indices for captured instructions.
  std::unordered_map<std::string, uint32_t> CapturedInstructionIndices;
  /// Per-slot index immediate values captured via
  /// `RecipeOperandIndexPattern::Capture` during declarative matching.
  /// Keys are capture names; values are 32-bit immediates from the matched slot.
  std::unordered_map<std::string, uint32_t> CapturedOperandIndexValues;

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
  int32_t RangeStartOffset = 0;
  int32_t RangeEndOffset = -1;
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

  /// @brief Selects the captured instruction targeted by declarative Before or
  /// After rewriting.
  RecipeRule &ReplaceCapture(std::string capture) & {
    RewriteCallback = {};
    Replace = std::move(capture);
    return *this;
  }

  /// @brief Selects the captured instruction targeted by declarative Before or
  /// After rewriting.
  RecipeRule &&ReplaceCapture(std::string capture) && {
    RewriteCallback = {};
    Replace = std::move(capture);
    return std::move(*this);
  }

  /// @brief Sets start offset for ReplaceRange relative to the match window.
  RecipeRule &RangeStart(int32_t offset) & {
    RewriteCallback = {};
    RangeStartOffset = offset;
    return *this;
  }

  /// @brief Sets start offset for ReplaceRange relative to the match window.
  RecipeRule &&RangeStart(int32_t offset) && {
    RewriteCallback = {};
    RangeStartOffset = offset;
    return std::move(*this);
  }

  /// @brief Sets end offset for ReplaceRange relative to the match window.
  RecipeRule &RangeEnd(int32_t offset) & {
    RewriteCallback = {};
    RangeEndOffset = offset;
    return *this;
  }

  /// @brief Sets end offset for ReplaceRange relative to the match window.
  RecipeRule &&RangeEnd(int32_t offset) && {
    RewriteCallback = {};
    RangeEndOffset = offset;
    return std::move(*this);
  }

  /// @brief Sets ReplaceRange offsets relative to the match window.
  RecipeRule &RangeOffsets(int32_t startOffset, int32_t endOffset) & {
    RewriteCallback = {};
    RangeStartOffset = startOffset;
    RangeEndOffset = endOffset;
    return *this;
  }

  /// @brief Sets ReplaceRange offsets relative to the match window.
  RecipeRule &&RangeOffsets(int32_t startOffset, int32_t endOffset) && {
    RewriteCallback = {};
    RangeStartOffset = startOffset;
    RangeEndOffset = endOffset;
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
    RangeStartOffset = 0;
    RangeEndOffset = -1;
    RewriteMode = RecipeRuleRewriteMode::Replace;
    RewriteCallback = std::move(callback);
    return *this;
  }

  /// @brief Uses callback-driven rewriting and clears declarative rewrite data.
  RecipeRule &&Rewrite(RecipeRewriteCallback callback) && {
    Emit.clear();
    Replace.clear();
    RangeStartOffset = 0;
    RangeEndOffset = -1;
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
