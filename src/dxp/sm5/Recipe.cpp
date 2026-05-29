#include "dxp/sm5/Recipe.h"

#include "d3d11TokenizedProgramFormat.hpp"

#include "dxp/PatchReport.h"
#include "dxp/sm5/Serialize.h"
#include "dxp/sm5/Transforms.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <exception>
#include <unordered_set>

namespace dxp::sm5 {

bool AddInputDeclaration(Program &program, const RecipeInputDecl &decl,
                         RecipeContext &context, std::string &error);

bool AddOutputDeclaration(Program &program, const RecipeOutputDecl &decl,
                          RecipeContext &context, std::string &error);

bool AddCBufferDeclaration(Program &program, const RecipeCBufferDecl &decl,
                           RecipeContext &context, std::string &error);

bool AddTextureDeclaration(Program &program, const RecipeTextureDecl &decl,
                           RecipeContext &context, std::string &error);

bool AddRawResourceDeclaration(Program &program,
                               const RecipeRawResourceDecl &decl,
                               RecipeContext &context, std::string &error);

bool AddStructuredResourceDeclaration(Program &program,
                                      const RecipeStructuredResourceDecl &decl,
                                      RecipeContext &context,
                                      std::string &error);

bool AddSamplerDeclaration(Program &program, const RecipeSamplerDecl &decl,
                           RecipeContext &context, std::string &error);

bool AddUavDeclaration(Program &program, const RecipeUavDecl &decl,
                       RecipeContext &context, std::string &error);

namespace {

static std::string Lowercase(const std::string &value) {
  std::string lowered = value;
  for (char &ch : lowered) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return lowered;
}

static bool ResolveOpcodeAndTestBoolean(const std::string &opcodeName,
                                        int32_t requestedTestBoolean,
                                        Opcode &opcode,
                                        int32_t &resolvedTestBoolean,
                                        std::string &error,
                                        const char *context) {
  int32_t implicitTestBoolean = -1;
  if (!ParseOpcodeWithImplicitTestBoolean(opcodeName, opcode,
                                          implicitTestBoolean)) {
    error = std::string("Unknown SM5 opcode in ") + context + ": " + opcodeName;
    return false;
  }

  resolvedTestBoolean = requestedTestBoolean;
  if (implicitTestBoolean >= 0) {
    if (resolvedTestBoolean >= 0 && resolvedTestBoolean != implicitTestBoolean) {
      error = std::string("SM5 opcode alias '") + opcodeName +
              "' conflicts with explicit test_boolean";
      return false;
    }
    resolvedTestBoolean = implicitTestBoolean;
  }

  if (resolvedTestBoolean >= 0 && !OpcodeUsesTestBoolean(opcode)) {
    error = std::string("SM5 test_boolean is not valid for opcode '") +
            opcodeName + "'";
    return false;
  }

  return true;
}

static dxp::PatchSideEffect MakeAddedBindingSideEffect(
    dxp::PatchResourceKind resourceKind, std::string handle,
    uint32_t bindPoint, std::string description) {
  dxp::PatchSideEffect sideEffect;
  sideEffect.Kind = dxp::PatchSideEffectKind::ResourceAdded;
  sideEffect.ResourceKind = resourceKind;
  sideEffect.Handle = std::move(handle);
  sideEffect.BindPoint = bindPoint;
  sideEffect.Space = 0;
  sideEffect.Changed = true;
  sideEffect.Description = std::move(description);
  return sideEffect;
}

static uint32_t ResolveBindingPoint(
    const std::unordered_map<std::string, uint32_t> &bindings,
    const std::string &handle, uint32_t fallbackBindPoint) {
  const auto it = bindings.find(handle);
  if (it != bindings.end())
    return it->second;
  return fallbackBindPoint;
}

static void AppendBindingExports(dxp::PatchReport &report,
                                 const std::vector<dxp::PatchSideEffect> &sideEffects) {
  for (const dxp::PatchSideEffect &sideEffect : sideEffects) {
    if (sideEffect.Kind != dxp::PatchSideEffectKind::ResourceAdded ||
        sideEffect.Handle.empty()) {
      continue;
    }

    dxp::PatchBindingValue binding;
    binding.Handle = sideEffect.Handle;
    binding.ResourceKind = sideEffect.ResourceKind;
    binding.BindPoint = sideEffect.BindPoint;
    binding.Space = sideEffect.Space;
    report.NewBindings[sideEffect.Handle] = std::move(binding);
  }
}

static std::string InferSingleAppliedRuleName(
    const std::vector<dxp::PatchRuleReport> &ruleReports) {
  const dxp::PatchRuleReport *appliedRule = nullptr;
  for (const dxp::PatchRuleReport &ruleReport : ruleReports) {
    if (ruleReport.AppliedCount == 0)
      continue;
    if (appliedRule != nullptr)
      return std::string();
    appliedRule = &ruleReport;
  }
  return appliedRule != nullptr ? appliedRule->Name : std::string();
}

struct RuntimeRule {
  InstructionMatch Match;
  std::vector<InstructionMatch> MatchSequence;
  bool HasMatchSequence = false;
  RecipeMatchCallback MatchCallback;
  std::vector<Instruction> Emit;
  std::string Replace;
  int32_t RangeStartOffset = 0;
  int32_t RangeEndOffset = -1;
  RecipeRuleApplicationMode ApplicationMode = RecipeRuleApplicationMode::First;
  RecipeRuleRewriteMode RewriteMode = RecipeRuleRewriteMode::Replace;
  std::function<bool(RecipeContext &)> Predicate;
  RecipeRewriteCallback RewriteCallback;
};

static bool HasDeclarativeMatchPattern(const RecipeMatchPattern &match) {
  return !match.Opcode.empty() || !match.Capture.empty() ||
         !match.Saturate.empty() || !match.InterpolationMode.empty() ||
         match.TestBoolean >= 0 || !match.Operands.empty() ||
         !match.Sequence.empty();
}

static bool HasDeclarativeRewritePlan(const RecipeRule &rule) {
  return !rule.Emit.empty() || !rule.Replace.empty() ||
         rule.RewriteMode != RecipeRuleRewriteMode::Replace;
}

struct CaptureNameTables {
  std::unordered_set<std::string> Instructions;
  std::unordered_set<std::string> Operands;
  std::unordered_set<std::string> OperandIndices;
};

static void CollectOperandCaptures(const RecipeOperandPattern &operand,
                                   CaptureNameTables &captures) {
  if (!operand.Capture.empty()) {
    captures.Operands.insert(operand.Capture);
  }

  for (const RecipeOperandIndexPattern &indexPattern : operand.IndexPatterns) {
    if (!indexPattern.Capture.empty()) {
      captures.OperandIndices.insert(indexPattern.Capture);
    }
    if (indexPattern.RelativeOperand) {
      CollectOperandCaptures(*indexPattern.RelativeOperand, captures);
    }
  }
}

static void CollectInstructionCaptures(const RecipeInstructionPattern &instruction,
                                       CaptureNameTables &captures) {
  if (!instruction.Capture.empty()) {
    captures.Instructions.insert(instruction.Capture);
  }

  for (const RecipeOperandPattern &operand : instruction.Operands) {
    CollectOperandCaptures(operand, captures);
  }
}

static void CollectMatchCaptures(const RecipeMatchPattern &match,
                                 CaptureNameTables &captures) {
  if (!match.Capture.empty()) {
    captures.Instructions.insert(match.Capture);
  }

  for (const RecipeOperandPattern &operand : match.Operands) {
    CollectOperandCaptures(operand, captures);
  }

  for (const RecipeInstructionPattern &instruction : match.Sequence) {
    CollectInstructionCaptures(instruction, captures);
  }
}

static bool DescribeCaptureKind(const CaptureNameTables &captures,
                                const std::string &capture,
                                std::string &kindDescription) {
  if (captures.Operands.find(capture) != captures.Operands.end()) {
    kindDescription = "operand";
    return true;
  }
  if (captures.OperandIndices.find(capture) != captures.OperandIndices.end()) {
    kindDescription = "index";
    return true;
  }
  if (captures.Instructions.find(capture) != captures.Instructions.end()) {
    kindDescription = "instruction";
    return true;
  }
  kindDescription.clear();
  return false;
}

static bool ValidateCaptureReference(const CaptureNameTables &captures,
                                     const std::string &capture,
                                     const char *expectedKind,
                                     const std::unordered_set<std::string> &expectedSet,
                                     const char *referenceSite,
                                     std::string &error) {
  if (capture.empty()) {
    return true;
  }

  if (expectedSet.find(capture) != expectedSet.end()) {
    return true;
  }

  std::string actualKind;
  if (DescribeCaptureKind(captures, capture, actualKind)) {
    error = std::string("SM5 ") + referenceSite + " '" + capture +
            "' expects " + expectedKind + " capture but found " +
            actualKind + " capture";
    return false;
  }

  error = std::string("SM5 ") + referenceSite + " '" + capture +
          "' references an unknown capture";
  return false;
}

static bool ValidateOperandCaptureReferences(const RecipeOperandPattern &operand,
                                             const CaptureNameTables &captures,
                                             bool emitOperand,
                                             std::string &error) {
  if (!operand.MatchCapture.empty()) {
    if (!ValidateCaptureReference(captures, operand.MatchCapture, "operand",
                                  captures.Operands, "operand match_capture",
                                  error)) {
      return false;
    }
  }

  if (emitOperand && !operand.Capture.empty()) {
    if (!ValidateCaptureReference(captures, operand.Capture, "operand",
                                  captures.Operands, "emit operand capture",
                                  error)) {
      return false;
    }
  }

  for (const RecipeOperandIndexPattern &indexPattern : operand.IndexPatterns) {
    if (!indexPattern.MatchCapture.empty()) {
      const char *referenceSite =
          emitOperand ? "emit index match_capture" : "index match_capture";
      if (!ValidateCaptureReference(captures, indexPattern.MatchCapture,
                                    "index", captures.OperandIndices,
                                    referenceSite, error)) {
        return false;
      }
    }

    if (indexPattern.RelativeOperand) {
      if (!ValidateOperandCaptureReferences(*indexPattern.RelativeOperand,
                                            captures, emitOperand, error)) {
        return false;
      }
    }
  }

  return true;
}

static bool ValidateRuleCaptureReferences(const RecipeRule &rule,
                                          std::string &error) {
  CaptureNameTables captures;
  CollectMatchCaptures(rule.Match, captures);

  if (!rule.Replace.empty()) {
    if (!ValidateCaptureReference(captures, rule.Replace, "instruction",
                                  captures.Instructions, "replace capture",
                                  error)) {
      return false;
    }
  }

  for (const RecipeOperandPattern &operand : rule.Match.Operands) {
    if (!ValidateOperandCaptureReferences(operand, captures, false, error)) {
      return false;
    }
  }

  for (const RecipeInstructionPattern &instruction : rule.Match.Sequence) {
    for (const RecipeOperandPattern &operand : instruction.Operands) {
      if (!ValidateOperandCaptureReferences(operand, captures, false, error)) {
        return false;
      }
    }
  }

  for (const RecipeInstructionTemplate &instruction : rule.Emit) {
    for (const RecipeOperandPattern &operand : instruction.Operands) {
      if (!ValidateOperandCaptureReferences(operand, captures, true, error)) {
        return false;
      }
    }
  }

  return true;
}

static MatchResult ToRuntimeMatchResult(const RecipeRuleMatch &match) {
  MatchResult runtimeMatch;
  runtimeMatch.InstructionIndex = match.InstructionIndex;
  runtimeMatch.Instruction = match.InstructionHandle;
  runtimeMatch.RangeStartIndex = match.RangeStartIndex;
  runtimeMatch.RangeEndIndex = match.RangeEndIndex;
  runtimeMatch.CapturedOperands = match.CapturedOperands;
  runtimeMatch.CapturedInstructions = match.CapturedInstructions;
  runtimeMatch.CapturedInstructionIndices = match.CapturedInstructionIndices;
  runtimeMatch.CapturedOperandIndexValues = match.CapturedOperandIndexValues;
  return runtimeMatch;
}

static RewriteActionType
ToRuntimeRewriteActionType(RecipeRewriteActionKind kind) {
  switch (kind) {
  case RecipeRewriteActionKind::ReplaceOne:
    return RewriteActionType::ReplaceOne;
  case RecipeRewriteActionKind::ReplaceRange:
    return RewriteActionType::ReplaceRange;
  case RecipeRewriteActionKind::InsertBefore:
    return RewriteActionType::InsertBefore;
  case RecipeRewriteActionKind::InsertAfter:
    return RewriteActionType::InsertAfter;
  case RecipeRewriteActionKind::RemoveRange:
    return RewriteActionType::RemoveRange;
  }

  return RewriteActionType::ReplaceOne;
}

static bool IsMutatingRewriteMode(RecipeRuleRewriteMode mode) {
  return mode != RecipeRuleRewriteMode::None;
}

static uint32_t GetRewriteActionAnchorIndex(const RewriteAction &action) {
  switch (action.Type) {
  case RewriteActionType::ReplaceOne:
    return action.ReplaceIndex;
  case RewriteActionType::ReplaceRange:
    return action.RangeStart;
  case RewriteActionType::InsertBefore:
  case RewriteActionType::InsertAfter:
    return action.InsertPosition;
  case RewriteActionType::RemoveRange:
    return action.RemoveStart;
  }

  return 0;
}

struct RuntimePrefilter {
  PrefilterKind Kind = PrefilterKind::CheckShaderVersion;
  std::string Name;
  bool Required = true;
  uint32_t ExpectedMajorVersion = 0;
  uint32_t ExpectedMinorVersion = 0;
  Opcode FilterOpcode = Opcode::Unknown();
  int32_t ExpectedCount = 0;
  int32_t ExpectedResourceCount = 0;
  InstructionMatch Match;
  std::vector<InstructionMatch> MatchSequence;
  bool HasMatch = false;
  bool HasMatchSequence = false;
};

static bool ResolveRangeReplacement(const RuntimeRule &rule,
                                    const MatchResult &match,
                                    RewriteAction &action,
                                    std::string &error);

static Instruction FinalizeInstruction(Instruction instruction);

static bool BuildRewriteInstructions(const std::vector<Instruction> &templates,
                                     const MatchResult &match,
                                     uint32_t baseTempCount,
                                     RecipeContext &context,
                                     std::vector<Instruction> &instructions,
                                     uint32_t &requiredTempCount,
                                     std::string &error);

static bool ParseBoolToken(const std::string &value, bool &parsedValue,
                           std::string &error) {
  const std::string lowered = Lowercase(value);
  if (lowered == "true") {
    parsedValue = true;
    return true;
  }
  if (lowered == "false") {
    parsedValue = false;
    return true;
  }

  error = "expected boolean token, got '" + value + "'";
  return false;
}

static bool ParseInterpolationModeToken(const std::string &value,
                                        uint32_t &mode, std::string &error) {
  const std::string lowered = Lowercase(value);
  if (lowered == "undefined") {
    mode = D3D10_SB_INTERPOLATION_UNDEFINED;
    return true;
  }
  if (lowered == "constant") {
    mode = D3D10_SB_INTERPOLATION_CONSTANT;
    return true;
  }
  if (lowered == "linear") {
    mode = D3D10_SB_INTERPOLATION_LINEAR;
    return true;
  }
  if (lowered == "linear_centroid" || lowered == "linearcentroid") {
    mode = D3D10_SB_INTERPOLATION_LINEAR_CENTROID;
    return true;
  }
  if (lowered == "linear_noperspective" || lowered == "linearnoperspective") {
    mode = D3D10_SB_INTERPOLATION_LINEAR_NOPERSPECTIVE;
    return true;
  }
  if (lowered == "linear_noperspective_centroid" ||
      lowered == "linearnoperspectivecentroid") {
    mode = D3D10_SB_INTERPOLATION_LINEAR_NOPERSPECTIVE_CENTROID;
    return true;
  }
  if (lowered == "linear_sample" || lowered == "linearsample") {
    mode = D3D10_SB_INTERPOLATION_LINEAR_SAMPLE;
    return true;
  }
  if (lowered == "linear_noperspective_sample" ||
      lowered == "linearnoperspectivesample") {
    mode = D3D10_SB_INTERPOLATION_LINEAR_NOPERSPECTIVE_SAMPLE;
    return true;
  }

  error = "unsupported SM5 interpolation mode: " + value;
  return false;
}

static bool ParseOperandTypeToken(const std::string &value, OperandType &type,
                                  std::string &error) {
  const std::string lowered = Lowercase(value);
  if (lowered == "temp") {
    type = D3D10_SB_OPERAND_TYPE_TEMP;
    return true;
  }
  if (lowered == "input") {
    type = D3D10_SB_OPERAND_TYPE_INPUT;
    return true;
  }
  if (lowered == "output") {
    type = D3D10_SB_OPERAND_TYPE_OUTPUT;
    return true;
  }
  if (lowered == "indexable_temp") {
    type = D3D10_SB_OPERAND_TYPE_INDEXABLE_TEMP;
    return true;
  }
  if (lowered == "immediate32") {
    type = D3D10_SB_OPERAND_TYPE_IMMEDIATE32;
    return true;
  }
  if (lowered == "immediate64") {
    type = D3D10_SB_OPERAND_TYPE_IMMEDIATE64;
    return true;
  }
  if (lowered == "sampler") {
    type = D3D10_SB_OPERAND_TYPE_SAMPLER;
    return true;
  }
  if (lowered == "resource") {
    type = D3D10_SB_OPERAND_TYPE_RESOURCE;
    return true;
  }
  if (lowered == "unordered_access_view" || lowered == "uav") {
    type = D3D11_SB_OPERAND_TYPE_UNORDERED_ACCESS_VIEW;
    return true;
  }
  if (lowered == "constant_buffer" || lowered == "cbuffer") {
    type = D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER;
    return true;
  }
  if (lowered == "output_depth") {
    type = D3D10_SB_OPERAND_TYPE_OUTPUT_DEPTH;
    return true;
  }

  char *end = nullptr;
  const unsigned long parsed = std::strtoul(value.c_str(), &end, 0);
  if (end != nullptr && *end == '\0') {
    type = static_cast<OperandType>(parsed);
    return true;
  }

  error = "unsupported SM5 operand type: " + value;
  return false;
}

static bool ParseOperandModifierToken(const std::string &value,
                                      OperandModifier &modifier,
                                      std::string &error) {
  const std::string lowered = Lowercase(value);
  if (lowered.empty() || lowered == "none") {
    modifier = D3D10_SB_OPERAND_MODIFIER_NONE;
    return true;
  }
  if (lowered == "neg" || lowered == "minus") {
    modifier = D3D10_SB_OPERAND_MODIFIER_NEG;
    return true;
  }
  if (lowered == "abs") {
    modifier = D3D10_SB_OPERAND_MODIFIER_ABS;
    return true;
  }
  if (lowered == "absneg" || lowered == "abs_neg") {
    modifier = D3D10_SB_OPERAND_MODIFIER_ABSNEG;
    return true;
  }

  error = "unsupported SM5 operand modifier: " + value;
  return false;
}

static bool TryParseComponentChar(char ch, D3D10_SB_4_COMPONENT_NAME &component,
                                  std::string &error) {
  switch (static_cast<char>(std::tolower(static_cast<unsigned char>(ch)))) {
  case 'x':
    component = D3D10_SB_4_COMPONENT_X;
    return true;
  case 'y':
    component = D3D10_SB_4_COMPONENT_Y;
    return true;
  case 'z':
    component = D3D10_SB_4_COMPONENT_Z;
    return true;
  case 'w':
    component = D3D10_SB_4_COMPONENT_W;
    return true;
  default:
    error = std::string("unsupported SM5 component selector: ") + ch;
    return false;
  }
}

static bool ParseOperandComponentMode(const RecipeOperandPattern &operandModel,
                                      OperandType operandType,
                                      uint32_t &numComponents,
                                      uint32_t &componentMode,
                                      std::string &error) {
  if (!operandModel.Select.empty()) {
    D3D10_SB_4_COMPONENT_NAME component = D3D10_SB_4_COMPONENT_X;
    if (operandModel.Select.size() != 1 ||
        !TryParseComponentChar(operandModel.Select.front(), component, error)) {
      return false;
    }
    numComponents = D3D10_SB_OPERAND_4_COMPONENT;
    componentMode = ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
                        D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE) |
                    ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECT_1(component);
    return true;
  }

