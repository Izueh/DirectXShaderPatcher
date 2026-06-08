#include "dxp/sm5/Recipe.h"
#include "dxp/sm5/Patch.h"

#include "d3d11TokenizedProgramFormat.hpp"

#include "dxp/PatchReport.h"
#include "Serialize.h"
#include "Transforms.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
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
  int32_t RangeStartOffset = 0;
  int32_t RangeEndOffset = -1;
  int32_t InsertRelativeIndex = -1;
  bool RequiredMatch = false;
  RecipeRuleApplicationMode ApplicationMode = RecipeRuleApplicationMode::First;
  RecipeRuleRewriteMode RewriteMode = RecipeRuleRewriteMode::Replace;
  std::function<bool(RecipeContext &)> Predicate;
  RecipeRewriteCallback RewriteCallback;
  bool RefreshDeclarations = false;
};

static bool HasDeclarativeMatchPattern(const RecipeMatchPattern &match) {
  return !match.Opcode.empty() || !match.Capture.empty() ||
         !match.Saturate.empty() || !match.InterpolationMode.empty() ||
         match.TestBoolean >= 0 || !match.Operands.empty() ||
         !match.Sequence.empty();
}

static bool HasDeclarativeRewritePlan(const RecipeRule &rule) {
  return !rule.Emit.empty() ||
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

    if (!instruction.Capture.empty()) {
      if (!ValidateCaptureReference(captures, instruction.Capture, "instruction",
                                    captures.Instructions,
                                    "emit instruction capture", error)) {
        return false;
      }
    }

    if (instruction.CaptureFields.AnySelected() && instruction.Capture.empty()) {
      error = "SM5 emit instruction capture_fields requires capture name";
      return false;
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
  for (const auto &entry : match.CapturedOperands) {
    runtimeMatch.operands[entry.first] = *entry.second;
  }
  for (const auto &entry : match.CapturedInstructions) {
    runtimeMatch.instructions[entry.first] = *entry.second;
  }
  runtimeMatch.indexValues = match.CapturedOperandIndexValues;
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

static bool ResolveRangeReplacement(const RuntimeRule &rule,
                                    const MatchResult &match,
                                    const std::string &rewritePath,
                                    RewriteAction &action,
                                    std::string &error);

static Instruction FinalizeInstruction(Instruction instruction);

static bool BuildRewriteInstructions(const std::vector<Instruction> &templates,
                                     const MatchResult &match,
                                     const std::string &rewritePath,
                                     uint32_t baseTempCount,
                                     RecipeContext &context,
                                     std::vector<Instruction> &instructions,
                                     uint32_t &requiredTempCount,
                                     std::string &error);

static bool ValidateOperandStructure(const Operand &operand,
                                     const std::string &path,
                                     std::string &error);

static bool ValidateInstructionStructure(const Instruction &instruction,
                                         const std::string &path,
                                         std::string &error);

static bool ValidateProgramStructure(const Program &program,
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
  if (lowered == "linear_centroid") {
    mode = D3D10_SB_INTERPOLATION_LINEAR_CENTROID;
    return true;
  }
  if (lowered == "linear_noperspective") {
    mode = D3D10_SB_INTERPOLATION_LINEAR_NOPERSPECTIVE;
    return true;
  }
  if (lowered == "linear_noperspective_centroid") {
    mode = D3D10_SB_INTERPOLATION_LINEAR_NOPERSPECTIVE_CENTROID;
    return true;
  }
  if (lowered == "linear_sample") {
    mode = D3D10_SB_INTERPOLATION_LINEAR_SAMPLE;
    return true;
  }
  if (lowered == "linear_noperspective_sample") {
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
  if (lowered == "abs_neg") {
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
  if ((operandType != D3D10_SB_OPERAND_TYPE_IMMEDIATE32 &&
       operandType != D3D10_SB_OPERAND_TYPE_IMMEDIATE64) &&
      numComponents == D3D10_SB_OPERAND_4_COMPONENT) {
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

  if (!emitModel.Opcode.empty() && !emitModel.Capture.empty()) {
    error = std::string(errorPrefix) + " cannot specify both opcode and capture";
    return false;
  }

  if (emitModel.Opcode.empty() && emitModel.Capture.empty()) {
    error = std::string(errorPrefix) + " entries require opcode or capture";
    return false;
  }

  if (emitModel.Opcode.empty()) {
    instruction.Capture = emitModel.Capture;
    instruction.CaptureFields.Opcode = emitModel.CaptureFields.Opcode;
    instruction.CaptureFields.Saturate = emitModel.CaptureFields.Saturate;
    instruction.CaptureFields.TestBoolean = emitModel.CaptureFields.TestBoolean;
    instruction.CaptureFields.Operands = emitModel.CaptureFields.Operands;
    instruction.CaptureFields.Immediates = emitModel.CaptureFields.Immediates;
    return true;
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

  if (!operandModel.Capture.empty() && !operandModel.FromHandle.empty()) {
    error = "SM5 emit operand cannot use both capture and from_handle";
    return false;
  }

  if (operandModel.Capture.empty() && operandModel.CaptureFields.AnySelected()) {
    error = "SM5 capture_fields requires emit operand capture";
    return false;
  }

  const bool hasCaptureReference = !operandModel.Capture.empty();
  if (hasCaptureReference) {
    operand.CaptureName = operandModel.Capture;
    operand.CaptureType = operandModel.CaptureFields.Type;
    operand.CaptureComponents = operandModel.CaptureFields.Components;
    operand.CaptureModifier = operandModel.CaptureFields.Modifier;
    operand.CaptureIndices = operandModel.CaptureFields.Indices;
    operand.CaptureImmediates = operandModel.CaptureFields.Immediates;
  }

  if (!hasCaptureReference && operandModel.Type.empty()) {
    error = "literal SM5 emit operands require type or capture";
    return false;
  }

  if (!operandModel.FromHandle.empty() && operandModel.Type.empty()) {
    error = "SM5 from_handle emit operands require explicit operand type";
    return false;
  }

  if (!operandModel.Type.empty()) {
    if (!ParseOperandTypeToken(operandModel.Type, operand.Type, error)) {
      return false;
    }
  }

  const bool hasLiteralComponentSpec =
      !operandModel.Mask.empty() || !operandModel.Swizzle.empty() ||
      !operandModel.Select.empty() || operandModel.NumComponents >= 0;
  if (!hasCaptureReference || !operandModel.Type.empty() ||
      hasLiteralComponentSpec) {
    if (!ParseOperandComponentMode(operandModel, operand.Type,
                                   operand.NumComponents,
                                   operand.ComponentMode, error)) {
      return false;
    }
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
        indexEntry.ImmediateLoVariableName = indexPattern.ImmediateLoVariable;
        indexEntry.ImmediateHiVariableName = indexPattern.ImmediateHiVariable;
        indexEntry.ImmediateVariableFamily =
          static_cast<uint32_t>(indexPattern.ImmediateFamily);

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

  operand.FromHandle = operandModel.FromHandle;

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

  if (operandModel.MatchCapture.empty() &&
      operandModel.MatchCaptureFields.AnySelected()) {
    error = "SM5 match_capture_fields requires operand match_capture";
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
  operandMatch.MatchCaptureType = operandModel.MatchCaptureFields.Type;
  operandMatch.MatchCaptureComponents = operandModel.MatchCaptureFields.Components;
  operandMatch.MatchCaptureModifier = operandModel.MatchCaptureFields.Modifier;
  operandMatch.MatchCaptureIndices = operandModel.MatchCaptureFields.Indices;
  operandMatch.MatchCaptureImmediates = operandModel.MatchCaptureFields.Immediates;
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

  rule.RangeStartOffset = ruleModel.RangeStartOffset;
  rule.RangeEndOffset = ruleModel.RangeEndOffset;
  rule.InsertRelativeIndex = ruleModel.InsertRelativeIndex;
  rule.RequiredMatch = ruleModel.RequiredMatch;
  rule.RewriteMode = ruleModel.RewriteMode;
  rule.RewriteCallback = ruleModel.RewriteCallback;

  if (rule.RewriteCallback) {
    if (HasDeclarativeRewritePlan(ruleModel)) {
      error =
          "SM5 rules cannot combine declarative rewrite fields with rewrite callbacks";
      return false;
    }
  } else if (!IsMutatingRewriteMode(rule.RewriteMode)) {
    if (!ruleModel.Emit.empty() || rule.InsertRelativeIndex >= 0) {
      error =
          "SM5 rewrite mode None cannot be combined with emit or "
          "insert_relative_index";
      return false;
    }
  } else if (ruleModel.Emit.empty()) {
    error = "SM5 rules without emit must use match.rewrite_mode: None";
    return false;
  }

  if (rule.RewriteMode == RecipeRuleRewriteMode::Before ||
      rule.RewriteMode == RecipeRuleRewriteMode::After) {
    if (rule.InsertRelativeIndex < 0) {
      error = "SM5 before/after rewrites require match.insert_relative_index";
      return false;
    }
  } else if (rule.InsertRelativeIndex >= 0) {
    error = "SM5 match.insert_relative_index requires match.rewrite_mode: "
            "before or after";
    return false;
  }

  for (const RecipeInstructionTemplate &emitModel : ruleModel.Emit) {
    Instruction instruction;
    if (!CompileEmitInstructionTemplate(emitModel, instruction, error)) {
      return false;
    }
    rule.Emit.push_back(std::move(instruction));
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
    matches = rule.HasMatchSequence ? CollectSequenceMatches(program, rule.MatchSequence, context.captures)
                                    : CollectMatches(program, rule.Match, context.captures);
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
                                        const std::string &rewritePath,
                                        RecipeContext &context,
                                        std::vector<RewriteAction> &actions,
                                        std::string &error) {
  error.clear();
  actions.clear();
  if (!rule.RewriteCallback) {
    RewriteAction action;
    if (!ResolveRangeReplacement(rule, match, rewritePath, action, error)) {
      return false;
    }

    if (!BuildRewriteInstructions(rule.Emit, match, rewritePath,
                                  program.TempCount, context,
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
    for (const auto &entry : context.captures.operands) {
      publicMatch.CapturedOperands[entry.first] = &entry.second;
    }
    for (const auto &entry : context.captures.instructions) {
      publicMatch.CapturedInstructions[entry.first] = &entry.second;
    }
    publicMatch.CapturedOperandIndexValues = context.captures.indexValues;

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
        if (!BuildRewriteInstructions(templates, match,
                                      rewritePath + ".callback",
                                      program.TempCount, context,
                                      action.NewInstructions,
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

static bool ResolveInsertAnchorIndex(const RuntimeRule &rule,
                                     const MatchResult &match,
                                     const std::string &rewritePath,
                                     uint32_t &insertIndex,
                                     std::string &error) {
  if (rule.InsertRelativeIndex < 0) {
    error = rewritePath +
            ": before/after rewrites require non-negative insert index";
    return false;
  }

  const uint32_t windowStart = match.RangeStartIndex;
  const uint32_t windowEnd = match.RangeEndIndex;
  if (windowStart > windowEnd) {
    error = rewritePath + ": invalid SM5 match window";
    return false;
  }

  const uint32_t windowLength = windowEnd - windowStart + 1;
  const uint32_t relativeIndex = static_cast<uint32_t>(rule.InsertRelativeIndex);
  if (relativeIndex >= windowLength) {
    error = rewritePath +
            ": insert_relative_index is out of match window bounds";
    return false;
  }

  insertIndex = windowStart + relativeIndex;
  return true;
}

static bool ValidateIndexStructure(const Operand::Index &index,
                                   const std::string &path,
                                   std::string &error) {
  const bool hasRelative = static_cast<bool>(index.RelativeOperand);
  switch (index.Representation) {
  case Operand::IndexRepresentation::Immediate32:
    if (!index.HasImmediateLo || index.HasImmediateHi || hasRelative) {
      error = path +
              " expects immediate32 (requires immediate_lo, forbids "
              "immediate_hi and relative_operand)";
      return false;
    }
    break;
  case Operand::IndexRepresentation::Immediate64:
    if (!index.HasImmediateLo || !index.HasImmediateHi || hasRelative) {
      error = path +
              " expects immediate64 (requires immediate_lo/immediate_hi, "
              "forbids relative_operand)";
      return false;
    }
    break;
  case Operand::IndexRepresentation::Relative:
    if (index.HasImmediateLo || index.HasImmediateHi || !hasRelative) {
      error = path +
              " expects relative (requires relative_operand, forbids "
              "immediates)";
      return false;
    }
    break;
  case Operand::IndexRepresentation::Immediate32PlusRelative:
    if (!index.HasImmediateLo || index.HasImmediateHi || !hasRelative) {
      error = path +
              " expects immediate32_plus_relative (requires immediate_lo and "
              "relative_operand, forbids immediate_hi)";
      return false;
    }
    break;
  case Operand::IndexRepresentation::Immediate64PlusRelative:
    if (!index.HasImmediateLo || !index.HasImmediateHi || !hasRelative) {
      error = path +
              " expects immediate64_plus_relative (requires immediate_lo, "
              "immediate_hi, and relative_operand)";
      return false;
    }
    break;
  }

  if (index.RelativeOperand != nullptr) {
    if (!ValidateOperandStructure(*index.RelativeOperand,
                                  path + ".relative_operand", error)) {
      return false;
    }
  }

  return true;
}

static bool ValidateOperandStructure(const Operand &operand,
                                     const std::string &path,
                                     std::string &error) {
  if (!operand.IndexEntries.empty()) {
    std::vector<uint32_t> flattenedIndices;
    for (size_t index = 0; index < operand.IndexEntries.size(); ++index) {
      const Operand::Index &indexEntry = operand.IndexEntries[index];
      const std::string indexPath =
          path + ".indices[" + std::to_string(index) + "]";
      if (!ValidateIndexStructure(indexEntry, indexPath, error)) {
        return false;
      }
      if (indexEntry.HasImmediateLo) {
        flattenedIndices.push_back(indexEntry.ImmediateLo);
      }
      if (indexEntry.HasImmediateHi) {
        flattenedIndices.push_back(indexEntry.ImmediateHi);
      }
    }

    if (!operand.Indices.empty() && operand.Indices != flattenedIndices) {
      error = path +
              " has inconsistent flattened indices for encoded index entries";
      return false;
    }

    if ((operand.Type == D3D10_SB_OPERAND_TYPE_IMMEDIATE32 ||
         operand.Type == D3D10_SB_OPERAND_TYPE_IMMEDIATE64) &&
        !operand.ImmediateValues.empty() &&
        operand.ImmediateValues != flattenedIndices) {
      error = path + " has immediate payload mismatch against index entries";
      return false;
    }
  }

  if ((operand.Type == D3D10_SB_OPERAND_TYPE_IMMEDIATE32 ||
       operand.Type == D3D10_SB_OPERAND_TYPE_IMMEDIATE64) &&
      !operand.Indices.empty()) {
    error = path + " immediate operand cannot keep register indices";
    return false;
  }

  if (operand.RelativeOperand != nullptr) {
    if (!ValidateOperandStructure(*operand.RelativeOperand,
                                  path + ".relative_operand", error)) {
      return false;
    }
  }

  return true;
}

static bool ValidateInstructionStructure(const Instruction &instruction,
                                         const std::string &path,
                                         std::string &error) {
  if (instruction.Controls.HasTestBoolean &&
      !OpcodeUsesTestBoolean(instruction.Opcode)) {
    error = path +
            " has test_boolean controls on opcode that does not support "
            "test_boolean";
    return false;
  }

  if (instruction.Controls.HasInputInterpolationMode) {
    const auto opcode = static_cast<OpcodeType>(instruction.Opcode);
    if (opcode != D3D10_SB_OPCODE_DCL_INPUT_PS &&
        opcode != D3D10_SB_OPCODE_DCL_INPUT_PS_SIV) {
      error = path +
              " has interpolation_mode controls on non-dcl_input_ps opcode";
      return false;
    }
  }

  for (size_t operandIndex = 0; operandIndex < instruction.Operands.size();
       ++operandIndex) {
    const std::string operandPath =
        path + ".operands[" + std::to_string(operandIndex) + "]";
    if (!ValidateOperandStructure(instruction.Operands[operandIndex],
                                  operandPath, error)) {
      return false;
    }
  }
  return true;
}

static bool ValidateProgramStructure(const Program &program,
                                     std::string &error) {
  for (size_t instructionIndex = 0;
       instructionIndex < program.Instructions.size(); ++instructionIndex) {
    const std::string instructionPath =
        "program.instructions[" + std::to_string(instructionIndex) + "]";
    if (!ValidateInstructionStructure(program.Instructions[instructionIndex],
                                      instructionPath, error)) {
      return false;
    }
  }
  return true;
}

static bool ResolveReplacementRange(const RuntimeRule &rule,
                                    const MatchResult &match,
                                    const std::string &rewritePath,
                                    uint32_t &rangeStart,
                                    uint32_t &rangeEnd,
                                    std::string &error) {
  const uint32_t windowStart = match.RangeStartIndex;
  const uint32_t windowEnd = match.RangeEndIndex;
  if (windowStart > windowEnd) {
    error = rewritePath + ": invalid SM5 match window";
    return false;
  }

  if (rule.RewriteMode == RecipeRuleRewriteMode::Replace) {
    rangeStart = windowStart;
    rangeEnd = windowEnd;
    return true;
  }

  if (rule.RewriteMode != RecipeRuleRewriteMode::ReplaceRange) {
    error = rewritePath + ": unsupported replacement window mode";
    return false;
  }

  if (rule.RangeStartOffset < 0 || rule.RangeEndOffset < -1) {
    error = rewritePath + ": invalid SM5 replacement range offsets";
    return false;
  }

  const uint32_t windowLength = windowEnd - windowStart + 1;
  const uint32_t startOffset = static_cast<uint32_t>(rule.RangeStartOffset);
  const uint32_t endOffset =
      rule.RangeEndOffset < 0
          ? (windowLength - 1)
          : static_cast<uint32_t>(rule.RangeEndOffset);

  if (startOffset >= windowLength || endOffset >= windowLength) {
    error = rewritePath +
            ": SM5 replacement range offsets are out of match window bounds";
    return false;
  }
  if (startOffset > endOffset) {
    error = rewritePath +
            ": SM5 replacement range start offset must be <= end offset";
    return false;
  }

  rangeStart = windowStart + startOffset;
  rangeEnd = windowStart + endOffset;
  return true;
}

static bool ResolveRangeReplacement(const RuntimeRule &rule,
                                    const MatchResult &match,
                                    const std::string &rewritePath,
                                    RewriteAction &action, std::string &error) {
  if (rule.RewriteMode == RecipeRuleRewriteMode::Replace) {
    uint32_t rangeStart = 0;
    uint32_t rangeEnd = 0;
    if (!ResolveReplacementRange(rule, match, rewritePath, rangeStart,
                                 rangeEnd, error)) {
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
    if (!ResolveInsertAnchorIndex(rule, match, rewritePath, insertIndex,
                                  error)) {
      return false;
    }
    action.Type = RewriteActionType::InsertBefore;
    action.InsertPosition = insertIndex;
    return true;
  }

  if (rule.RewriteMode == RecipeRuleRewriteMode::After) {
    uint32_t insertIndex = 0;
    if (!ResolveInsertAnchorIndex(rule, match, rewritePath, insertIndex,
                                  error)) {
      return false;
    }
    action.Type = RewriteActionType::InsertAfter;
    action.InsertPosition = insertIndex;
    return true;
  }

  if (rule.RewriteMode == RecipeRuleRewriteMode::ReplaceRange) {
    uint32_t rangeStart = 0;
    uint32_t rangeEnd = 0;
    if (!ResolveReplacementRange(rule, match, rewritePath, rangeStart,
                                 rangeEnd, error)) {
      return false;
    }
    action.Type = RewriteActionType::ReplaceRange;
    action.ReplaceIndex = rangeStart;
    action.RangeStart = rangeStart;
    action.RangeEnd = rangeEnd;
    return true;
  }

  error = rewritePath + ": unsupported SM5 rewrite mode";
  return false;
}

template <typename TDest, typename TSource>
static TDest BitCastValue(TSource value) {
  static_assert(sizeof(TDest) == sizeof(TSource),
                "bit-cast source/destination sizes must match");
  TDest result{};
  std::memcpy(&result, &value, sizeof(result));
  return result;
}

static std::string DescribeAnyType(const std::any &value) {
  if (value.type() == typeid(bool))
    return "bool";
  if (value.type() == typeid(uint32_t))
    return "uint32_t";
  if (value.type() == typeid(int32_t))
    return "int32_t";
  if (value.type() == typeid(uint64_t))
    return "uint64_t";
  if (value.type() == typeid(int64_t))
    return "int64_t";
  if (value.type() == typeid(float))
    return "float";
  if (value.type() == typeid(double))
    return "double";
  if (value.type() == typeid(std::string))
    return "string";
  return "unsupported";
}

static bool ResolveImmediateFromVariable(const std::string &path,
                                         const std::string &familyLabel,
                                         const std::string &variableName,
                                         const RecipeContext &context,
                                         uint32_t family,
                                         uint32_t &outLo,
                                         uint32_t &outHi,
                                         bool &hasHi,
                                         std::string &error) {
  const std::any *value = context.FindVariableAny(variableName);
  if (value == nullptr) {
    error = path + ": missing variable '" + variableName + "' for " +
            familyLabel;
    return false;
  }

  auto failType = [&]() {
    error = path + ": variable '" + variableName + "' type " +
            DescribeAnyType(*value) + " is incompatible with " + familyLabel;
    return false;
  };

  hasHi = false;
  switch (family) {
  case static_cast<uint32_t>(RecipeImmediateFamily::U32): {
    if (const uint32_t *typed = std::any_cast<uint32_t>(value)) {
      outLo = *typed;
      return true;
    }
    if (const int32_t *typed = std::any_cast<int32_t>(value)) {
      outLo = BitCastValue<uint32_t>(*typed);
      return true;
    }
    if (const bool *typed = std::any_cast<bool>(value)) {
      outLo = *typed ? 1u : 0u;
      return true;
    }
    return failType();
  }
  case static_cast<uint32_t>(RecipeImmediateFamily::I32): {
    if (const int32_t *typed = std::any_cast<int32_t>(value)) {
      outLo = BitCastValue<uint32_t>(*typed);
      return true;
    }
    if (const uint32_t *typed = std::any_cast<uint32_t>(value)) {
      outLo = *typed;
      return true;
    }
    return failType();
  }
  case static_cast<uint32_t>(RecipeImmediateFamily::U64): {
    uint64_t resolved = 0;
    if (const uint64_t *typed = std::any_cast<uint64_t>(value)) {
      resolved = *typed;
    } else if (const int64_t *typed = std::any_cast<int64_t>(value)) {
      resolved = BitCastValue<uint64_t>(*typed);
    } else {
      return failType();
    }
    outLo = static_cast<uint32_t>(resolved & 0xFFFFFFFFull);
    outHi = static_cast<uint32_t>(resolved >> 32);
    hasHi = true;
    return true;
  }
  case static_cast<uint32_t>(RecipeImmediateFamily::I64): {
    uint64_t resolved = 0;
    if (const int64_t *typed = std::any_cast<int64_t>(value)) {
      resolved = BitCastValue<uint64_t>(*typed);
    } else if (const uint64_t *typed = std::any_cast<uint64_t>(value)) {
      resolved = *typed;
    } else {
      return failType();
    }
    outLo = static_cast<uint32_t>(resolved & 0xFFFFFFFFull);
    outHi = static_cast<uint32_t>(resolved >> 32);
    hasHi = true;
    return true;
  }
  case static_cast<uint32_t>(RecipeImmediateFamily::F32): {
    if (const float *typed = std::any_cast<float>(value)) {
      outLo = BitCastValue<uint32_t>(*typed);
      return true;
    }
    return failType();
  }
  case static_cast<uint32_t>(RecipeImmediateFamily::F64): {
    if (const double *typed = std::any_cast<double>(value)) {
      const uint64_t resolved = BitCastValue<uint64_t>(*typed);
      outLo = static_cast<uint32_t>(resolved & 0xFFFFFFFFull);
      outHi = static_cast<uint32_t>(resolved >> 32);
      hasHi = true;
      return true;
    }
    return failType();
  }
  default:
    error = path + ": variable-backed immediate is missing family metadata";
    return false;
  }
}

static uint32_t ExtractSwizzleUniqueComponents(const Operand &op) {
  if (op.NumComponents != D3D10_SB_OPERAND_4_COMPONENT) {
    return 0;
  }
  const uint32_t selMode =
      DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(op.ComponentMode);
  if (selMode != static_cast<uint32_t>(D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE)) {
    return 0;
  }

  uint32_t mask = 0;
  for (int comp = 0; comp < 4; ++comp) {
    const uint32_t src =
        DECODE_D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_SOURCE(op.ComponentMode, comp);
    mask |= (1u << src);
  }
  return mask;
}

static uint32_t DecodeDstMaskFromOperand(const Operand &op) {
  if (op.NumComponents != D3D10_SB_OPERAND_4_COMPONENT) {
    return 0;
  }

  const uint32_t selMode =
      DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(op.ComponentMode);

  switch (static_cast<D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE>(selMode)) {
  case D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE: {
    const uint32_t mask = DECODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(op.ComponentMode);
    // Mask bits are in [7:4]; shift down to [3:0] for uniform handling.
    return mask >> 4;
  }
  case D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE:
    return ExtractSwizzleUniqueComponents(op);
  case D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE:
    return 1u << DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECT_1(op.ComponentMode);
  default:
    return 0;
  }
}

static bool HasLiteralComponentSpec(const Operand &op) {
  if (op.NumComponents >= 0 && op.NumComponents != 4) {
    return true;
  }
  const uint32_t selMode =
      DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(op.ComponentMode);
  return (selMode != 0) || (op.ComponentMode != 0);
}

static void ConvertComponentModeForRoleChange(
    OperandRole fromRole, OperandRole toRole,
    uint32_t fromNumComponents, uint32_t fromComponentMode,
    uint32_t contextMask,
    uint32_t &toNumComponents, uint32_t &toComponentMode) {

  // Skip conversion for non-4-component operands (samplers, resources, etc.).
  if (fromNumComponents != D3D10_SB_OPERAND_4_COMPONENT) {
    return;
  }

  // Same role: no conversion needed.
  if (fromRole == toRole) {
    return;
  }

  const uint32_t fromSelectionMode =
      DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(fromComponentMode);

  if (fromRole == OperandRole::Source && toRole == OperandRole::Destination) {
    // Source (read) → Destination (write)
    // Destinations only support mask mode.
    switch (static_cast<D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE>(
        fromSelectionMode)) {
    case D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE: {
      // Already a mask: intersect with context mask.
      const uint32_t srcMask =
          DECODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(fromComponentMode);
      const uint32_t effectiveMask = (contextMask != 0)
                                         ? (srcMask & ENCODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(contextMask << 4))
                                         : srcMask;
      if (effectiveMask != 0) {
        toComponentMode =
            ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
                D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) |
            effectiveMask;
      }
      break;
    }
    case D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE: {
      // SELECT_1(X) → mask with that single bit set.
      const uint32_t selected =
          DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECT_1(fromComponentMode);
      const uint32_t effectiveBit = (contextMask != 0) && ((contextMask >> selected) & 1)
                                        ? (1u << selected)
                                        : (1u << selected);
      toComponentMode =
          ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
              D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) |
          ENCODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(effectiveBit);
      break;
    }
    case D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE: {
      // SWIZZLE (including NOSWIZZLE) → extract unique components, then
      // intersect with context mask.
      uint32_t unique = 0;
      for (int c = 0; c < 4; ++c) {
        const uint32_t src =
            DECODE_D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_SOURCE(
                fromComponentMode, c);
        unique |= (1u << src);
      }
      const uint32_t effectiveMask = (contextMask != 0)
                                         ? (unique & contextMask)
                                         : unique;
      if (effectiveMask != 0) {
        toComponentMode =
            ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
                D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) |
            ENCODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(effectiveMask << 4);
      }
      break;
    }
    default:
      // NOSWIZZLE falls through here (selection mode 0, but ComponentMode
      // is non-zero). Treat as full 4-component swizzle.
      toComponentMode =
          ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
              D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) |
          ENCODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(
              D3D10_SB_OPERAND_4_COMPONENT_MASK_ALL);
      break;
    }
  } else if (fromRole == OperandRole::Destination &&
             toRole == OperandRole::Source) {
    // Destination (write) → Source (read)
    const uint32_t fromMask =
        DECODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(fromComponentMode);

    switch (static_cast<D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE>(
        fromSelectionMode)) {
    case D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE: {
      if (fromMask != 0) {
        // Count set bits to determine single vs multi.
        const uint32_t maskBits = fromMask >> 4;
        int setCount = 0;
        for (int b = 0; b < 4; ++b) {
          if (maskBits & (1u << b)) {
            ++setCount;
          }
        }

        if (setCount == 1) {
          // Single-bit mask → SELECT_1.
          for (int b = 0; b < 4; ++b) {
            if (maskBits & (1u << b)) {
              toComponentMode =
                  ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
                      D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE) |
                  ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECT_1(
                      static_cast<D3D10_SB_4_COMPONENT_NAME>(b));
              break;
            }
          }
        } else {
          // Multi-bit mask → SWIZZLE with replicated components.
          uint32_t swizzle = 0;
          int bit = 0;
          for (int comp = 0; comp < 4; ++comp) {
            if (maskBits & (1u << comp)) {
              swizzle |= (static_cast<uint32_t>(comp) << (bit * 2));
              ++bit;
            }
          }
          toComponentMode =
              ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
                  D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE) |
              (swizzle << 4);
        }
      }
      break;
    }
    case D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE:
    case D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE:
      // SWIZZLE and SELECT_1 are valid for source operands: keep as-is.
      break;
    default:
      // NOSWIZZLE: keep as-is (valid for source).
      break;
    }
  }
}

static bool InstantiateOperand(const Operand &operandTemplate,
                               const MatchResult &match,
                               const std::string &path,
                               RecipeContext &context, Operand &operand,
                               std::string &error,
                               size_t emitOperandIndex = 0) {
  if (!operandTemplate.CaptureName.empty()) {
    const auto capIt = context.captures.operands.find(operandTemplate.CaptureName);
    if (capIt == context.captures.operands.end()) {
      error = path + ": missing captured operand '" +
              operandTemplate.CaptureName + "'";
      return false;
    }
    const Operand &capturedOperand = capIt->second;
    const OperandRole capturedRole = capturedOperand.Role;

    if (operandTemplate.HasCaptureFieldProjection()) {
      const std::vector<uint32_t> literalIndices = operandTemplate.Indices;
      const std::vector<uint32_t> literalImmediates =
          operandTemplate.ImmediateValues;

      operand = operandTemplate;
      if (operandTemplate.CaptureType) {
        operand.Type = capturedOperand.Type;
      }
      if (operandTemplate.CaptureComponents) {
        operand.NumComponents = capturedOperand.NumComponents;
        operand.ComponentMode = capturedOperand.ComponentMode;

        // Apply role-based component mode conversion with context-aware
        // heuristics: emit dst mask > match dst mask > source swizzle.
        if (match.Instruction != nullptr) {
          // Use the stored role from capture time.
          // Emit operand 0 is destination; operands 1+ are sources.
          const OperandRole emitRole =
              (emitOperandIndex == 0) ? OperandRole::Destination
                                      : OperandRole::Source;

          // Compute context mask by priority.
          uint32_t contextMask = 0;

          // Priority 1: Emit operand template's literal component spec.
          if (HasLiteralComponentSpec(operandTemplate)) {
            contextMask = DecodeDstMaskFromOperand(operandTemplate);
          }

          // Priority 2: Matched instruction's destination mask.
          if (contextMask == 0 && match.Instruction->Operands.size() > 0) {
            contextMask = DecodeDstMaskFromOperand(
                match.Instruction->Operands[0]);
          }

          // Priority 3: Source swizzle's unique components.
          if (contextMask == 0) {
            const uint32_t selMode =
                DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
                    capturedOperand.ComponentMode);
            if (selMode == static_cast<uint32_t>(
                D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE)) {
              contextMask = ExtractSwizzleUniqueComponents(capturedOperand);
            }
          }

          ConvertComponentModeForRoleChange(
              capturedRole, emitRole,
              capturedOperand.NumComponents,
              capturedOperand.ComponentMode,
              contextMask,
              operand.NumComponents, operand.ComponentMode);
        }
      }
      if (operandTemplate.CaptureModifier) {
        operand.Modifier = capturedOperand.Modifier;
      }
      if (operandTemplate.CaptureIndices) {
        operand.IndexEntries = capturedOperand.IndexEntries;
        operand.Indices = capturedOperand.Indices;
        operand.RelativeOperand = capturedOperand.RelativeOperand;
      }
      if (operandTemplate.CaptureImmediates) {
        operand.ImmediateValues = capturedOperand.ImmediateValues;
      }

      // Literal indices/immediates override replayed values when provided.
      if (!literalIndices.empty()) {
        operand.Indices = literalIndices;
      }
      if (!literalImmediates.empty()) {
        operand.ImmediateValues = literalImmediates;
      }
    } else {
      operand = capturedOperand;
      return true;
    }
  }

  if (operandTemplate.CaptureName.empty()) {
    operand = operandTemplate;
  }
  if (!operand.FromHandle.empty()) {
    const uint32_t *resolvedBindPoint = nullptr;
    if (operand.Type == D3D10_SB_OPERAND_TYPE_TEMP) {
      const auto it = context.TempBindings.find(operand.FromHandle);
      if (it != context.TempBindings.end()) {
        resolvedBindPoint = &it->second;
      }
    } else if (operand.Type == D3D10_SB_OPERAND_TYPE_INPUT) {
      const auto it = context.InputBindings.find(operand.FromHandle);
      if (it != context.InputBindings.end()) {
        resolvedBindPoint = &it->second;
      }
    } else if (operand.Type == D3D10_SB_OPERAND_TYPE_OUTPUT) {
      const auto it = context.OutputBindings.find(operand.FromHandle);
      if (it != context.OutputBindings.end()) {
        resolvedBindPoint = &it->second;
      }
    } else if (operand.Type == D3D10_SB_OPERAND_TYPE_RESOURCE) {
      const auto it = context.TextureBindings.find(operand.FromHandle);
      if (it != context.TextureBindings.end()) {
        resolvedBindPoint = &it->second;
      }
      if (resolvedBindPoint == nullptr) {
        const auto rawIt = context.RawResourceBindings.find(operand.FromHandle);
        if (rawIt != context.RawResourceBindings.end()) {
          resolvedBindPoint = &rawIt->second;
        }
      }
      if (resolvedBindPoint == nullptr) {
        const auto structuredIt =
            context.StructuredResourceBindings.find(operand.FromHandle);
        if (structuredIt != context.StructuredResourceBindings.end()) {
          resolvedBindPoint = &structuredIt->second;
        }
      }
    } else if (operand.Type == D3D10_SB_OPERAND_TYPE_SAMPLER) {
      const auto it = context.SamplerBindings.find(operand.FromHandle);
      if (it != context.SamplerBindings.end()) {
        resolvedBindPoint = &it->second;
      }
    } else if (operand.Type == D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER) {
      const auto it = context.CBufferBindings.find(operand.FromHandle);
      if (it != context.CBufferBindings.end()) {
        resolvedBindPoint = &it->second;
      }
    } else if (operand.Type == D3D11_SB_OPERAND_TYPE_UNORDERED_ACCESS_VIEW) {
      const auto it = context.UavBindings.find(operand.FromHandle);
      if (it != context.UavBindings.end()) {
        resolvedBindPoint = &it->second;
      }
    } else {
      error =
          path +
          ": SM5 from_handle operand type is unsupported for resource "
          "binding";
      return false;
    }

    if (resolvedBindPoint == nullptr) {
      error = path + ": missing SM5 declaration handle binding '" +
              operand.FromHandle + "'";
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
      const size_t indexPosition =
          &indexEntry - operand.IndexEntries.data();

      if (!indexEntry.MatchCaptureName.empty()) {
        const uint32_t *capturedOperandIndex = nullptr;
        const auto idxIt = context.captures.indexValues.find(indexEntry.MatchCaptureName);
        if (idxIt != context.captures.indexValues.end()) {
          capturedOperandIndex = &idxIt->second;
        }
        if (capturedOperandIndex == nullptr) {
          error = path + ".indices[" + std::to_string(indexPosition) +
                  "]: missing captured operand index '" +
                  indexEntry.MatchCaptureName + "'";
          return false;
        }
        indexEntry.HasImmediateLo = true;
        indexEntry.ImmediateLo = *capturedOperandIndex;
      }

      if (indexEntry.HasImmediateLo && immediateCursor < operand.Indices.size()) {
        const bool hasDynamicImmediateLo =
            !indexEntry.MatchCaptureName.empty() ||
            !indexEntry.ImmediateLoVariableName.empty();
        if (!hasDynamicImmediateLo) {
          indexEntry.ImmediateLo = operand.Indices[immediateCursor];
        }
        ++immediateCursor;
      }
      if (indexEntry.HasImmediateHi && immediateCursor < operand.Indices.size()) {
        const bool hasDynamicImmediateHi =
            !indexEntry.ImmediateHiVariableName.empty() ||
            (!indexEntry.ImmediateLoVariableName.empty() &&
             (indexEntry.ImmediateVariableFamily ==
                  static_cast<uint32_t>(RecipeImmediateFamily::U64) ||
              indexEntry.ImmediateVariableFamily ==
                  static_cast<uint32_t>(RecipeImmediateFamily::I64) ||
              indexEntry.ImmediateVariableFamily ==
                  static_cast<uint32_t>(RecipeImmediateFamily::F64)));
        if (!hasDynamicImmediateHi) {
          indexEntry.ImmediateHi = operand.Indices[immediateCursor];
        }
        ++immediateCursor;
      }

      if (!indexEntry.ImmediateLoVariableName.empty() ||
          !indexEntry.ImmediateHiVariableName.empty()) {
        const std::string variableName =
            !indexEntry.ImmediateLoVariableName.empty()
                ? indexEntry.ImmediateLoVariableName
                : indexEntry.ImmediateHiVariableName;
        uint32_t resolvedLo = 0;
        uint32_t resolvedHi = 0;
        bool resolvedHasHi = false;
        const std::string familyLabel =
            (indexEntry.ImmediateVariableFamily ==
             static_cast<uint32_t>(RecipeImmediateFamily::U32))
                ? "immediates_u32"
                : (indexEntry.ImmediateVariableFamily ==
                   static_cast<uint32_t>(RecipeImmediateFamily::U64))
                      ? "immediates_u64"
                      : (indexEntry.ImmediateVariableFamily ==
                         static_cast<uint32_t>(RecipeImmediateFamily::I32))
                            ? "immediates_i32"
                            : (indexEntry.ImmediateVariableFamily ==
                               static_cast<uint32_t>(RecipeImmediateFamily::I64))
                                  ? "immediates_i64"
                                  : (indexEntry.ImmediateVariableFamily ==
                                     static_cast<uint32_t>(
                                         RecipeImmediateFamily::F32))
                                        ? "immediates_f32"
                                        : "immediates_f64";
        if (!ResolveImmediateFromVariable(
                path + ".indices[" + std::to_string(indexPosition) + "]",
                familyLabel, variableName, context,
                indexEntry.ImmediateVariableFamily, resolvedLo, resolvedHi,
                resolvedHasHi, error)) {
          return false;
        }

        if (!indexEntry.ImmediateLoVariableName.empty()) {
          indexEntry.HasImmediateLo = true;
          indexEntry.ImmediateLo = resolvedLo;
        }
        if (!indexEntry.ImmediateHiVariableName.empty() || resolvedHasHi) {
          indexEntry.HasImmediateHi = true;
          indexEntry.ImmediateHi = resolvedHi;
        }
      }

      if (indexEntry.RelativeOperand) {
        Operand instantiatedRelative;
        if (!InstantiateOperand(*indexEntry.RelativeOperand, match,
                    path + ".indices[" +
                      std::to_string(indexPosition) +
                      "].relative_operand",
                    context, instantiatedRelative, error)) {
          return false;
        }
        indexEntry.RelativeOperand =
            std::make_shared<Operand>(std::move(instantiatedRelative));
      }
    }

    // Binding resolution can append extra binding indices (for example CB range index).
    // Preserve those values in IndexEntries so serialization keeps operands intact.
    while (immediateCursor < operand.Indices.size()) {
      Operand::Index appendedIndex;
      appendedIndex.Representation = Operand::IndexRepresentation::Immediate32;
      appendedIndex.HasImmediateLo = true;
      appendedIndex.ImmediateLo = operand.Indices[immediateCursor++];
      operand.IndexEntries.push_back(std::move(appendedIndex));
    }

    operand.Indices.clear();
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
  }

  if (operand.RelativeOperand) {
    Operand instantiatedRelative;
    if (!InstantiateOperand(*operand.RelativeOperand, match,
                            path + ".relative_operand", context,
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
                                   const std::string &instructionPath,
                                   RecipeContext &context,
                                   Instruction &instruction,
                                   std::string &error) {
  if (!instructionTemplate.Capture.empty()) {
    const auto it = context.captures.instructions.find(instructionTemplate.Capture);
    if (it == context.captures.instructions.end()) {
      error = std::string(instructionPath) + ": unknown captured instruction '" +
              instructionTemplate.Capture + "'";
      return false;
    }

    const Instruction &captured = it->second;
    const auto &fields = instructionTemplate.CaptureFields;

    if (fields.AnySelected()) {
      instruction = Instruction{};
      if (fields.Opcode) {
        instruction.Opcode = captured.Opcode;
      }
      if (fields.Saturate) {
        instruction.Controls.Saturate = captured.Controls.Saturate;
      }
      if (fields.TestBoolean) {
        instruction.Controls.HasTestBoolean = captured.Controls.HasTestBoolean;
        instruction.Controls.TestBoolean = captured.Controls.TestBoolean;
      }
      if (fields.Operands) {
        instruction.Operands = captured.Operands;
      }
      if (fields.Immediates) {
        instruction.CustomData = captured.CustomData;
      }
    } else {
      instruction = captured;
    }

    if (!ValidateInstructionStructure(instruction, instructionPath, error)) {
      return false;
    }

    instruction = FinalizeInstruction(std::move(instruction));
    return true;
  }

  instruction = instructionTemplate;
  instruction.Operands.clear();
  instruction.RawTokens.clear();
  instruction.LengthInDwords = 0;

  for (size_t operandIndex = 0; operandIndex < instructionTemplate.Operands.size();
       ++operandIndex) {
    Operand operand;
    if (!InstantiateOperand(instructionTemplate.Operands[operandIndex], match,
                            instructionPath + ".operands[" +
                                std::to_string(operandIndex) + "]",
                            context, operand, error,
                            operandIndex)) {
      return false;
    }
    instruction.Operands.push_back(std::move(operand));
  }

  if (!ValidateInstructionStructure(instruction, instructionPath, error)) {
    return false;
  }

  instruction = FinalizeInstruction(std::move(instruction));
  return true;
}

static bool BuildRewriteInstructions(const std::vector<Instruction> &templates,
                                     const MatchResult &match,
                                     const std::string &rewritePath,
                                     uint32_t baseTempCount,
                                     RecipeContext &context,
                                     std::vector<Instruction> &instructions,
                                     uint32_t &requiredTempCount,
                                     std::string &error) {
  (void)baseTempCount;
  instructions.clear();
  instructions.reserve(templates.size());
  for (size_t instructionIndex = 0; instructionIndex < templates.size();
       ++instructionIndex) {
    const Instruction &instructionTemplate = templates[instructionIndex];
    Instruction instruction;
    const std::string instructionPath =
        rewritePath + ".emit[" + std::to_string(instructionIndex) + "]";
    if (!InstantiateInstruction(instructionTemplate, match, instructionPath,
                                context, instruction, error)) {
      return false;
    }
    instructions.push_back(std::move(instruction));
  }
  requiredTempCount = baseTempCount;
  return true;
}

static bool MatchesOpcodeCount(uint32_t count, int32_t expectedCount) {
  if (expectedCount < 0) {
    return count <= static_cast<uint32_t>(-expectedCount);
  }
  if (expectedCount == 0) {
    return count == 0;
  }
  return count >= static_cast<uint32_t>(expectedCount);
}

static uint32_t CountOpcodeMatches(const Program &program, Opcode opcode) {
  uint32_t count = 0;
  for (const auto &instruction : program.Instructions) {
    if (instruction.Opcode == opcode) {
      ++count;
    }
  }
  return count;
}

static bool TryParseBoolLiteral(const std::string &text, bool &value) {
  if (_stricmp(text.c_str(), "true") == 0) {
    value = true;
    return true;
  }
  if (_stricmp(text.c_str(), "false") == 0) {
    value = false;
    return true;
  }
  return false;
}

static bool TryParseSignedLiteral(const std::string &text, int64_t &value) {
  char *end = nullptr;
  const long long parsed = std::strtoll(text.c_str(), &end, 0);
  if (end == nullptr || *end != '\0') {
    return false;
  }
  value = static_cast<int64_t>(parsed);
  return true;
}

static bool TryParseUnsignedLiteral(const std::string &text, uint64_t &value) {
  char *end = nullptr;
  const unsigned long long parsed = std::strtoull(text.c_str(), &end, 0);
  if (end == nullptr || *end != '\0') {
    return false;
  }
  value = static_cast<uint64_t>(parsed);
  return true;
}

static bool TryParseFloatLiteral(const std::string &text, double &value) {
  char *end = nullptr;
  const double parsed = std::strtod(text.c_str(), &end);
  if (end == nullptr || *end != '\0') {
    return false;
  }
  value = parsed;
  return true;
}

template <typename TValue>
static bool ApplyComparison(RecipeConditionCompareOp op, const TValue &lhs,
                            const TValue &rhs) {
  switch (op) {
  case RecipeConditionCompareOp::Eq:
    return lhs == rhs;
  case RecipeConditionCompareOp::Ne:
    return lhs != rhs;
  case RecipeConditionCompareOp::Gt:
    return lhs > rhs;
  case RecipeConditionCompareOp::Gte:
    return lhs >= rhs;
  case RecipeConditionCompareOp::Lt:
    return lhs < rhs;
  case RecipeConditionCompareOp::Lte:
    return lhs <= rhs;
  case RecipeConditionCompareOp::None:
    return false;
  }

  return false;
}

static bool EvaluateComparison(const RecipeStepCondition &condition,
                               const RecipeContext &context) {
  if (condition.CompareOp == RecipeConditionCompareOp::None) {
    return false;
  }

  const std::any *lhsValue = nullptr;
  if (!condition.Compare.State.empty()) {
    const auto stateIt = context.State.find(condition.Compare.State);
    if (stateIt != context.State.end()) {
      lhsValue = &stateIt->second;
    }
  } else if (!condition.Compare.Input.empty()) {
    lhsValue = context.FindVariableAny(condition.Compare.Input);
  }

  if (lhsValue == nullptr) {
    return false;
  }

  const std::string &rhsText = condition.Compare.Value;
  if (const bool *lhs = std::any_cast<bool>(lhsValue)) {
    bool rhs = false;
    return TryParseBoolLiteral(rhsText, rhs) &&
           ApplyComparison(condition.CompareOp, *lhs, rhs);
  }
  if (const uint32_t *lhs = std::any_cast<uint32_t>(lhsValue)) {
    uint64_t rhs = 0;
    return TryParseUnsignedLiteral(rhsText, rhs) &&
           ApplyComparison(condition.CompareOp, *lhs,
                           static_cast<uint32_t>(rhs));
  }
  if (const int32_t *lhs = std::any_cast<int32_t>(lhsValue)) {
    int64_t rhs = 0;
    return TryParseSignedLiteral(rhsText, rhs) &&
           ApplyComparison(condition.CompareOp, *lhs,
                           static_cast<int32_t>(rhs));
  }
  if (const uint64_t *lhs = std::any_cast<uint64_t>(lhsValue)) {
    uint64_t rhs = 0;
    return TryParseUnsignedLiteral(rhsText, rhs) &&
           ApplyComparison(condition.CompareOp, *lhs, rhs);
  }
  if (const int64_t *lhs = std::any_cast<int64_t>(lhsValue)) {
    int64_t rhs = 0;
    return TryParseSignedLiteral(rhsText, rhs) &&
           ApplyComparison(condition.CompareOp, *lhs, rhs);
  }
  if (const float *lhs = std::any_cast<float>(lhsValue)) {
    double rhs = 0.0;
    return TryParseFloatLiteral(rhsText, rhs) &&
           ApplyComparison(condition.CompareOp, *lhs,
                           static_cast<float>(rhs));
  }
  if (const double *lhs = std::any_cast<double>(lhsValue)) {
    double rhs = 0.0;
    return TryParseFloatLiteral(rhsText, rhs) &&
           ApplyComparison(condition.CompareOp, *lhs, rhs);
  }
  if (const std::string *lhs = std::any_cast<std::string>(lhsValue)) {
    return ApplyComparison(condition.CompareOp, *lhs, rhsText);
  }

  return false;
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
  } else if (!condition.Input.empty()) {
    const std::any *inputValue = context.FindVariableAny(condition.Input);
    if (inputValue != nullptr) {
      if (const bool *boolValue = std::any_cast<bool>(inputValue)) {
        value = *boolValue;
      } else if (const uint32_t *u32Value = std::any_cast<uint32_t>(inputValue)) {
        value = *u32Value != 0;
      } else if (const int32_t *i32Value = std::any_cast<int32_t>(inputValue)) {
        value = *i32Value != 0;
      } else if (const uint64_t *u64Value = std::any_cast<uint64_t>(inputValue)) {
        value = *u64Value != 0;
      } else if (const int64_t *i64Value = std::any_cast<int64_t>(inputValue)) {
        value = *i64Value != 0;
      } else if (const std::string *stringValue =
                     std::any_cast<std::string>(inputValue)) {
        value = !stringValue->empty();
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
  } else if (condition.CompareOp != RecipeConditionCompareOp::None) {
    value = EvaluateComparison(condition, context);
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
    ruleReport.Name = ruleModel.Name.empty()
                ? (stepName + ".rule" + std::to_string(ruleIndex))
                : ruleModel.Name;

    std::vector<MatchResult> matches;
    std::string matchError;
    if (!EvaluateRuleMatchCallback(rule, stepName, required, program, context,
                     matches, matchError)) {
      return MakeRecipeStepFailure(context, std::move(matchError));
    }
    const bool matchedRule = !matches.empty();
    if (!ruleModel.Name.empty()) {
      context.SetState<bool>(ruleModel.Name, matchedRule);
    }
    result.MatchCount += static_cast<uint32_t>(matches.size());
    ruleReport.MatchCount = static_cast<uint32_t>(matches.size());
    if (matches.empty()) {
      result.RuleReports.push_back(std::move(ruleReport));
      if (required || rule.RequiredMatch)
        return MakeRecipeStepFailure(
            context, "step[" + stepName + "].rule[" +
                         std::to_string(ruleIndex) +
                         "]: required recipe step had no matches");
      continue;
    }

    const auto selectedMatches =
        SelectMatchIndices(matches, rule.ApplicationMode);

    if (!rule.RewriteCallback && !IsMutatingRewriteMode(rule.RewriteMode)) {
      // Even for non-mutating rules, move captures into context for
      // cross-step reuse by subsequent rules.
      if (!selectedMatches.empty()) {
        const auto &lastMatch = matches[selectedMatches.back()];
        context.captures.operands.insert(lastMatch.operands.begin(), lastMatch.operands.end());
        context.captures.instructions.insert(lastMatch.instructions.begin(), lastMatch.instructions.end());
        context.captures.indexValues.insert(lastMatch.indexValues.begin(), lastMatch.indexValues.end());
      }
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

      // Merge per-match captures into global context for BuildRewriteInstructions.
      // Use insert (merge) to preserve captures from previous steps.
      context.captures.operands.insert(match.operands.begin(), match.operands.end());
      context.captures.instructions.insert(match.instructions.begin(), match.instructions.end());
      context.captures.indexValues.insert(match.indexValues.begin(), match.indexValues.end());

      std::vector<RewriteAction> localActions;
      std::string error;
      const std::string rewritePath =
          "step[" + stepName + "].rule[" + std::to_string(ruleIndex) +
          "].match[" + std::to_string(selectedIndex) + "]";
      if (!EvaluateRuleRewriteCallback(rule, stepName, required, program, match,
                                       rewritePath, context, localActions,
                                       error)) {
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

    // Actions are sorted ascending internally by ApplyRewriteActions
    // (single-pass forward rebuild), so no pre-sorting is needed.

    if (actions.empty()) {
      result.RuleReports.push_back(std::move(ruleReport));
      continue;
    }

    bool declarationsAffected = ruleModel.RefreshDeclarations;

    if (!ApplyRewriteActions(program, actions))
      return MakeRecipeStepFailure(
          context, "step[" + stepName + "].rule[" +
                       std::to_string(ruleIndex) +
                       "]: failed to apply rewrite action");
    EnsureTempDeclaration(program, requiredTempCount);

    if (declarationsAffected) {
      RefreshDeclarations(program);
    }

    std::string validationError;
    if (!ValidateProgramStructure(program, validationError)) {
      return MakeRecipeStepFailure(
          context, "SM5 runtime structural validation failed: " +
                       validationError);
    }

    result.Changed = true;
    result.ResourceBindingsChanged = declarationsAffected;
    result.ResourcesRefreshed = declarationsAffected;
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
                                RecipeRuleApplicationMode mode,
                                bool abortOnFailure) {
  RecipeStep step;
  step.Name = std::move(name);
  step.AbortOnFailure = abortOnFailure;
  step.Execute = [name = step.Name, rules = std::move(rules), mode,
                  abortOnFailure](RecipeContext &context) {
    if (context.ProgramHandle == nullptr) {
      return MakeRecipeStepFailure(
          context, "recipe context is missing active SM5 program");
    }
    return ExecuteRewriteRules(*context.ProgramHandle, name, rules, mode,
                               abortOnFailure, context);
  };
  return step;
}

RecipeStep MakeCheckShaderVersionStep(std::string name, uint32_t majorVersion,
                                      uint32_t minorVersion,
                                      bool abortOnFailure) {
  RecipeStep step;
  step.Name = std::move(name);
  step.AbortOnFailure = abortOnFailure;
  step.Execute = [stepName = step.Name, majorVersion,
                  minorVersion](RecipeContext &context) {
    if (context.ProgramHandle == nullptr) {
      return MakeRecipeStepFailure(
          context, stepName + ": recipe context is missing active SM5 program");
    }

    const Program &program = *context.ProgramHandle;
    const bool matched = program.MajorVersion == majorVersion &&
                         program.MinorVersion == minorVersion;
    context.SetState<bool>(stepName, matched);
    if (matched) {
      return MakeRecipeStepSuccess(false, 1);
    }

    return MakeRecipeStepFailure(
        context, stepName + ": expected shader version " +
                     std::to_string(majorVersion) + "_" +
                     std::to_string(minorVersion) + ", found " +
                     std::to_string(program.MajorVersion) + "_" +
                     std::to_string(program.MinorVersion));
  };
  return step;
}

RecipeStep MakeCheckOpcodeCountStep(std::string name, std::string opcode,
                                    int32_t expectedCount,
                                    bool abortOnFailure) {
  RecipeStep step;
  step.Name = std::move(name);
  step.AbortOnFailure = abortOnFailure;
  step.Execute = [stepName = step.Name, opcode = std::move(opcode),
                  expectedCount](RecipeContext &context) {
    if (context.ProgramHandle == nullptr) {
      return MakeRecipeStepFailure(
          context, stepName + ": recipe context is missing active SM5 program");
    }

    Opcode parsedOpcode;
    if (!ParseOpcode(opcode, parsedOpcode)) {
      context.SetState<bool>(stepName, false);
      return MakeRecipeStepFailure(context,
                                   "Unknown SM5 opcode in step '" + stepName +
                                       "': " + opcode);
    }

    const uint32_t count =
        CountOpcodeMatches(*context.ProgramHandle, parsedOpcode);
    const bool matched = MatchesOpcodeCount(count, expectedCount);
    context.SetState<bool>(stepName, matched);
    if (matched) {
      return MakeRecipeStepSuccess(false, count > 0 ? 1u : 0u);
    }

    return MakeRecipeStepFailure(
        context, stepName + ": opcode '" + opcode + "' count was " +
                     std::to_string(count) +
                     " and did not satisfy expected_count " +
                     std::to_string(expectedCount));
  };
  return step;
}

RecipeStep MakeCheckResourceCountStep(std::string name,
                                      int32_t expectedResourceCount,
                                      bool abortOnFailure) {
  RecipeStep step;
  step.Name = std::move(name);
  step.AbortOnFailure = abortOnFailure;
  step.Execute = [stepName = step.Name,
                  expectedResourceCount](RecipeContext &context) {
    if (context.ProgramHandle == nullptr) {
      return MakeRecipeStepFailure(
          context, stepName + ": recipe context is missing active SM5 program");
    }

    const int32_t resourceCount =
        static_cast<int32_t>(context.ProgramHandle->Resources.size());
    const bool matched = resourceCount >= expectedResourceCount;
    context.SetState<bool>(stepName, matched);
    if (matched) {
      return MakeRecipeStepSuccess(false, resourceCount > 0 ? 1u : 0u);
    }

    return MakeRecipeStepFailure(
        context, stepName + ": resource count was " +
                     std::to_string(resourceCount) +
                     " and did not satisfy expected_resources " +
                     std::to_string(expectedResourceCount));
  };
  return step;
}

static bool ReserveTempRegisters(RecipeContext &context, uint32_t count,
                                 uint32_t &baseIndex);

RecipeStep MakeAddTempStep(std::string id, RecipeTempDecl decl) {
  return MakeCustomRecipeStep("add_temp:" + id, [id = std::move(id), decl](
                                                   RecipeContext &context) {
    if (context.ProgramHandle == nullptr) {
      return MakeRecipeStepFailure(
          context, "add_temp: recipe context is missing active SM5 program");
    }

    const std::string handle = decl.Handle.empty() ? id : decl.Handle;
    if (handle.empty()) {
      return MakeRecipeStepFailure(
          context, "add_temp: handle is required");
    }

    if (context.TempBindings.find(handle) != context.TempBindings.end()) {
      return MakeRecipeStepFailure(
          context, "add_temp: duplicate temp handle '" + handle + "'");
    }

    uint32_t baseIndex = 0;
    if (!ReserveTempRegisters(context, 1, baseIndex)) {
      return MakeRecipeStepFailure(
          context, "add_temp: failed to reserve temp for '" + handle + "': " +
                       context.LastError);
    }

    context.TempBindings.emplace(handle, baseIndex);

    RecipeStepResult result;
    result.Success = true;
    result.Changed = true;
    return result;
  });
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

static bool ReserveTempRegisters(RecipeContext &context, uint32_t count,
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
                             step.ApplicationMode, step.AbortOnFailure,
                             context);
}

bool ExecuteRecipe(Program &program, const Recipe &recipe,
           RecipeContext &context, dxp::PatchReport *report,
           const std::function<void(const std::string &, RecipeContext &)>
             *beforeStep,
           const std::function<void(const std::string &,
                      const RecipeStepResult &,
                      RecipeContext &)> *afterStep) {
  ScopedProgramBinding boundProgram(context, program);
  context.captures.clear();
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

  for (const auto &input : context.Inputs) {
    if (!context.HasVariable(input.first)) {
      context.Variables[input.first] = input.second;
    }
  }
  context.HasInitialVariablesSnapshot = false;
  context.SnapshotInitialVariables();

  if (report != nullptr)
    report->Steps.clear();
  if (report != nullptr)
    report->NewBindings.clear();

  context.ReservedTempBase = program.TempCount;
  context.ReservedTempCount = 0;

  for (const auto &step : recipe.GetSteps()) {
    if (beforeStep != nullptr && *beforeStep) {
      (*beforeStep)(step.Name, context);
    }

    if (!ShouldExecuteStep(step, context)) {
      if (report != nullptr) {
        dxp::PatchStepReport stepReport;
        stepReport.Name = step.Name;
        stepReport.Executed = false;
        stepReport.Skipped = true;
        stepReport.Success = true;
        stepReport.Required = step.AbortOnFailure;
        report->Steps.push_back(std::move(stepReport));
      }
      context.SetState<bool>(step.Name, false);
      if (afterStep != nullptr && *afterStep) {
        RecipeStepResult skipped;
        skipped.Success = true;
        (*afterStep)(step.Name, skipped, context);
      }
      continue;
    }

    auto result = ExecuteRecipeStep(program, step, context);
    if (context.State.find(step.Name) == context.State.end()) {
      context.SetState<bool>(step.Name, result.Success);
    }
    if (afterStep != nullptr && *afterStep) {
      (*afterStep)(step.Name, result, context);
    }
    if (report != nullptr) {
      dxp::PatchStepReport stepReport;
      stepReport.Name = step.Name;
      stepReport.Executed = true;
      stepReport.Skipped = false;
      stepReport.Success = result.Success;
      stepReport.Changed = result.Changed;
      stepReport.StopRecipe = result.StopRecipe;
      stepReport.Required = step.AbortOnFailure;
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
    if (!result.Success && step.AbortOnFailure)
      return false;
    if (result.StopRecipe)
      break;
  }

  return true;
}

/// Public API: execute a pre-compiled recipe against a parsed program.
RecipeStepResult ExecuteRecipe(Program &program, const Recipe &recipe,
                               RecipeContext &context) {
  ScopedProgramBinding boundProgram(context, program);
  context.captures.clear();
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

  for (const auto &input : context.Inputs) {
    if (!context.HasVariable(input.first)) {
      context.Variables[input.first] = input.second;
    }
  }
  context.HasInitialVariablesSnapshot = false;
  context.SnapshotInitialVariables();

  context.ReservedTempBase = program.TempCount;
  context.ReservedTempCount = 0;

  RecipeStepResult overallResult;
  overallResult.Success = true;

  for (const auto &step : recipe.GetSteps()) {
    if (!ShouldExecuteStep(step, context)) {
      context.SetState<bool>(step.Name, false);
      continue;
    }

    auto result = ExecuteRecipeStep(program, step, context);
    if (context.State.find(step.Name) == context.State.end()) {
      context.SetState<bool>(step.Name, result.Success);
    }
    context.TotalRuleMatches += result.MatchCount;
    context.ProgramModified = context.ProgramModified || result.Changed;
    context.ResourceBindingsChanged =
        context.ResourceBindingsChanged || result.ResourceBindingsChanged;
    context.ResourcesRefreshed =
        context.ResourcesRefreshed || result.ResourcesRefreshed;
    context.ModuleVerified = context.ModuleVerified || result.ModuleVerified;
    if (!result.Success && step.AbortOnFailure) {
      overallResult.Success = false;
      overallResult.Error = result.Error;
      context.LastError = result.Error;
      break;
    }
    if (result.StopRecipe)
      break;
  }

  return overallResult;
}

} // namespace dxp::sm5