  if (!operandModel.Mask.empty()) {
    uint32_t mask = 0;
    for (char ch : operandModel.Mask) {
      switch (static_cast<char>(std::tolower(static_cast<unsigned char>(ch)))) {
      case 'x':
        mask |= D3D10_SB_OPERAND_4_COMPONENT_MASK_X;
        break;
      case 'y':
        mask |= D3D10_SB_OPERAND_4_COMPONENT_MASK_Y;
        break;
      case 'z':
        mask |= D3D10_SB_OPERAND_4_COMPONENT_MASK_Z;
        break;
      case 'w':
        mask |= D3D10_SB_OPERAND_4_COMPONENT_MASK_W;
        break;
      default:
        error = std::string("unsupported SM5 mask component: ") + ch;
        return false;
      }
    }
    numComponents = D3D10_SB_OPERAND_4_COMPONENT;
    componentMode = ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
                        D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) |
                    ENCODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(mask);
    return true;
  }

  if (!operandModel.Swizzle.empty()) {
    if (operandModel.Swizzle.size() != 4) {
      error = "SM5 swizzle requires exactly four components";
      return false;
    }
    D3D10_SB_4_COMPONENT_NAME components[4] = {};
    for (size_t index = 0; index < 4; ++index) {
      if (!TryParseComponentChar(operandModel.Swizzle[index], components[index],
                                 error)) {
        return false;
      }
    }
    numComponents = D3D10_SB_OPERAND_4_COMPONENT;
    componentMode =
        ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
            D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE) |
        ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE(
            components[0], components[1], components[2], components[3]);
    return true;
  }

  if (operandModel.NumComponents >= 0) {
    numComponents = static_cast<uint32_t>(operandModel.NumComponents);
  } else if (operandType == D3D10_SB_OPERAND_TYPE_SAMPLER ||
             operandType == D3D10_SB_OPERAND_TYPE_RESOURCE ||
             operandType == D3D11_SB_OPERAND_TYPE_UNORDERED_ACCESS_VIEW) {
    numComponents = D3D10_SB_OPERAND_0_COMPONENT;
  } else if (operandType == D3D10_SB_OPERAND_TYPE_IMMEDIATE32 ||
             operandType == D3D10_SB_OPERAND_TYPE_IMMEDIATE64) {
    numComponents = operandModel.IndexPatterns.size() > 1
                        ? D3D10_SB_OPERAND_4_COMPONENT
                        : D3D10_SB_OPERAND_1_COMPONENT;
  } else {
    numComponents = D3D10_SB_OPERAND_4_COMPONENT;
  }

  componentMode = 0;
  if (numComponents == D3D10_SB_OPERAND_4_COMPONENT) {
    componentMode = D3D10_SB_OPERAND_4_COMPONENT_NOSWIZZLE;
  }
  return true;
}

  static bool CompileEmitOperand(const RecipeOperandPattern &operandModel,
                                 Operand &operand, std::string &error);

static bool CompileEmitInstructionTemplate(
    const RecipeInstructionTemplate &emitModel, Instruction &instruction,
    std::string &error, const char *errorPrefix = "SM5 emit") {
  instruction = Instruction{};

  if (emitModel.Opcode.empty()) {
    error = std::string(errorPrefix) + " entries require opcode";
    return false;
  }

  int32_t resolvedTestBoolean = emitModel.TestBoolean;
  if (!ResolveOpcodeAndTestBoolean(emitModel.Opcode, emitModel.TestBoolean,
                                   instruction.Opcode, resolvedTestBoolean,
                                   error, "emit")) {
    return false;
  }
  if (!emitModel.Saturate.empty()) {
    bool saturate = false;
    if (!ParseBoolToken(emitModel.Saturate, saturate, error)) {
      error = "invalid " + std::string(errorPrefix) + " saturate value: " +
              error;
      return false;
    }
    instruction.Controls.Saturate = saturate;
  }
    if (resolvedTestBoolean >= 0) {
    instruction.Controls.HasTestBoolean = true;
    instruction.Controls.TestBoolean =
      static_cast<uint32_t>(resolvedTestBoolean);
  }
  if (!emitModel.InterpolationMode.empty()) {
    uint32_t interpolationMode = 0;
    if (!ParseInterpolationModeToken(emitModel.InterpolationMode,
                                     interpolationMode, error)) {
      return false;
    }

    const auto opcode = static_cast<OpcodeType>(instruction.Opcode);
    if (opcode != D3D10_SB_OPCODE_DCL_INPUT_PS &&
        opcode != D3D10_SB_OPCODE_DCL_INPUT_PS_SIV) {
      error = "SM5 interpolation_mode is only valid for dcl_input_ps and "
              "dcl_input_ps_siv";
      return false;
    }

    instruction.Controls.HasInputInterpolationMode = true;
    instruction.Controls.InputInterpolationMode = interpolationMode;
  }
  for (const RecipeOperandPattern &operandModel : emitModel.Operands) {
    Operand operand;
    if (!CompileEmitOperand(operandModel, operand, error)) {
      return false;
    }
    instruction.Operands.push_back(std::move(operand));
  }

  return true;
}

static bool CompileEmitOperand(const RecipeOperandPattern &operandModel,
                               Operand &operand, std::string &error) {
  operand = Operand{};

  if (operandModel.Any) {
    error = "SM5 emit operand cannot use any wildcard";
    return false;
  }

  if (!operandModel.Capture.empty() && !operandModel.BindHandle.empty()) {
    error = "SM5 emit operand cannot use both capture and bind_handle";
    return false;
  }

  if (!operandModel.Capture.empty()) {
    operand.CaptureName = operandModel.Capture;
    return true;
  }

  if (operandModel.Type.empty()) {
    error = "literal SM5 emit operands require type or capture";
    return false;
  }

  if (!operandModel.BindHandle.empty() && operandModel.Type.empty()) {
    error = "SM5 bind_handle emit operands require explicit operand type";
    return false;
  }

  if (!ParseOperandTypeToken(operandModel.Type, operand.Type, error)) {
    return false;
  }

  if (!ParseOperandComponentMode(operandModel, operand.Type,
                                 operand.NumComponents, operand.ComponentMode,
                                 error)) {
    return false;
  }

  operand.IndexEntries.clear();
  if (!operandModel.IndexPatterns.empty()) {
    operand.Indices.clear();
    operand.IndexEntries.reserve(operandModel.IndexPatterns.size());
    for (const RecipeOperandIndexPattern &indexPattern :
         operandModel.IndexPatterns) {
      if (indexPattern.Any) {
        error = "SM5 emit operand indices cannot use any wildcard";
        return false;
      }

      if (!indexPattern.Capture.empty()) {
        error = "SM5 emit operand index cannot use capture";
        return false;
      }

      Operand::Index indexEntry;
      switch (indexPattern.Representation) {
      case RecipeOperandIndexRepresentation::Immediate32:
        indexEntry.Representation = Operand::IndexRepresentation::Immediate32;
        break;
      case RecipeOperandIndexRepresentation::Immediate64:
        indexEntry.Representation = Operand::IndexRepresentation::Immediate64;
        break;
      case RecipeOperandIndexRepresentation::Relative:
        indexEntry.Representation = Operand::IndexRepresentation::Relative;
        break;
      case RecipeOperandIndexRepresentation::Immediate32PlusRelative:
        indexEntry.Representation =
            Operand::IndexRepresentation::Immediate32PlusRelative;
        break;
      case RecipeOperandIndexRepresentation::Immediate64PlusRelative:
        indexEntry.Representation =
            Operand::IndexRepresentation::Immediate64PlusRelative;
        break;
      }

      indexEntry.HasImmediateLo = indexPattern.HasImmediateLo;
      indexEntry.ImmediateLo = indexPattern.ImmediateLo;
      indexEntry.HasImmediateHi = indexPattern.HasImmediateHi;
      indexEntry.ImmediateHi = indexPattern.ImmediateHi;
      indexEntry.MatchCaptureName = indexPattern.MatchCapture;

      if (indexPattern.RelativeOperand) {
        Operand relativeOperand;
        if (!CompileEmitOperand(*indexPattern.RelativeOperand, relativeOperand,
                                error)) {
          return false;
        }
        indexEntry.RelativeOperand =
            std::make_shared<Operand>(std::move(relativeOperand));
      }

      operand.IndexEntries.push_back(std::move(indexEntry));
    }
  }

  for (const Operand::Index &indexEntry : operand.IndexEntries) {
    if (indexEntry.HasImmediateLo) {
      operand.Indices.push_back(indexEntry.ImmediateLo);
    }
    if (indexEntry.HasImmediateHi) {
      operand.Indices.push_back(indexEntry.ImmediateHi);
    }
  }

  if (operand.Type == D3D10_SB_OPERAND_TYPE_IMMEDIATE32 ||
      operand.Type == D3D10_SB_OPERAND_TYPE_IMMEDIATE64) {
    operand.ImmediateValues = operand.Indices;
    operand.Indices.clear();
  }

  operand.BindHandle = operandModel.BindHandle;

  if (!operandModel.Modifier.empty() &&
      !ParseOperandModifierToken(operandModel.Modifier, operand.Modifier,
                                 error)) {
    return false;
  }

  return true;
}

static bool CompileMatchOperand(const RecipeOperandPattern &operandModel,
                                OperandMatch &operandMatch,
                                std::string &error) {
  operandMatch = OperandMatch{};
  operandMatch.Any = operandModel.Any;

  if (operandModel.Any && !operandModel.MatchCapture.empty()) {
    error = "SM5 any operand cannot use match_capture";
    return false;
  }

  if (!operandModel.Type.empty()) {
    if (!ParseOperandTypeToken(operandModel.Type, operandMatch.MatchType,
                               error)) {
      return false;
    }
    operandMatch.HasTypeMatch = true;
  }

  if (!operandModel.IndexPatterns.empty()) {
    operandMatch.MatchIndexPatterns.clear();
    for (const RecipeOperandIndexPattern &indexPattern :
         operandModel.IndexPatterns) {
      OperandIndexMatchPattern matchIndexPattern;
      matchIndexPattern.Any = indexPattern.Any;
      matchIndexPattern.HasRepresentation = true;
      switch (indexPattern.Representation) {
      case RecipeOperandIndexRepresentation::Immediate32:
        matchIndexPattern.Representation =
            Operand::IndexRepresentation::Immediate32;
        break;
      case RecipeOperandIndexRepresentation::Immediate64:
        matchIndexPattern.Representation =
            Operand::IndexRepresentation::Immediate64;
        break;
      case RecipeOperandIndexRepresentation::Relative:
        matchIndexPattern.Representation = Operand::IndexRepresentation::Relative;
        break;
      case RecipeOperandIndexRepresentation::Immediate32PlusRelative:
        matchIndexPattern.Representation =
            Operand::IndexRepresentation::Immediate32PlusRelative;
        break;
      case RecipeOperandIndexRepresentation::Immediate64PlusRelative:
        matchIndexPattern.Representation =
            Operand::IndexRepresentation::Immediate64PlusRelative;
        break;
      }
      matchIndexPattern.HasImmediateLo = indexPattern.HasImmediateLo;
      matchIndexPattern.ImmediateLo = indexPattern.ImmediateLo;
      matchIndexPattern.HasImmediateHi = indexPattern.HasImmediateHi;
      matchIndexPattern.ImmediateHi = indexPattern.ImmediateHi;
      matchIndexPattern.CaptureName = indexPattern.Capture;
      matchIndexPattern.MatchCapture = indexPattern.MatchCapture;

      operandMatch.MatchIndexPatterns.push_back(std::move(matchIndexPattern));
    }
  }

  const OperandType componentType = operandMatch.HasTypeMatch
                                        ? operandMatch.MatchType
                                        : D3D10_SB_OPERAND_TYPE_TEMP;
  uint32_t numComponents = 0;
  uint32_t componentMode = 0;
  if (!operandModel.Mask.empty() || !operandModel.Swizzle.empty() ||
      !operandModel.Select.empty() || operandModel.NumComponents >= 0) {
    if (!ParseOperandComponentMode(operandModel, componentType, numComponents,
                                   componentMode, error)) {
      return false;
    }
    operandMatch.MatchNumComponents = numComponents;
    operandMatch.HasNumComponentsMatch = true;
    operandMatch.MatchComponentMode = componentMode;
    operandMatch.HasComponentMatch = true;
  }

  if (!operandModel.Modifier.empty()) {
    if (!ParseOperandModifierToken(operandModel.Modifier,
                                   operandMatch.MatchModifier, error)) {
      return false;
    }
    operandMatch.HasModifierMatch = true;
  }

  if (operandMatch.HasTypeMatch &&
      (operandMatch.MatchType == D3D10_SB_OPERAND_TYPE_IMMEDIATE32 ||
       operandMatch.MatchType == D3D10_SB_OPERAND_TYPE_IMMEDIATE64) &&
      !operandMatch.MatchIndexPatterns.empty()) {
    for (const OperandIndexMatchPattern &indexPattern :
         operandMatch.MatchIndexPatterns) {
      if (indexPattern.HasImmediateLo) {
        operandMatch.MatchImmediates.push_back(indexPattern.ImmediateLo);
      }
      if (indexPattern.HasImmediateHi) {
        operandMatch.MatchImmediates.push_back(indexPattern.ImmediateHi);
      }
    }
    operandMatch.HasImmediateMatch = true;
  }

  operandMatch.CaptureName = operandModel.Capture;
  operandMatch.MatchAgainstCapture = operandModel.MatchCapture;
  return true;
}

static bool
CompileInstructionPattern(const RecipeInstructionPattern &matchModel,
                          InstructionMatch &instructionMatch,
                          std::string &error) {
  instructionMatch = InstructionMatch{};

  int32_t resolvedTestBoolean = matchModel.TestBoolean;
  if (!ResolveOpcodeAndTestBoolean(matchModel.Opcode, matchModel.TestBoolean,
                                   instructionMatch.Opcode,
                                   resolvedTestBoolean, error, "match")) {
    return false;
  }
  instructionMatch.HasOpcode = true;
  instructionMatch.CaptureName = matchModel.Capture;
  if (!matchModel.Saturate.empty()) {
    bool saturate = false;
    if (!ParseBoolToken(matchModel.Saturate, saturate, error)) {
      error = "invalid SM5 saturate value: " + error;
      return false;
    }
    instructionMatch.HasSaturateMatch = true;
    instructionMatch.SaturateValue = saturate;
  }
    if (resolvedTestBoolean >= 0) {
    instructionMatch.HasTestBooleanMatch = true;
    instructionMatch.MatchTestBoolean =
      static_cast<uint32_t>(resolvedTestBoolean);
  }
  if (!matchModel.InterpolationMode.empty()) {
    uint32_t interpolationMode = 0;
    if (!ParseInterpolationModeToken(matchModel.InterpolationMode,
                                     interpolationMode, error)) {
      return false;
    }
    if (instructionMatch.Opcode != Opcode{D3D10_SB_OPCODE_DCL_INPUT_PS} &&
        instructionMatch.Opcode != Opcode{D3D10_SB_OPCODE_DCL_INPUT_PS_SIV}) {
      error = "SM5 interpolation_mode is only valid for dcl_input_ps and "
              "dcl_input_ps_siv";
      return false;
    }
    instructionMatch.HasInputInterpolationModeMatch = true;
    instructionMatch.MatchInputInterpolationMode = interpolationMode;
  }
  for (const RecipeOperandPattern &operandModel : matchModel.Operands) {
    OperandMatch operandMatch;
    if (!CompileMatchOperand(operandModel, operandMatch, error)) {
      return false;
    }
    instructionMatch.OperandPatterns.push_back(std::move(operandMatch));
  }

  return true;
}

static bool CompileMatchPattern(const RecipeMatchPattern &matchModel,
                                InstructionMatch &singleMatch,
                                std::vector<InstructionMatch> &sequenceMatches,
                                bool &hasMatchSequence, std::string &error) {
  sequenceMatches.clear();
  hasMatchSequence = false;

  if (!matchModel.Sequence.empty()) {
    if (!matchModel.Opcode.empty() || !matchModel.Capture.empty() ||
        !matchModel.Saturate.empty() || !matchModel.InterpolationMode.empty() ||
        matchModel.TestBoolean >= 0 || !matchModel.Operands.empty()) {
      error = "SM5 match.sequence cannot be combined with single-instruction "
              "match fields";
      return false;
    }

    hasMatchSequence = true;
    for (const RecipeInstructionPattern &instructionModel :
         matchModel.Sequence) {
      InstructionMatch instructionMatch;
      if (!CompileInstructionPattern(instructionModel, instructionMatch,
                                     error)) {
        return false;
      }
      sequenceMatches.push_back(std::move(instructionMatch));
    }
    return true;
  }

  if (matchModel.Opcode.empty()) {
    error = "SM5 match patterns require match.opcode or match.sequence";
    return false;
  }

  RecipeInstructionPattern liftedMatch;
  liftedMatch.Opcode = matchModel.Opcode;
  liftedMatch.Capture = matchModel.Capture;
  liftedMatch.Saturate = matchModel.Saturate;
  liftedMatch.InterpolationMode = matchModel.InterpolationMode;
  liftedMatch.TestBoolean = matchModel.TestBoolean;
  liftedMatch.Operands = matchModel.Operands;
  return CompileInstructionPattern(liftedMatch, singleMatch, error);
}

static bool CompileRule(const RecipeRule &ruleModel,
                        RecipeRuleApplicationMode inheritedMode,
                        RuntimeRule &rule, std::string &error) {
  rule = RuntimeRule{};
  rule.ApplicationMode = inheritedMode;
  if (ruleModel.ApplicationMode != RecipeRuleApplicationMode::First ||
      inheritedMode == RecipeRuleApplicationMode::First) {
    rule.ApplicationMode = ruleModel.ApplicationMode;
  }

  const bool hasDeclarativeMatch = HasDeclarativeMatchPattern(ruleModel.Match);
  if (ruleModel.MatchCallback) {
    if (hasDeclarativeMatch) {
      error =
          "SM5 rules cannot combine declarative match patterns with match callbacks";
      return false;
    }
    rule.MatchCallback = ruleModel.MatchCallback;
  } else {
    if (!CompileMatchPattern(ruleModel.Match, rule.Match, rule.MatchSequence,
                             rule.HasMatchSequence, error)) {
      if (error == "SM5 match patterns require match.opcode or match.sequence") {
        error = "SM5 rules require match.opcode or match.sequence";
      }
      return false;
    }
  }

  rule.Replace = ruleModel.Replace;
  rule.RangeStartOffset = ruleModel.RangeStartOffset;
  rule.RangeEndOffset = ruleModel.RangeEndOffset;
  rule.RewriteMode = ruleModel.RewriteMode;
  rule.RewriteCallback = ruleModel.RewriteCallback;

  if (rule.RewriteCallback) {
    if (HasDeclarativeRewritePlan(ruleModel)) {
      error =
          "SM5 rules cannot combine declarative rewrite fields with rewrite callbacks";
      return false;
    }
  } else if (!IsMutatingRewriteMode(rule.RewriteMode)) {
    if (!rule.Replace.empty() || !ruleModel.Emit.empty()) {
      error = "SM5 rewrite mode None cannot be combined with replace or emit";
      return false;
    }
  } else if (ruleModel.Emit.empty()) {
    error = "SM5 rules without emit must use match.rewrite_mode: None";
    return false;
  }

  for (const RecipeInstructionTemplate &emitModel : ruleModel.Emit) {
    Instruction instruction;
    if (!CompileEmitInstructionTemplate(emitModel, instruction, error)) {
      return false;
    }
    rule.Emit.push_back(std::move(instruction));
  }

  if (!ValidateRuleCaptureReferences(ruleModel, error)) {
    return false;
  }

  rule.Predicate = ruleModel.Predicate;

  return true;
}

static bool EvaluateRulePredicate(const RuntimeRule &rule,
                                  const std::string &stepName, bool required,
                                  RecipeContext &context, bool &shouldApply,
                                  std::string &error) {
  shouldApply = true;
  error.clear();
  if (!rule.Predicate) {
    return true;
  }

  try {
    shouldApply = rule.Predicate(context);
    return true;
  } catch (const std::exception &exception) {
    const std::string message = "SM5 rule predicate threw exception in step '" +
                                stepName + "': " + exception.what();
    if (required) {
      error = message;
      return false;
    }
    context.AddDiagnostic(message);
    shouldApply = false;
    return true;
  } catch (...) {
    const std::string message =
        "SM5 rule predicate threw unknown exception in step '" + stepName + "'";
    if (required) {
      error = message;
      return false;
    }
    context.AddDiagnostic(message);
    shouldApply = false;
    return true;
  }
}

static bool EvaluateRuleMatchCallback(const RuntimeRule &rule,
                                      const std::string &stepName,
                                      bool required,
                                      const Program &program,
                                      RecipeContext &context,
                                      std::vector<MatchResult> &matches,
                                      std::string &error) {
  error.clear();
  matches.clear();
  if (!rule.MatchCallback) {
    matches = rule.HasMatchSequence ? CollectSequenceMatches(program, rule.MatchSequence)
                                    : CollectMatches(program, rule.Match);
    return true;
  }

  try {
    const auto callbackMatches = rule.MatchCallback(program, context);
    matches.reserve(callbackMatches.size());
    for (const RecipeRuleMatch &match : callbackMatches) {
      matches.push_back(ToRuntimeMatchResult(match));
    }
    return true;
  } catch (const std::exception &exception) {
    const std::string message = "SM5 rule match callback threw exception in step '" +
                                stepName + "': " + exception.what();
    if (required) {
      error = message;
      return false;
    }
    context.AddDiagnostic(message);
    return true;
  } catch (...) {
    const std::string message =
        "SM5 rule match callback threw unknown exception in step '" + stepName + "'";
    if (required) {
      error = message;
      return false;
    }
    context.AddDiagnostic(message);
    return true;
  }
}

static bool EvaluateRuleRewriteCallback(const RuntimeRule &rule,
                                        const std::string &stepName,
                                        bool required,
                                        const Program &program,
                                        const MatchResult &match,
                                        RecipeContext &context,
                                        std::vector<RewriteAction> &actions,
                                        std::string &error) {
  error.clear();
  actions.clear();
  if (!rule.RewriteCallback) {
    RewriteAction action;
    if (!ResolveRangeReplacement(rule, match, action, error)) {
      return false;
    }

    if (!BuildRewriteInstructions(rule.Emit, match, program.TempCount, context,
                                  action.NewInstructions,
                                  action.RequiredTempCount, error)) {
      return false;
    }

    actions.push_back(std::move(action));
    return true;
  }

  try {
    RecipeRuleMatch publicMatch;
    publicMatch.InstructionIndex = match.InstructionIndex;
    publicMatch.InstructionHandle = match.Instruction;
    publicMatch.RangeStartIndex = match.RangeStartIndex;
    publicMatch.RangeEndIndex = match.RangeEndIndex;
    publicMatch.CapturedOperands = match.CapturedOperands;
    publicMatch.CapturedInstructions = match.CapturedInstructions;
    publicMatch.CapturedInstructionIndices = match.CapturedInstructionIndices;
    publicMatch.CapturedOperandIndexValues = match.CapturedOperandIndexValues;

    const auto callbackActions = rule.RewriteCallback(program, publicMatch, context);
    actions.reserve(callbackActions.size());
    for (const RecipeRewriteAction &actionModel : callbackActions) {
      RewriteAction action;
      action.Type = ToRuntimeRewriteActionType(actionModel.Kind);
      action.ReplaceIndex = actionModel.ReplaceIndex;
      action.RangeStart = actionModel.RangeStart;
      action.RangeEnd = actionModel.RangeEnd;
      action.InsertPosition = actionModel.InsertPosition;
      action.RemoveStart = actionModel.RemoveStart;
      action.RemoveEnd = actionModel.RemoveEnd;
      action.RequiredTempCount = actionModel.RequiredTempCount;

      if (!actionModel.Emit.empty()) {
        std::vector<Instruction> templates;
        templates.reserve(actionModel.Emit.size());
        for (const RecipeInstructionTemplate &emitModel : actionModel.Emit) {
          Instruction instruction;
          if (!CompileEmitInstructionTemplate(emitModel, instruction, error,
                                              "SM5 callback rewrite emit")) {
            return false;
          }
          templates.push_back(std::move(instruction));
        }

        uint32_t compiledTempCount = program.TempCount;
        if (!BuildRewriteInstructions(templates, match, program.TempCount,
                                      context, action.NewInstructions,
                                      compiledTempCount, error)) {
          return false;
        }
        action.RequiredTempCount =
            std::max(action.RequiredTempCount, compiledTempCount);
      }

      actions.push_back(std::move(action));
    }
  } catch (const std::exception &exception) {
    const std::string message =
        "SM5 rule rewrite callback threw exception in step '" + stepName +
        "': " + exception.what();
    if (required) {
      error = message;
      return false;
    }
    context.AddDiagnostic(message);
    return true;
  } catch (...) {
    const std::string message =
        "SM5 rule rewrite callback threw unknown exception in step '" +
        stepName + "'";
    if (required) {
      error = message;
      return false;
    }
    context.AddDiagnostic(message);
    return true;
  }

  return true;
}

static bool CompilePrefilter(const RecipePrefilter &prefilterModel,
                             RuntimePrefilter &prefilter, std::string &error) {
  prefilter = RuntimePrefilter{};
  prefilter.Kind = prefilterModel.Kind;
  prefilter.Name = prefilterModel.Name;
  prefilter.Required = prefilterModel.Required;
  prefilter.ExpectedMajorVersion = prefilterModel.ExpectedMajorVersion;
  prefilter.ExpectedMinorVersion = prefilterModel.ExpectedMinorVersion;
  prefilter.ExpectedCount = prefilterModel.ExpectedCount;
  prefilter.ExpectedResourceCount = prefilterModel.ExpectedResourceCount;
  switch (prefilter.Kind) {
  case PrefilterKind::CheckShaderVersion:
  case PrefilterKind::CheckResourceCount:
    return true;
  case PrefilterKind::CheckOpcodeCount:
    if (!prefilterModel.Opcode.empty() &&
        !ParseOpcode(prefilterModel.Opcode, prefilter.FilterOpcode)) {
      error = "Unknown SM5 opcode in prefilter: " + prefilterModel.Opcode;
      return false;
    }
    return true;
  case PrefilterKind::CheckPatternMatch:
    if (!CompileMatchPattern(prefilterModel.Match, prefilter.Match,
                             prefilter.MatchSequence,
                             prefilter.HasMatchSequence, error)) {
      if (error ==
          "SM5 match patterns require match.opcode or match.sequence") {
        error = "SM5 pattern prefilter requires match.opcode or match.sequence";
      }
      return false;
    }
    prefilter.HasMatch = !prefilter.HasMatchSequence;
    return true;
  }

  return false;
}

static Instruction FinalizeInstruction(Instruction instruction) {
  instruction.RawTokens = EncodeInstruction(instruction);
  instruction.LengthInDwords =
      static_cast<uint32_t>(instruction.RawTokens.size());
  return instruction;
}

static bool IsDeclarationInstruction(const Instruction &instruction) {
  const char *opcodeName = GetOpcodeName(instruction.Opcode);
  return opcodeName != nullptr && std::strncmp(opcodeName, "dcl_", 4) == 0;
}

static Instruction BuildTempDeclaration(uint32_t tempCount) {
  Instruction instruction;
  instruction.Opcode = Opcode{D3D10_SB_OPCODE_DCL_TEMPS};

  Operand operand;
  operand.Type = D3D10_SB_OPERAND_TYPE_TEMP;
  operand.NumComponents = D3D10_SB_OPERAND_0_COMPONENT;
  operand.ComponentMode = 0;
  operand.Indices = {tempCount};
  instruction.Operands.push_back(std::move(operand));

  return FinalizeInstruction(std::move(instruction));
}

static void EnsureTempDeclaration(Program &program,
                                  uint32_t requiredTempCount) {
  if (requiredTempCount <= program.TempCount) {
    return;
  }

  for (auto &instruction : program.Instructions) {
    if (static_cast<OpcodeType>(instruction.Opcode) !=
        D3D10_SB_OPCODE_DCL_TEMPS) {
      continue;
    }

    if (instruction.Operands.empty()) {
      instruction.Operands.push_back(Operand{});
    }

    Operand &operand = instruction.Operands.front();
    operand.Type = D3D10_SB_OPERAND_TYPE_TEMP;
    operand.NumComponents = D3D10_SB_OPERAND_0_COMPONENT;
    operand.ComponentMode = 0;
    operand.Indices = {requiredTempCount};
    if (instruction.RawTokens.size() >= 2) {
      instruction.RawTokens.back() = requiredTempCount;
      instruction.LengthInDwords =
          static_cast<uint32_t>(instruction.RawTokens.size());
    } else {
      instruction = BuildTempDeclaration(requiredTempCount);
    }
    program.TempCount = requiredTempCount;
    program.TempSize = requiredTempCount * 4;
    return;
  }

  uint32_t insertIndex = 0;
  for (uint32_t i = 0; i < program.Instructions.size(); ++i) {
    if (IsDeclarationInstruction(program.Instructions[i])) {
      insertIndex = i + 1;
    }
  }

  program.Instructions.insert(program.Instructions.begin() +
                                  static_cast<ptrdiff_t>(insertIndex),
                              BuildTempDeclaration(requiredTempCount));
  program.TempCount = requiredTempCount;
  program.TempSize = requiredTempCount * 4;
}

static std::vector<uint32_t>
SelectMatchIndices(const std::vector<MatchResult> &matches,
                   RecipeRuleApplicationMode applicationMode) {
  std::vector<uint32_t> selected;
  if (matches.empty()) {
    return selected;
  }

  switch (applicationMode) {
  case RecipeRuleApplicationMode::First:
    selected.push_back(0);
    break;
  case RecipeRuleApplicationMode::Last:
    selected.push_back(static_cast<uint32_t>(matches.size() - 1));
    break;
  case RecipeRuleApplicationMode::MatchAll:
    selected.reserve(matches.size());
    for (uint32_t i = 0; i < matches.size(); ++i) {
      selected.push_back(i);
    }
    break;
  }

  return selected;
}

static bool ResolveReplaceIndex(const RuntimeRule &rule,
                                const MatchResult &match,
                                uint32_t &replaceIndex, std::string &error) {
  replaceIndex = match.RangeStartIndex;
  if (rule.Replace.empty()) {
    return true;
  }

  const uint32_t *capturedIndex =
      match.GetCapturedInstructionIndex(rule.Replace);
  if (capturedIndex == nullptr) {
    error = "missing captured instruction '" + rule.Replace + "'";
    return false;
  }

  replaceIndex = *capturedIndex;
  return true;
}

static bool ResolveReplacementRange(const RuntimeRule &rule,
                                    const MatchResult &match,
                                    uint32_t &rangeStart,
                                    uint32_t &rangeEnd,
                                    std::string &error) {
  const uint32_t windowStart = match.RangeStartIndex;
  const uint32_t windowEnd = match.RangeEndIndex;
  if (windowStart > windowEnd) {
    error = "invalid SM5 match window";
    return false;
  }

  if (rule.RewriteMode == RecipeRuleRewriteMode::Replace) {
    rangeStart = windowStart;
    rangeEnd = windowEnd;
    return true;
  }

  if (rule.RewriteMode != RecipeRuleRewriteMode::ReplaceRange) {
    error = "unsupported replacement window mode";
    return false;
  }

  if (rule.RangeStartOffset < 0 || rule.RangeEndOffset < -1) {
    error = "invalid SM5 replacement range offsets";
    return false;
  }

  const uint32_t windowLength = windowEnd - windowStart + 1;
  const uint32_t startOffset = static_cast<uint32_t>(rule.RangeStartOffset);
  const uint32_t endOffset =
      rule.RangeEndOffset < 0
          ? (windowLength - 1)
          : static_cast<uint32_t>(rule.RangeEndOffset);

  if (startOffset >= windowLength || endOffset >= windowLength) {
    error = "SM5 replacement range offsets are out of match window bounds";
    return false;
  }
  if (startOffset > endOffset) {
    error = "SM5 replacement range start offset must be <= end offset";
    return false;
  }

  rangeStart = windowStart + startOffset;
  rangeEnd = windowStart + endOffset;
  return true;
}

static bool ResolveRangeReplacement(const RuntimeRule &rule,
                                    const MatchResult &match,
                                    RewriteAction &action, std::string &error) {
  if (rule.RewriteMode == RecipeRuleRewriteMode::Replace) {
    uint32_t rangeStart = 0;
    uint32_t rangeEnd = 0;
    if (!ResolveReplacementRange(rule, match, rangeStart, rangeEnd, error)) {
      return false;
    }
    action.Type = RewriteActionType::ReplaceRange;
    action.ReplaceIndex = rangeStart;
    action.RangeStart = rangeStart;
    action.RangeEnd = rangeEnd;
    return true;
  }

  if (rule.RewriteMode == RecipeRuleRewriteMode::Before) {
    uint32_t insertIndex = 0;
    if (!ResolveReplaceIndex(rule, match, insertIndex, error)) {
      return false;
    }
    action.Type = RewriteActionType::InsertBefore;
    action.InsertPosition = insertIndex;
    return true;
  }

  if (rule.RewriteMode == RecipeRuleRewriteMode::After) {
    uint32_t insertIndex = 0;
    if (!ResolveReplaceIndex(rule, match, insertIndex, error)) {
      return false;
    }
    action.Type = RewriteActionType::InsertAfter;
    action.InsertPosition = insertIndex;
    return true;
  }

  if (rule.RewriteMode == RecipeRuleRewriteMode::ReplaceRange) {
    uint32_t rangeStart = 0;
    uint32_t rangeEnd = 0;
    if (!ResolveReplacementRange(rule, match, rangeStart, rangeEnd, error)) {
      return false;
    }
    action.Type = RewriteActionType::ReplaceRange;
    action.ReplaceIndex = rangeStart;
    action.RangeStart = rangeStart;
    action.RangeEnd = rangeEnd;
    return true;
  }

  error = "unsupported SM5 rewrite mode";
  return false;
}

static bool InstantiateOperand(const Operand &operandTemplate,
                               const MatchResult &match,
                               RecipeContext &context, Operand &operand,
                               std::string &error) {
  if (!operandTemplate.CaptureName.empty()) {
    const Operand *capturedOperand =
        match.GetCapturedOperand(operandTemplate.CaptureName);
    if (capturedOperand == nullptr) {
      error = "missing captured operand '" + operandTemplate.CaptureName + "'";
      return false;
    }
    operand = *capturedOperand;
    return true;
  }

  operand = operandTemplate;
  if (!operand.BindHandle.empty()) {
    const uint32_t *resolvedBindPoint = nullptr;
    if (operand.Type == D3D10_SB_OPERAND_TYPE_TEMP) {
      const auto it = context.TempBindings.find(operand.BindHandle);
      if (it != context.TempBindings.end()) {
        resolvedBindPoint = &it->second;
      }
    } else if (operand.Type == D3D10_SB_OPERAND_TYPE_INPUT) {
      const auto it = context.InputBindings.find(operand.BindHandle);
      if (it != context.InputBindings.end()) {
        resolvedBindPoint = &it->second;
      }
    } else if (operand.Type == D3D10_SB_OPERAND_TYPE_OUTPUT) {
      const auto it = context.OutputBindings.find(operand.BindHandle);
      if (it != context.OutputBindings.end()) {
        resolvedBindPoint = &it->second;
      }
    } else if (operand.Type == D3D10_SB_OPERAND_TYPE_RESOURCE) {
      const auto it = context.TextureBindings.find(operand.BindHandle);
      if (it != context.TextureBindings.end()) {
        resolvedBindPoint = &it->second;
      }
      if (resolvedBindPoint == nullptr) {
        const auto rawIt = context.RawResourceBindings.find(operand.BindHandle);
        if (rawIt != context.RawResourceBindings.end()) {
          resolvedBindPoint = &rawIt->second;
        }
      }
      if (resolvedBindPoint == nullptr) {
        const auto structuredIt =
            context.StructuredResourceBindings.find(operand.BindHandle);
        if (structuredIt != context.StructuredResourceBindings.end()) {
          resolvedBindPoint = &structuredIt->second;
        }
      }
    } else if (operand.Type == D3D10_SB_OPERAND_TYPE_SAMPLER) {
      const auto it = context.SamplerBindings.find(operand.BindHandle);
      if (it != context.SamplerBindings.end()) {
        resolvedBindPoint = &it->second;
      }
    } else if (operand.Type == D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER) {
      const auto it = context.CBufferBindings.find(operand.BindHandle);
      if (it != context.CBufferBindings.end()) {
        resolvedBindPoint = &it->second;
      }
    } else if (operand.Type == D3D11_SB_OPERAND_TYPE_UNORDERED_ACCESS_VIEW) {
      const auto it = context.UavBindings.find(operand.BindHandle);
      if (it != context.UavBindings.end()) {
        resolvedBindPoint = &it->second;
      }
    } else {
      error =
          "SM5 bind_handle operand type is unsupported for resource binding";
      return false;
    }

    if (resolvedBindPoint == nullptr) {
      error =
          "missing SM5 declaration handle binding '" + operand.BindHandle + "'";
      return false;
    }

    if (operand.Indices.empty()) {
      operand.Indices.push_back(*resolvedBindPoint);
      if (operand.Type == D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER) {
        operand.Indices.push_back(0);
      }
    } else {
      operand.Indices[0] = *resolvedBindPoint;
      if (operand.Type == D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER &&
          operand.Indices.size() == 1) {
        operand.Indices.push_back(0);
      }
    }
  }

  if (!operand.IndexEntries.empty()) {
    size_t immediateCursor = 0;
    for (Operand::Index &indexEntry : operand.IndexEntries) {
      if (!indexEntry.MatchCaptureName.empty()) {
        const uint32_t *capturedOperandIndex =
            match.GetCapturedOperandIndexValue(indexEntry.MatchCaptureName);
        if (capturedOperandIndex == nullptr) {
          error = "missing captured operand index '" +
                  indexEntry.MatchCaptureName + "'";
          return false;
        }
        indexEntry.HasImmediateLo = true;
        indexEntry.ImmediateLo = *capturedOperandIndex;
      }

      if (indexEntry.HasImmediateLo && immediateCursor < operand.Indices.size()) {
        indexEntry.ImmediateLo = operand.Indices[immediateCursor++];
      }
      if (indexEntry.HasImmediateHi && immediateCursor < operand.Indices.size()) {
        indexEntry.ImmediateHi = operand.Indices[immediateCursor++];
      }

      if (indexEntry.RelativeOperand) {
        Operand instantiatedRelative;
        if (!InstantiateOperand(*indexEntry.RelativeOperand, match, context,
                                instantiatedRelative, error)) {
          return false;
        }
        indexEntry.RelativeOperand =
            std::make_shared<Operand>(std::move(instantiatedRelative));
      }
    }

    // Binding resolution can append legacy indices (for example CB range index).
    // Preserve those values in IndexEntries so serialization keeps operands intact.
    while (immediateCursor < operand.Indices.size()) {
      Operand::Index appendedIndex;
      appendedIndex.Representation = Operand::IndexRepresentation::Immediate32;
      appendedIndex.HasImmediateLo = true;
      appendedIndex.ImmediateLo = operand.Indices[immediateCursor++];
      operand.IndexEntries.push_back(std::move(appendedIndex));
    }
  }

  if (operand.RelativeOperand) {
    Operand instantiatedRelative;
    if (!InstantiateOperand(*operand.RelativeOperand, match, context,
                            instantiatedRelative, error)) {
      return false;
    }
    operand.RelativeOperand =
        std::make_shared<Operand>(std::move(instantiatedRelative));
  }
  return true;
}

static bool InstantiateInstruction(const Instruction &instructionTemplate,
                                   const MatchResult &match,
                                   RecipeContext &context,
                                   Instruction &instruction,
                                   std::string &error) {
  instruction = instructionTemplate;
  instruction.Operands.clear();
  instruction.RawTokens.clear();
  instruction.LengthInDwords = 0;

  for (const Operand &operandTemplate : instructionTemplate.Operands) {
    Operand operand;
    if (!InstantiateOperand(operandTemplate, match, context, operand, error)) {
      return false;
    }
    instruction.Operands.push_back(std::move(operand));
  }

  instruction = FinalizeInstruction(std::move(instruction));
  return true;
}

static bool BuildRewriteInstructions(const std::vector<Instruction> &templates,
                                     const MatchResult &match,
                                     uint32_t baseTempCount,
                                     RecipeContext &context,
                                     std::vector<Instruction> &instructions,
                                     uint32_t &requiredTempCount,
                                     std::string &error) {
  (void)baseTempCount;
  instructions.clear();
  instructions.reserve(templates.size());
  for (const Instruction &instructionTemplate : templates) {
    Instruction instruction;
    if (!InstantiateInstruction(instructionTemplate, match, context,
                                instruction, error)) {
      return false;
    }
    instructions.push_back(std::move(instruction));
  }
  requiredTempCount = baseTempCount;
  return true;
}

static bool PrefilterMatches(const Program &program,
                             const RuntimePrefilter &prefilter) {
  switch (prefilter.Kind) {
  case PrefilterKind::CheckShaderVersion:
    return program.MajorVersion == prefilter.ExpectedMajorVersion &&
           program.MinorVersion == prefilter.ExpectedMinorVersion;
  case PrefilterKind::CheckOpcodeCount: {
    uint32_t count = 0;
    for (const auto &instruction : program.Instructions) {
      if (instruction.Opcode == prefilter.FilterOpcode) {
        ++count;
      }
    }
    if (prefilter.ExpectedCount < 0) {
      return count <= static_cast<uint32_t>(-prefilter.ExpectedCount);
    }
    if (prefilter.ExpectedCount == 0) {
      return count == 0;
    }
    return count >= static_cast<uint32_t>(prefilter.ExpectedCount);
  }
  case PrefilterKind::CheckResourceCount:
    return static_cast<int32_t>(program.Resources.size()) >=
           prefilter.ExpectedResourceCount;
  case PrefilterKind::CheckPatternMatch:
    if (prefilter.HasMatchSequence) {
      return !CollectSequenceMatches(program, prefilter.MatchSequence).empty();
    }
    if (prefilter.HasMatch) {
      return !CollectMatches(program, prefilter.Match).empty();
    }
    return false;
  }

  return false;
}

static RecipeStepResult ExecutePrefilterStep(
    const Program &program, const std::vector<RecipePrefilter> &prefilterModels,
  RecipePrefilterMode mode, const std::string &stateKey,
  RecipeContext &context) {
  uint32_t matchedChecks = 0;
  for (const RecipePrefilter &prefilterModel : prefilterModels) {
    RuntimePrefilter prefilter;
    std::string compileError;
    if (!CompilePrefilter(prefilterModel, prefilter, compileError)) {
      return MakeRecipeStepFailure(context, std::move(compileError));
    }

    if (PrefilterMatches(program, prefilter)) {
      ++matchedChecks;
      if (mode == RecipePrefilterMode::Any) {
        return MakeRecipeStepSuccess(false, matchedChecks);
      }
      continue;
    }

    if (mode == RecipePrefilterMode::All) {
      break;
    }
  }

  const bool matched = mode == RecipePrefilterMode::All
                           ? matchedChecks == prefilterModels.size()
                           : matchedChecks != 0;
  if (!stateKey.empty()) {
    context.SetState<bool>(stateKey, matched);
  }
  return MakeRecipeStepSuccess(false, matchedChecks);
}

static bool EvaluateStepCondition(const RecipeStepCondition &condition,
                                  const RecipeContext &context) {
  if (!condition.IsSet()) {
    return true;
  }

  bool value = false;

  if (!condition.State.empty()) {
    const bool *boolValue = context.FindState<bool>(condition.State);
    if (boolValue != nullptr) {
      value = *boolValue;
    } else {
      const uint32_t *u32Value = context.FindState<uint32_t>(condition.State);
      if (u32Value != nullptr) {
        value = *u32Value != 0;
      } else {
        const int32_t *i32Value = context.FindState<int32_t>(condition.State);
        if (i32Value != nullptr) {
          value = *i32Value != 0;
        } else {
          const std::string *stringValue =
              context.FindState<std::string>(condition.State);
          if (stringValue != nullptr) {
            value = !stringValue->empty();
          }
        }
      }
    }
  } else if (!condition.All.empty()) {
    value = true;
    for (const RecipeStepCondition &child : condition.All) {
      if (!EvaluateStepCondition(child, context)) {
        value = false;
        break;
      }
    }
  } else if (!condition.Any.empty()) {
    value = false;
    for (const RecipeStepCondition &child : condition.Any) {
      if (EvaluateStepCondition(child, context)) {
        value = true;
        break;
      }
    }
  }

  return condition.Negate ? !value : value;
}

static bool ShouldExecuteStep(const RecipeStep &step, RecipeContext &context) {
  if (!EvaluateStepCondition(step.If, context)) {
    return false;
  }
  if (step.Predicate && !step.Predicate(context)) {
    return false;
  }
  return true;
}

static void ApplyReservedTemps(Program &program, const Recipe &recipe,
                               RecipeContext &context) {
  const auto &tempDecls = recipe.GetTempDecls();
  const uint32_t reservedTemps =
      std::max(recipe.GetReservedTempRegisters(),
               static_cast<uint32_t>(tempDecls.size()));
  if (reservedTemps == 0) {
    context.ReservedTempBase = program.TempCount;
    context.ReservedTempCount = 0;
    return;
  }

  const uint32_t reservedBase = program.TempCount;
  EnsureTempDeclaration(program, reservedBase + reservedTemps);
  context.ReservedTempBase = reservedBase;
  context.ReservedTempCount = reservedTemps;

  for (uint32_t i = 0; i < tempDecls.size(); ++i) {
    const std::string &handle = tempDecls[i].Handle;
    if (handle.empty()) {
      continue;
    }
    context.TempBindings.emplace(handle, reservedBase + i);
  }

  context.ProgramModified = true;
  context.ModuleVerified = false;
}

class ScopedProgramBinding {
public:
  ScopedProgramBinding(RecipeContext &context, Program &program)
      : context_(context), previous_(context.ProgramHandle) {
    context_.ProgramHandle = &program;
  }

  ~ScopedProgramBinding() { context_.ProgramHandle = previous_; }

private:
  RecipeContext &context_;
  Program *previous_;
};

static RecipeStepResult
ExecuteRewriteRules(Program &program, const std::string &stepName,
                    const std::vector<RecipeRule> &rules,
                    RecipeRuleApplicationMode mode, bool required,
                    RecipeContext &context) {
  RecipeStepResult result;
  for (size_t ruleIndex = 0; ruleIndex < rules.size(); ++ruleIndex) {
    const RecipeRule &ruleModel = rules[ruleIndex];
    RuntimeRule rule;
    std::string compileError;
    if (!CompileRule(ruleModel, mode, rule, compileError)) {
      return MakeRecipeStepFailure(context, std::move(compileError));
    }

    dxp::PatchRuleReport ruleReport;
    ruleReport.Name = stepName + ".rule" + std::to_string(ruleIndex);

    std::vector<MatchResult> matches;
    std::string matchError;
    if (!EvaluateRuleMatchCallback(rule, stepName, required, program, context,
                     matches, matchError)) {
      return MakeRecipeStepFailure(context, std::move(matchError));
    }
    result.MatchCount += static_cast<uint32_t>(matches.size());
    ruleReport.MatchCount = static_cast<uint32_t>(matches.size());
    if (matches.empty()) {
      result.RuleReports.push_back(std::move(ruleReport));
      if (required)
        return MakeRecipeStepFailure(context,
                                     "required recipe step had no matches");
      continue;
    }

    const auto selectedMatches =
        SelectMatchIndices(matches, rule.ApplicationMode);

    if (!rule.RewriteCallback && !IsMutatingRewriteMode(rule.RewriteMode)) {
      for (uint32_t selectedIndex : selectedMatches) {
        const auto &match = matches[selectedIndex];
        bool shouldApply = true;
        std::string predicateError;
        if (!EvaluateRulePredicate(rule, stepName, required, context,
                                   shouldApply, predicateError)) {
          return MakeRecipeStepFailure(context, std::move(predicateError));
        }
        (void)match;
        if (shouldApply)
          ++ruleReport.AppliedCount;
      }
      result.RuleReports.push_back(std::move(ruleReport));
      continue;
    }

    std::vector<RewriteAction> actions;
    actions.reserve(selectedMatches.size());
    uint32_t requiredTempCount = program.TempCount;
    for (uint32_t selectedIndex : selectedMatches) {
      const auto &match = matches[selectedIndex];
      bool shouldApply = true;
      std::string predicateError;
      if (!EvaluateRulePredicate(rule, stepName, required, context, shouldApply,
                                 predicateError)) {
        return MakeRecipeStepFailure(context, std::move(predicateError));
      }
      if (!shouldApply) {
        continue;
      }

      std::vector<RewriteAction> localActions;
      std::string error;
      if (!EvaluateRuleRewriteCallback(rule, stepName, required, program, match,
                                       context, localActions, error)) {
        return MakeRecipeStepFailure(context, std::move(error));
      }
      if (localActions.empty()) {
        continue;
      }

      for (RewriteAction &action : localActions) {
        requiredTempCount = std::max(requiredTempCount, action.RequiredTempCount);
        actions.push_back(std::move(action));
      }
      ++ruleReport.AppliedCount;
    }

    std::sort(actions.begin(), actions.end(),
              [](const RewriteAction &lhs, const RewriteAction &rhs) {
                const uint32_t lhsIndex = GetRewriteActionAnchorIndex(lhs);
                const uint32_t rhsIndex = GetRewriteActionAnchorIndex(rhs);
                if (lhsIndex != rhsIndex) {
                  return lhsIndex > rhsIndex;
                }

                if (lhs.Type == rhs.Type) {
                  return false;
                }

                if (lhs.Type == RewriteActionType::InsertAfter) {
                  return true;
                }
                if (rhs.Type == RewriteActionType::InsertAfter) {
                  return false;
                }

                if (lhs.Type == RewriteActionType::InsertBefore) {
                  return false;
                }
                if (rhs.Type == RewriteActionType::InsertBefore) {
                  return true;
                }

                return lhsIndex > rhsIndex;
              });

    if (actions.empty()) {
      result.RuleReports.push_back(std::move(ruleReport));
      continue;
    }

    if (!ApplyRewriteActions(program, actions))
      return MakeRecipeStepFailure(context, "failed to apply rewrite action");
    EnsureTempDeclaration(program, requiredTempCount);
    result.Changed = true;
    result.ResourceBindingsChanged = true;
    result.ResourcesRefreshed = false;
    result.ModuleVerified = false;
    ruleReport.Changed = true;
    result.RuleReports.push_back(std::move(ruleReport));
  }

  (void)stepName;
  result.Success = true;
  return result;
}

} // namespace

RecipeStepResult MakeRecipeStepSuccess(bool changed, uint32_t matchCount,
                                       bool stopRecipe) {
  RecipeStepResult result;
  result.Success = true;
  result.Changed = changed;
  result.MatchCount = matchCount;
  result.StopRecipe = stopRecipe;
  return result;
}

RecipeStepResult MakeRecipeStepFailure(RecipeContext &context,
                                       std::string message) {
  RecipeStepResult result;
  result.Success = false;
  result.Error = std::move(message);
  context.LastError = result.Error;
  context.AddDiagnostic(result.Error);
  return result;
}

RecipeStep MakeCustomRecipeStep(std::string name, RecipeStepExecutor execute) {
  RecipeStep step;
  step.Name = std::move(name);
  step.Execute = std::move(execute);
  return step;
}

RecipeStep MakeRewriteRulesStep(std::string name, std::vector<RecipeRule> rules,
                                RecipeRuleApplicationMode mode, bool required) {
  RecipeStep step;
  step.Name = std::move(name);
  step.Required = required;
  step.Execute = [name = step.Name, rules = std::move(rules), mode,
                  required](RecipeContext &context) {
    if (context.ProgramHandle == nullptr) {
      return MakeRecipeStepFailure(
          context, "recipe context is missing active SM5 program");
    }
    return ExecuteRewriteRules(*context.ProgramHandle, name, rules, mode,
                               required, context);
  };
  return step;
}

RecipeStep MakePrefilterStep(std::string name,
                             std::vector<RecipePrefilter> checks,
                             std::string setState,
                             RecipePrefilterMode mode) {
  RecipeStep step;
  step.Name = std::move(name);
  step.Execute = [stepName = step.Name, checks = std::move(checks),
                  setState = std::move(setState), mode](RecipeContext &context) {
    if (context.ProgramHandle == nullptr) {
      return MakeRecipeStepFailure(
          context, stepName + ": recipe context is missing active SM5 program");
    }

    const std::string &stateKey = setState.empty() ? stepName : setState;
    return ExecutePrefilterStep(*context.ProgramHandle, checks, mode, stateKey,
                                context);
  };
  return step;
}

RecipeStep MakeAddInputStep(std::string id, RecipeInputDecl decl) {
  return MakeCustomRecipeStep("add_input:" + id, [id = std::move(id), decl](
                                                     RecipeContext &context) {
    if (context.ProgramHandle == nullptr) {
      return MakeRecipeStepFailure(
          context, "add_input: recipe context is missing active SM5 program");
    }

    std::string error;
    if (!AddInputDeclaration(*context.ProgramHandle, decl, context, error)) {
      return MakeRecipeStepFailure(context, "add_input: failed to add input '" +
                                                id + "': " + error);
    }

    RecipeStepResult result;
    result.Success = true;
    result.Changed = true;
    result.ResourceBindingsChanged = true;
    const std::string handle = decl.Handle.empty() ? id : decl.Handle;
    result.SideEffects.push_back(MakeAddedBindingSideEffect(
        dxp::PatchResourceKind::Input, handle,
        ResolveBindingPoint(context.InputBindings, handle, decl.BindPoint),
        "added SM5 input binding"));
    return result;
  });
}

RecipeStep MakeAddOutputStep(std::string id, RecipeOutputDecl decl) {
  return MakeCustomRecipeStep("add_output:" + id, [id = std::move(id), decl](
                                                      RecipeContext &context) {
    if (context.ProgramHandle == nullptr) {
      return MakeRecipeStepFailure(
          context, "add_output: recipe context is missing active SM5 program");
    }

    std::string error;
    if (!AddOutputDeclaration(*context.ProgramHandle, decl, context, error)) {
      return MakeRecipeStepFailure(
          context, "add_output: failed to add output '" + id + "': " + error);
    }

    RecipeStepResult result;
    result.Success = true;
    result.Changed = true;
    result.ResourceBindingsChanged = true;
    const std::string handle = decl.Handle.empty() ? id : decl.Handle;
    result.SideEffects.push_back(MakeAddedBindingSideEffect(
        dxp::PatchResourceKind::Output, handle,
        ResolveBindingPoint(context.OutputBindings, handle, decl.BindPoint),
        "added SM5 output binding"));
    return result;
  });
}

RecipeStep MakeAddTextureStep(std::string id, RecipeTextureDecl decl) {
  return MakeCustomRecipeStep("add_texture:" + id, [id = std::move(id), decl](
                                                       RecipeContext &context) {
    if (context.ProgramHandle == nullptr) {
      return MakeRecipeStepFailure(
          context, "add_texture: recipe context is missing active SM5 program");
    }

    std::string error;
    if (!AddTextureDeclaration(*context.ProgramHandle, decl, context, error)) {
      return MakeRecipeStepFailure(
          context, "add_texture: failed to add texture '" + id + "': " + error);
    }

    RecipeStepResult result;
    result.Success = true;
    result.Changed = true;
    result.ResourceBindingsChanged = true;
    const std::string handle = decl.Handle.empty() ? id : decl.Handle;
    result.SideEffects.push_back(MakeAddedBindingSideEffect(
        dxp::PatchResourceKind::Texture, handle,
        ResolveBindingPoint(context.TextureBindings, handle, decl.BindPoint),
        "added SM5 texture binding"));
    return result;
  });
}

RecipeStep MakeAddRawResourceStep(std::string id, RecipeRawResourceDecl decl) {
  return MakeCustomRecipeStep(
      "add_raw_resource:" + id,
      [id = std::move(id), decl](RecipeContext &context) {
        if (context.ProgramHandle == nullptr) {
          return MakeRecipeStepFailure(
              context,
              "add_raw_resource: recipe context is missing active SM5 program");
        }

        std::string error;
        if (!AddRawResourceDeclaration(*context.ProgramHandle, decl, context,
                                       error)) {
          return MakeRecipeStepFailure(
              context, "add_raw_resource: failed to add raw resource '" + id +
                           "': " + error);
        }

        RecipeStepResult result;
        result.Success = true;
        result.Changed = true;
        result.ResourceBindingsChanged = true;
        const std::string handle = decl.Handle.empty() ? id : decl.Handle;
        result.SideEffects.push_back(MakeAddedBindingSideEffect(
            dxp::PatchResourceKind::RawResource, handle,
            ResolveBindingPoint(context.RawResourceBindings, handle,
                                decl.BindPoint),
            "added SM5 raw resource binding"));
        return result;
      });
}

RecipeStep MakeAddStructuredResourceStep(std::string id,
                                         RecipeStructuredResourceDecl decl) {
  return MakeCustomRecipeStep(
      "add_structured_resource:" + id,
      [id = std::move(id), decl](RecipeContext &context) {
        if (context.ProgramHandle == nullptr) {
          return MakeRecipeStepFailure(context,
                                       "add_structured_resource: recipe "
                                       "context is missing active SM5 program");
        }

        std::string error;
        if (!AddStructuredResourceDeclaration(*context.ProgramHandle, decl,
                                              context, error)) {
          return MakeRecipeStepFailure(
              context,
              "add_structured_resource: failed to add structured resource '" +
                  id + "': " + error);
        }

        RecipeStepResult result;
        result.Success = true;
        result.Changed = true;
        result.ResourceBindingsChanged = true;
        const std::string handle = decl.Handle.empty() ? id : decl.Handle;
        result.SideEffects.push_back(MakeAddedBindingSideEffect(
            dxp::PatchResourceKind::StructuredResource, handle,
            ResolveBindingPoint(context.StructuredResourceBindings, handle,
                                decl.BindPoint),
            "added SM5 structured resource binding"));
        return result;
      });
}

RecipeStep MakeAddCBufferStep(std::string id, RecipeCBufferDecl decl) {
  return MakeCustomRecipeStep("add_cbuffer:" + id, [id = std::move(id), decl](
                                                       RecipeContext &context) {
    if (context.ProgramHandle == nullptr) {
      return MakeRecipeStepFailure(
          context, "add_cbuffer: recipe context is missing active SM5 program");
    }

    std::string error;
    if (!AddCBufferDeclaration(*context.ProgramHandle, decl, context, error)) {
      return MakeRecipeStepFailure(
          context, "add_cbuffer: failed to add cbuffer '" + id + "': " + error);
    }

    RecipeStepResult result;
    result.Success = true;
    result.Changed = true;
    result.ResourceBindingsChanged = true;
    const std::string handle = decl.Handle.empty() ? id : decl.Handle;
    result.SideEffects.push_back(MakeAddedBindingSideEffect(
        dxp::PatchResourceKind::CBuffer, handle,
        ResolveBindingPoint(context.CBufferBindings, handle, decl.BindPoint),
        "added SM5 cbuffer binding"));
    return result;
  });
}

RecipeStep MakeAddSamplerStep(std::string id, RecipeSamplerDecl decl) {
  return MakeCustomRecipeStep("add_sampler:" + id, [id = std::move(id), decl](
                                                       RecipeContext &context) {
    if (context.ProgramHandle == nullptr) {
      return MakeRecipeStepFailure(
          context, "add_sampler: recipe context is missing active SM5 program");
    }

    std::string error;
    if (!AddSamplerDeclaration(*context.ProgramHandle, decl, context, error)) {
      return MakeRecipeStepFailure(
          context, "add_sampler: failed to add sampler '" + id + "': " + error);
    }

    RecipeStepResult result;
    result.Success = true;
    result.Changed = true;
    result.ResourceBindingsChanged = true;
    const std::string handle = decl.Handle.empty() ? id : decl.Handle;
    result.SideEffects.push_back(MakeAddedBindingSideEffect(
        dxp::PatchResourceKind::Sampler, handle,
        ResolveBindingPoint(context.SamplerBindings, handle, decl.BindPoint),
        "added SM5 sampler binding"));
    return result;
  });
}

RecipeStep MakeAddUavStep(std::string id, RecipeUavDecl decl) {
  return MakeCustomRecipeStep(
      "add_uav:" + id, [id = std::move(id), decl](RecipeContext &context) {
        if (context.ProgramHandle == nullptr) {
          return MakeRecipeStepFailure(
              context, "add_uav: recipe context is missing active SM5 program");
        }

        std::string error;
        if (!AddUavDeclaration(*context.ProgramHandle, decl, context, error)) {
          return MakeRecipeStepFailure(context, "add_uav: failed to add uav '" +
                                                    id + "': " + error);
        }

        RecipeStepResult result;
        result.Success = true;
        result.Changed = true;
        result.ResourceBindingsChanged = true;
        const std::string handle = decl.Handle.empty() ? id : decl.Handle;
        result.SideEffects.push_back(MakeAddedBindingSideEffect(
            dxp::PatchResourceKind::Uav, handle,
            ResolveBindingPoint(context.UavBindings, handle, decl.BindPoint),
            "added SM5 UAV binding"));
        return result;
      });
}

RecipeStep MakeVerifyProgramStep(std::string name) {
  RecipeStep step;
  step.Name = std::move(name);
  step.Execute = [](RecipeContext &context) {
    if (context.ProgramHandle == nullptr) {
      return MakeRecipeStepFailure(
          context, "recipe context is missing active SM5 program");
    }

    std::vector<uint8_t> serializedProgram;
    if (!RebuildShaderChunk(*context.ProgramHandle, serializedProgram)) {
      return MakeRecipeStepFailure(
          context, "verify_program: failed to serialize SM5 program");
    }

    RecipeStepResult result;
    result.Success = true;
    result.ModuleVerified = true;
    return result;
  };
  return step;
}

RecipePrefilter MakeShaderVersionPrefilter(uint32_t majorVersion,
                                           uint32_t minorVersion,
                                           std::string name, bool required) {
  return RecipePrefilter{}
      .Named(std::move(name))
      .Require(required)
      .CheckShaderVersion(majorVersion, minorVersion);
}

RecipePrefilter MakeOpcodeCountPrefilter(std::string opcode,
                                         int32_t expectedCount,
                                         std::string name, bool required) {
  return RecipePrefilter{}
      .Named(std::move(name))
      .Require(required)
      .CheckOpcodeCount(std::move(opcode), expectedCount);
}

RecipePrefilter MakeResourceCountPrefilter(int32_t expectedResourceCount,
                                           std::string name, bool required) {
  return RecipePrefilter{}
      .Named(std::move(name))
      .Require(required)
      .CheckResourceCount(expectedResourceCount);
}

RecipePrefilter MakePatternPrefilter(RecipeMatchPattern match, std::string name,
                                     bool required) {
  return RecipePrefilter{}
      .Named(std::move(name))
      .Require(required)
      .CheckPatternMatch(std::move(match));
}

bool ReserveTempRegisters(RecipeContext &context, uint32_t count,
                          uint32_t &baseIndex) {
  if (context.ProgramHandle == nullptr) {
    context.LastError = "recipe context is missing active SM5 program";
    context.AddDiagnostic(context.LastError);
    return false;
  }

  Program &program = *context.ProgramHandle;
  baseIndex = program.TempCount;
  if (count == 0) {
    return true;
  }

  EnsureTempDeclaration(program, program.TempCount + count);
  context.ProgramModified = true;
  context.ModuleVerified = false;
  return true;
}

static RecipeStepResult ExecuteRecipeStep(Program &program,
                                          const RecipeStep &step,
                                          RecipeContext &context) {
  if (step.Execute) {
    if (!step.Rules.empty()) {
      return MakeRecipeStepFailure(
          context,
          "SM5 recipe step '" + step.Name +
              "' cannot combine execute callback with declarative rules");
    }
    return step.Execute(context);
  }

  return ExecuteRewriteRules(program, step.Name, step.Rules,
                             step.ApplicationMode, step.Required, context);
}

bool ExecuteRecipe(Program &program, const Recipe &recipe,
                   RecipeContext &context, dxp::PatchReport *report) {
  ScopedProgramBinding boundProgram(context, program);
  context.ReservedTempBase = 0;
  context.ReservedTempCount = 0;
  context.TempBindings.clear();
  context.InputBindings.clear();
  context.OutputBindings.clear();
  context.TextureBindings.clear();
  context.RawResourceBindings.clear();
  context.StructuredResourceBindings.clear();
  context.CBufferBindings.clear();
  context.SamplerBindings.clear();
  context.UavBindings.clear();

  if (report != nullptr)
    report->Steps.clear();
  if (report != nullptr)
    report->NewBindings.clear();

  ApplyReservedTemps(program, recipe, context);

  for (const auto &step : recipe.GetSteps()) {
    if (!ShouldExecuteStep(step, context)) {
      if (report != nullptr) {
        dxp::PatchStepReport stepReport;
        stepReport.Name = step.Name;
        stepReport.Executed = false;
        stepReport.Skipped = true;
        stepReport.Success = true;
        stepReport.Required = step.Required;
        report->Steps.push_back(std::move(stepReport));
      }
      continue;
    }

    const auto result = ExecuteRecipeStep(program, step, context);
    if (report != nullptr) {
      dxp::PatchStepReport stepReport;
      stepReport.Name = step.Name;
      stepReport.Executed = true;
      stepReport.Skipped = false;
      stepReport.Success = result.Success;
      stepReport.Changed = result.Changed;
      stepReport.StopRecipe = result.StopRecipe;
      stepReport.Required = step.Required;
      stepReport.MatchCount = result.MatchCount;
      stepReport.Rules = result.RuleReports;
      stepReport.SideEffects = result.SideEffects;
      for (auto &sideEffect : stepReport.SideEffects) {
        if (sideEffect.StepName.empty())
          sideEffect.StepName = step.Name;
      }
      stepReport.Error = result.Error;
      AppendBindingExports(*report, stepReport.SideEffects);
      report->Steps.push_back(std::move(stepReport));
    }
    context.TotalRuleMatches += result.MatchCount;
    context.ProgramModified = context.ProgramModified || result.Changed;
    context.ResourceBindingsChanged =
        context.ResourceBindingsChanged || result.ResourceBindingsChanged;
    context.ResourcesRefreshed =
        context.ResourcesRefreshed || result.ResourcesRefreshed;
    context.ModuleVerified = context.ModuleVerified || result.ModuleVerified;
    if (!result.Success && step.Required)
      return false;
    if (result.StopRecipe)
      break;
  }

  return true;
}

} // namespace dxp::sm5
