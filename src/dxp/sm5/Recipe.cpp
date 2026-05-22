#include "dxp/sm5/Recipe.h"

#include "dxp/sm5/Match.h"
#include "dxp/sm5/Rewrite.h"
#include "dxp/sm5/Serialize.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace dxp::sm5 {

namespace {

static std::string Lowercase(const std::string &value) {
  std::string lowered = value;
  for (char &ch : lowered) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return lowered;
}

static uint32_t FloatAsUint(float value) {
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

struct RuntimeRule {
  InstructionMatch Match;
  std::vector<InstructionMatch> MatchSequence;
  bool HasMatchSequence = false;
  std::vector<Instruction> Emit;
  std::string Replace;
  RecipeRuleApplicationMode ApplicationMode = RecipeRuleApplicationMode::First;
  std::function<bool(RecipeContext &, const MatchResult &)> Predicate;
};

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

static bool ParseBoolToken(const std::string &value,
                           bool &parsedValue,
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
                                        uint32_t &mode,
                                        std::string &error) {
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

static bool ParseOperandTypeToken(const std::string &value,
                                  OperandType &type,
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

static bool TryParseComponentChar(char ch,
                                  D3D10_SB_4_COMPONENT_NAME &component,
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
      if (!TryParseComponentChar(operandModel.Swizzle[index],
                                 components[index], error)) {
        return false;
      }
    }
    numComponents = D3D10_SB_OPERAND_4_COMPONENT;
    componentMode = ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
                        D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE) |
                    ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE(
                        components[0], components[1], components[2],
                        components[3]);
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
    numComponents = operandModel.ImmediateU32.size() > 1 ||
                            operandModel.ImmediateF32.size() > 1
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
                               Operand &operand,
                               std::string &error) {
  operand = Operand{};

  if (!operandModel.FromCapture.empty() && !operandModel.BindHandle.empty()) {
    error = "SM5 emit operand cannot use both from_capture and bind_handle";
    return false;
  }

  if (!operandModel.FromCapture.empty() && !operandModel.Scratch.empty()) {
    error = "SM5 emit operand cannot use both from_capture and scratch";
    return false;
  }

  if (!operandModel.FromCapture.empty() && !operandModel.StateTemp.empty()) {
    error = "SM5 emit operand cannot use both from_capture and state_temp";
    return false;
  }

  if (!operandModel.FromCapture.empty()) {
    operand.CaptureName = operandModel.FromCapture;
    return true;
  }

  if (operandModel.Type.empty() && operandModel.Scratch.empty() &&
      operandModel.StateTemp.empty()) {
    error = "literal SM5 emit operands require type, from_capture, scratch, or state_temp";
    return false;
  }

  if (!operandModel.BindHandle.empty() && operandModel.Type.empty()) {
    error = "SM5 bind_handle emit operands require explicit operand type";
    return false;
  }

  if (!operandModel.BindHandle.empty() && !operandModel.Scratch.empty()) {
    error = "SM5 emit operand cannot use both bind_handle and scratch";
    return false;
  }

  if (!operandModel.StateTemp.empty() && !operandModel.Scratch.empty()) {
    error = "SM5 emit operand cannot use both state_temp and scratch";
    return false;
  }

  if (!operandModel.StateTemp.empty() && !operandModel.BindHandle.empty()) {
    error = "SM5 emit operand cannot use both state_temp and bind_handle";
    return false;
  }

  if (!operandModel.Scratch.empty() || !operandModel.StateTemp.empty()) {
    operand.Type = D3D10_SB_OPERAND_TYPE_TEMP;
  } else if (!ParseOperandTypeToken(operandModel.Type, operand.Type, error)) {
    return false;
  }

  if (!operandModel.Scratch.empty() && !operandModel.Type.empty()) {
    OperandType parsedType = D3D10_SB_OPERAND_TYPE_TEMP;
    if (!ParseOperandTypeToken(operandModel.Type, parsedType, error)) {
      return false;
    }
    if (parsedType != D3D10_SB_OPERAND_TYPE_TEMP) {
      error = "SM5 scratch operands must use temp type";
      return false;
    }
  }

  if (!operandModel.StateTemp.empty() && !operandModel.Type.empty()) {
    OperandType parsedType = D3D10_SB_OPERAND_TYPE_TEMP;
    if (!ParseOperandTypeToken(operandModel.Type, parsedType, error)) {
      return false;
    }
    if (parsedType != D3D10_SB_OPERAND_TYPE_TEMP) {
      error = "SM5 state_temp operands must use temp type";
      return false;
    }
  }

  if (!ParseOperandComponentMode(operandModel, operand.Type,
                                 operand.NumComponents,
                                 operand.ComponentMode, error)) {
    return false;
  }

  operand.Indices = operandModel.Indices;
  operand.BindHandle = operandModel.BindHandle;
  operand.StateTempName = operandModel.StateTemp;
  operand.ScratchName = operandModel.Scratch;
  operand.ImmediateValues = operandModel.ImmediateU32;
  for (float immediateValue : operandModel.ImmediateF32) {
    operand.ImmediateValues.push_back(FloatAsUint(immediateValue));
  }

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

  if (!operandModel.Type.empty()) {
    if (!ParseOperandTypeToken(operandModel.Type, operandMatch.MatchType,
                               error)) {
      return false;
    }
    operandMatch.HasTypeMatch = true;
  }

  if (!operandModel.Indices.empty()) {
    operandMatch.MatchIndices.assign(operandModel.Indices.begin(),
                                     operandModel.Indices.end());
    operandMatch.HasIndexMatch = true;
  }

  const OperandType componentType = operandMatch.HasTypeMatch
                                        ? operandMatch.MatchType
                                        : D3D10_SB_OPERAND_TYPE_TEMP;
  uint32_t numComponents = 0;
  uint32_t componentMode = 0;
  if (!operandModel.Mask.empty() || !operandModel.Swizzle.empty() ||
      !operandModel.Select.empty() || operandModel.NumComponents >= 0) {
    if (!ParseOperandComponentMode(operandModel, componentType,
                                   numComponents, componentMode, error)) {
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

  if (!operandModel.ImmediateU32.empty() || !operandModel.ImmediateF32.empty()) {
    operandMatch.MatchImmediates = operandModel.ImmediateU32;
    for (float immediateValue : operandModel.ImmediateF32) {
      operandMatch.MatchImmediates.push_back(FloatAsUint(immediateValue));
    }
    operandMatch.HasImmediateMatch = true;
  }

  operandMatch.CaptureName = operandModel.Capture;
  operandMatch.MatchAgainstCapture = operandModel.MatchCapture;
  return true;
}

static bool CompileInstructionPattern(const RecipeInstructionPattern &matchModel,
                                      InstructionMatch &instructionMatch,
                                      std::string &error) {
  instructionMatch = InstructionMatch{};

  if (!ParseOpcode(matchModel.Opcode, instructionMatch.Opcode)) {
    error = "Unknown SM5 opcode in match: " + matchModel.Opcode;
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
  if (matchModel.TestBoolean >= 0) {
    instructionMatch.HasTestBooleanMatch = true;
    instructionMatch.MatchTestBoolean =
        static_cast<uint32_t>(matchModel.TestBoolean);
  }
  if (!matchModel.InterpolationMode.empty()) {
    uint32_t interpolationMode = 0;
    if (!ParseInterpolationModeToken(matchModel.InterpolationMode,
                                     interpolationMode, error)) {
      return false;
    }
    if (instructionMatch.Opcode != Opcode{D3D10_SB_OPCODE_DCL_INPUT_PS} &&
        instructionMatch.Opcode != Opcode{D3D10_SB_OPCODE_DCL_INPUT_PS_SIV}) {
      error =
          "SM5 interpolation_mode is only valid for dcl_input_ps and dcl_input_ps_siv";
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
                                bool &hasMatchSequence,
                                std::string &error) {
  sequenceMatches.clear();
  hasMatchSequence = false;

  if (!matchModel.Sequence.empty()) {
    if (!matchModel.Opcode.empty() || !matchModel.Capture.empty() ||
        !matchModel.Saturate.empty() || !matchModel.InterpolationMode.empty() ||
        matchModel.TestBoolean >= 0 ||
        !matchModel.Operands.empty()) {
      error =
          "SM5 match.sequence cannot be combined with single-instruction match fields";
      return false;
    }

    hasMatchSequence = true;
    for (const RecipeInstructionPattern &instructionModel : matchModel.Sequence) {
      InstructionMatch instructionMatch;
      if (!CompileInstructionPattern(instructionModel, instructionMatch, error)) {
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
                        RuntimeRule &rule,
                        std::string &error) {
  rule = RuntimeRule{};
  rule.ApplicationMode = inheritedMode;
  if (ruleModel.ApplicationMode != RecipeRuleApplicationMode::First ||
      inheritedMode == RecipeRuleApplicationMode::First) {
    rule.ApplicationMode = ruleModel.ApplicationMode;
  }

  if (!CompileMatchPattern(ruleModel.Match, rule.Match, rule.MatchSequence,
                           rule.HasMatchSequence, error)) {
    if (error == "SM5 match patterns require match.opcode or match.sequence") {
      error = "SM5 rules require match.opcode or match.sequence";
    }
    return false;
  }

  rule.Replace = ruleModel.Replace;

  for (const RecipeInstructionTemplate &emitModel : ruleModel.Emit) {
    if (emitModel.Opcode.empty()) {
      error = "SM5 emit entries require opcode";
      return false;
    }

    Instruction instruction;
    if (!ParseOpcode(emitModel.Opcode, instruction.Opcode)) {
      error = "Unknown SM5 opcode in emit: " + emitModel.Opcode;
      return false;
    }
    if (!emitModel.Saturate.empty()) {
      bool saturate = false;
      if (!ParseBoolToken(emitModel.Saturate, saturate, error)) {
        error = "invalid SM5 emit saturate value: " + error;
        return false;
      }
      instruction.Controls.Saturate = saturate;
    }
    if (emitModel.TestBoolean >= 0) {
      instruction.Controls.HasTestBoolean = true;
      instruction.Controls.TestBoolean =
          static_cast<uint32_t>(emitModel.TestBoolean);
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
        error =
            "SM5 interpolation_mode is only valid for dcl_input_ps and dcl_input_ps_siv";
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
    rule.Emit.push_back(std::move(instruction));
  }

  rule.Predicate = ruleModel.Predicate;

  return true;
}

static bool EvaluateRulePredicate(const RuntimeRule &rule,
                                  const MatchResult &match,
                                  const std::string &stepName,
                                  bool required,
                                  RecipeContext &context,
                                  bool &shouldApply,
                                  std::string &error) {
  shouldApply = true;
  error.clear();
  if (!rule.Predicate) {
    return true;
  }

  try {
    shouldApply = rule.Predicate(context, match);
    return true;
  } catch (const std::exception &exception) {
    const std::string message =
        "SM5 rule predicate threw exception in step '" + stepName + "': " +
        exception.what();
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

static bool CompilePrefilter(const RecipePrefilter &prefilterModel,
                             RuntimePrefilter &prefilter,
                             std::string &error) {
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
        if (error == "SM5 match patterns require match.opcode or match.sequence") {
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
  instruction.LengthInDwords = static_cast<uint32_t>(instruction.RawTokens.size());
  return instruction;
}

struct ScratchAllocationState {
  uint32_t NextTempIndex = 0;
  uint32_t RequiredTempCount = 0;
  std::unordered_map<std::string, uint32_t> ScratchTemps;
};

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

static void EnsureTempDeclaration(Program &program, uint32_t requiredTempCount) {
  if (requiredTempCount <= program.TempCount) {
    return;
  }

  for (auto &instruction : program.Instructions) {
    if (static_cast<OpcodeType>(instruction.Opcode) != D3D10_SB_OPCODE_DCL_TEMPS) {
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
      instruction.LengthInDwords = static_cast<uint32_t>(instruction.RawTokens.size());
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

  program.Instructions.insert(program.Instructions.begin() + static_cast<ptrdiff_t>(insertIndex),
                              BuildTempDeclaration(requiredTempCount));
  program.TempCount = requiredTempCount;
  program.TempSize = requiredTempCount * 4;
}

static std::vector<uint32_t> SelectMatchIndices(
  const std::vector<MatchResult> &matches,
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
                                uint32_t &replaceIndex,
                                std::string &error) {
  replaceIndex = match.RangeStartIndex;
  if (rule.Replace.empty()) {
    return true;
  }

  const uint32_t *capturedIndex = match.GetCapturedInstructionIndex(rule.Replace);
  if (capturedIndex == nullptr) {
    error = "missing captured instruction '" + rule.Replace + "'";
    return false;
  }

  replaceIndex = *capturedIndex;
  return true;
}

static bool ResolveRangeReplacement(const RuntimeRule &rule,
                                    const MatchResult &match,
                                    RewriteAction &action,
                                    std::string &error) {
  if (!rule.Replace.empty()) {
    uint32_t replaceIndex = 0;
    if (!ResolveReplaceIndex(rule, match, replaceIndex, error)) {
      return false;
    }
    action.Type = RewriteActionType::ReplaceOne;
    action.ReplaceIndex = replaceIndex;
    return true;
  }

  action.Type = rule.HasMatchSequence ? RewriteActionType::ReplaceRange
                                      : RewriteActionType::ReplaceOne;
  action.ReplaceIndex = match.RangeStartIndex;
  action.RangeStart = match.RangeStartIndex;
  action.RangeEnd = match.RangeEndIndex;
  return true;
}

static uint32_t MakeSelectComponentMode(D3D10_SB_4_COMPONENT_NAME component) {
  return ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
             D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE) |
         ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECT_1(component);
}

static Operand MakeConstantBufferOperand(uint32_t bindPoint,
                                         uint32_t elementIndex,
                                         D3D10_SB_4_COMPONENT_NAME component) {
  Operand operand;
  operand.Type = D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER;
  operand.NumComponents = D3D10_SB_OPERAND_4_COMPONENT;
  operand.ComponentMode = MakeSelectComponentMode(component);
  operand.Indices = {bindPoint, elementIndex};
  return operand;
}

static Operand MakeSamplerOperand(uint32_t bindPoint) {
  Operand operand;
  operand.Type = D3D10_SB_OPERAND_TYPE_SAMPLER;
  operand.NumComponents = D3D10_SB_OPERAND_0_COMPONENT;
  operand.ComponentMode = 0;
  operand.Indices = {bindPoint};
  return operand;
}

static Operand MakeResourceOperand(uint32_t bindPoint) {
  Operand operand;
  operand.Type = D3D10_SB_OPERAND_TYPE_RESOURCE;
  operand.NumComponents = D3D10_SB_OPERAND_0_COMPONENT;
  operand.ComponentMode = 0;
  operand.Indices = {bindPoint};
  return operand;
}

static Operand MakeInputOperand(uint32_t bindPoint) {
  Operand operand;
  operand.Type = D3D10_SB_OPERAND_TYPE_INPUT;
  operand.NumComponents = D3D10_SB_OPERAND_4_COMPONENT;
  operand.ComponentMode = ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
                            D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) |
                        ENCODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(
                            D3D10_SB_OPERAND_4_COMPONENT_MASK_ALL);
  operand.Indices = {bindPoint};
  return operand;
}

static Operand MakeOutputOperand(uint32_t bindPoint) {
  Operand operand;
  operand.Type = D3D10_SB_OPERAND_TYPE_OUTPUT;
  operand.NumComponents = D3D10_SB_OPERAND_4_COMPONENT;
  operand.ComponentMode = ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
                            D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) |
                        ENCODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(
                            D3D10_SB_OPERAND_4_COMPONENT_MASK_ALL);
  operand.Indices = {bindPoint};
  return operand;
}

static Operand MakeUavOperand(uint32_t bindPoint) {
  Operand operand;
  operand.Type = D3D11_SB_OPERAND_TYPE_UNORDERED_ACCESS_VIEW;
  operand.NumComponents = D3D10_SB_OPERAND_0_COMPONENT;
  operand.ComponentMode = 0;
  operand.Indices = {bindPoint};
  return operand;
}

static bool InstantiateOperand(const Operand &operandTemplate,
                               const MatchResult &match,
                               ScratchAllocationState &scratchState,
                               RecipeContext &context,
                               Operand &operand,
                               std::string &error) {
  if (!operandTemplate.CaptureName.empty()) {
    const Operand *capturedOperand = match.GetCapturedOperand(operandTemplate.CaptureName);
    if (capturedOperand == nullptr) {
      error = "missing captured operand '" + operandTemplate.CaptureName + "'";
      return false;
    }
    operand = *capturedOperand;
    return true;
  }

  operand = operandTemplate;
  if (!operand.ScratchName.empty()) {
    auto scratchIt = scratchState.ScratchTemps.find(operand.ScratchName);
    if (scratchIt == scratchState.ScratchTemps.end()) {
      const uint32_t allocatedIndex = scratchState.NextTempIndex++;
      scratchState.RequiredTempCount = std::max(scratchState.RequiredTempCount,
                                                allocatedIndex + 1);
      scratchIt = scratchState.ScratchTemps.emplace(operand.ScratchName, allocatedIndex).first;
    }
    operand.Indices = {scratchIt->second};
  }

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
        const auto structuredIt = context.StructuredResourceBindings.find(operand.BindHandle);
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
      error = "SM5 bind_handle operand type is unsupported for resource binding";
      return false;
    }

    if (resolvedBindPoint == nullptr) {
      error = "missing SM5 declaration handle binding '" + operand.BindHandle + "'";
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

  if (!operand.StateTempName.empty()) {
    const uint32_t *resolvedTempIndex =
        context.FindState<uint32_t>(operand.StateTempName);
    if (resolvedTempIndex == nullptr) {
      error = "missing SM5 state_temp value '" + operand.StateTempName + "'";
      return false;
    }

    if (operand.Type != D3D10_SB_OPERAND_TYPE_TEMP) {
      error = "SM5 state_temp operands must use temp type";
      return false;
    }

    if (operand.Indices.empty()) {
      operand.Indices.push_back(*resolvedTempIndex);
    } else {
      operand.Indices[0] = *resolvedTempIndex;
    }
  }

  if (operand.RelativeOperand) {
    Operand instantiatedRelative;
    if (!InstantiateOperand(*operand.RelativeOperand, match, scratchState,
                            context,
                            instantiatedRelative, error)) {
      return false;
    }
    operand.RelativeOperand = std::make_shared<Operand>(std::move(instantiatedRelative));
  }
  return true;
}

static bool InstantiateInstruction(const Instruction &instructionTemplate,
                                   const MatchResult &match,
                                   ScratchAllocationState &scratchState,
                                   RecipeContext &context,
                                   Instruction &instruction,
                                   std::string &error) {
  instruction = instructionTemplate;
  instruction.Operands.clear();
  instruction.RawTokens.clear();
  instruction.LengthInDwords = 0;

  for (const Operand &operandTemplate : instructionTemplate.Operands) {
    Operand operand;
    if (!InstantiateOperand(operandTemplate, match, scratchState, context,
                            operand, error)) {
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
  instructions.clear();
  instructions.reserve(templates.size());
  ScratchAllocationState scratchState;
  scratchState.NextTempIndex = baseTempCount;
  scratchState.RequiredTempCount = baseTempCount;
  for (const Instruction &instructionTemplate : templates) {
    Instruction instruction;
    if (!InstantiateInstruction(instructionTemplate, match, scratchState,
                                context, instruction, error)) {
      return false;
    }
    instructions.push_back(std::move(instruction));
  }
  requiredTempCount = scratchState.RequiredTempCount;
  return true;
}

static Instruction BuildConstantBufferDeclaration(const RecipeCBufferDecl &decl) {
  Instruction instruction;
  instruction.Opcode = Opcode{D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER};
  const auto operand = EncodeOperand(MakeConstantBufferOperand(
      decl.BindPoint, decl.Elements, D3D10_SB_4_COMPONENT_X));

  instruction.RawTokens.push_back(
      ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER) |
      ENCODE_D3D10_SB_D3D10_SB_CONSTANT_BUFFER_ACCESS_PATTERN(decl.AccessPattern) |
      ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(
          1u + static_cast<uint32_t>(operand.size())));
  instruction.RawTokens.insert(instruction.RawTokens.end(), operand.begin(), operand.end());
  instruction.LengthInDwords = static_cast<uint32_t>(instruction.RawTokens.size());
  instruction.Operands.push_back(
      MakeConstantBufferOperand(decl.BindPoint, decl.Elements, D3D10_SB_4_COMPONENT_X));
  return instruction;
}

static Instruction BuildTextureDeclaration(const RecipeTextureDecl &decl) {
  Instruction instruction;
  instruction.Opcode = Opcode{D3D10_SB_OPCODE_DCL_RESOURCE};
  const auto operand = EncodeOperand(MakeResourceOperand(decl.BindPoint));
  const uint32_t returnTypeToken =
      ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, 0) |
      ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, 1) |
      ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, 2) |
      ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, 3);

  instruction.RawTokens.push_back(
      ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_DCL_RESOURCE) |
      ENCODE_D3D10_SB_RESOURCE_DIMENSION(
          static_cast<D3D10_SB_RESOURCE_DIMENSION>(decl.Dimension)) |
      ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(
          2u + static_cast<uint32_t>(operand.size())));
  instruction.RawTokens.insert(instruction.RawTokens.end(), operand.begin(), operand.end());
  instruction.RawTokens.push_back(returnTypeToken);
  instruction.LengthInDwords = static_cast<uint32_t>(instruction.RawTokens.size());
  instruction.Operands.push_back(MakeResourceOperand(decl.BindPoint));
  return instruction;
}

static Instruction BuildInputDeclaration(const RecipeInputDecl &decl) {
  Instruction instruction;
  instruction.Opcode = Opcode{D3D10_SB_OPCODE_DCL_INPUT_PS};
  instruction.Controls.HasInputInterpolationMode = true;
  instruction.Controls.InputInterpolationMode = decl.InterpolationMode;
  const auto operand = EncodeOperand(MakeInputOperand(decl.BindPoint));

  instruction.RawTokens.push_back(
      ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_DCL_INPUT_PS) |
      ENCODE_D3D10_SB_INPUT_INTERPOLATION_MODE(
          static_cast<D3D10_SB_INTERPOLATION_MODE>(decl.InterpolationMode)) |
      ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(
          1u + static_cast<uint32_t>(operand.size())));
  instruction.RawTokens.insert(instruction.RawTokens.end(), operand.begin(), operand.end());
  instruction.LengthInDwords = static_cast<uint32_t>(instruction.RawTokens.size());
  instruction.Operands.push_back(MakeInputOperand(decl.BindPoint));
  return instruction;
}

static Instruction BuildOutputDeclaration(const RecipeOutputDecl &decl) {
  Instruction instruction;
  instruction.Opcode = Opcode{D3D10_SB_OPCODE_DCL_OUTPUT};
  const auto operand = EncodeOperand(MakeOutputOperand(decl.BindPoint));

  instruction.RawTokens.push_back(
      ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_DCL_OUTPUT) |
      ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(
          1u + static_cast<uint32_t>(operand.size())));
  instruction.RawTokens.insert(instruction.RawTokens.end(), operand.begin(), operand.end());
  instruction.LengthInDwords = static_cast<uint32_t>(instruction.RawTokens.size());
  instruction.Operands.push_back(MakeOutputOperand(decl.BindPoint));
  return instruction;
}

static Instruction BuildSamplerDeclaration(const RecipeSamplerDecl &decl) {
  Instruction instruction;
  instruction.Opcode = Opcode{D3D10_SB_OPCODE_DCL_SAMPLER};
  const auto operand = EncodeOperand(MakeSamplerOperand(decl.BindPoint));

  instruction.RawTokens.push_back(
      ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_DCL_SAMPLER) |
      ENCODE_D3D10_SB_SAMPLER_MODE(
          static_cast<D3D10_SB_SAMPLER_MODE>(decl.Mode)) |
      ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(
          1u + static_cast<uint32_t>(operand.size())));
  instruction.RawTokens.insert(instruction.RawTokens.end(), operand.begin(), operand.end());
  instruction.LengthInDwords = static_cast<uint32_t>(instruction.RawTokens.size());
  instruction.Operands.push_back(MakeSamplerOperand(decl.BindPoint));
  return instruction;
}

static Instruction BuildRawResourceDeclaration(const RecipeRawResourceDecl &decl) {
  Instruction instruction;
  instruction.Opcode = Opcode{D3D11_SB_OPCODE_DCL_RESOURCE_RAW};
  const auto operand = EncodeOperand(MakeResourceOperand(decl.BindPoint));

  instruction.RawTokens.push_back(
      ENCODE_D3D10_SB_OPCODE_TYPE(D3D11_SB_OPCODE_DCL_RESOURCE_RAW) |
      ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(
          1u + static_cast<uint32_t>(operand.size())));
  instruction.RawTokens.insert(instruction.RawTokens.end(), operand.begin(), operand.end());
  instruction.LengthInDwords = static_cast<uint32_t>(instruction.RawTokens.size());
  instruction.Operands.push_back(MakeResourceOperand(decl.BindPoint));
  return instruction;
}

static Instruction BuildStructuredResourceDeclaration(const RecipeStructuredResourceDecl &decl) {
  Instruction instruction;
  instruction.Opcode = Opcode{D3D11_SB_OPCODE_DCL_RESOURCE_STRUCTURED};
  const auto operand = EncodeOperand(MakeResourceOperand(decl.BindPoint));

  instruction.RawTokens.push_back(
      ENCODE_D3D10_SB_OPCODE_TYPE(D3D11_SB_OPCODE_DCL_RESOURCE_STRUCTURED) |
      ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(
          2u + static_cast<uint32_t>(operand.size())));
  instruction.RawTokens.insert(instruction.RawTokens.end(), operand.begin(), operand.end());
  instruction.RawTokens.push_back(decl.StructureStride);
  instruction.LengthInDwords = static_cast<uint32_t>(instruction.RawTokens.size());
  instruction.Operands.push_back(MakeResourceOperand(decl.BindPoint));
  return instruction;
}

static Instruction BuildUavDeclaration(const RecipeUavDecl &decl) {
  Instruction instruction;

  if (decl.Kind == RecipeUavKind::Raw) {
    instruction.Opcode = Opcode{D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW};
    const auto operand = EncodeOperand(MakeUavOperand(decl.BindPoint));
    instruction.RawTokens.push_back(
        ENCODE_D3D10_SB_OPCODE_TYPE(D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW) |
        ENCODE_D3D11_SB_ACCESS_COHERENCY_FLAGS(
            decl.GloballyCoherent ? D3D11_SB_GLOBALLY_COHERENT_ACCESS : 0) |
        ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(
            1u + static_cast<uint32_t>(operand.size())));
    instruction.RawTokens.insert(instruction.RawTokens.end(), operand.begin(), operand.end());
    instruction.LengthInDwords = static_cast<uint32_t>(instruction.RawTokens.size());
    instruction.Operands.push_back(MakeUavOperand(decl.BindPoint));
    return instruction;
  }

  if (decl.Kind == RecipeUavKind::Structured) {
    instruction.Opcode = Opcode{D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED};
    const auto operand = EncodeOperand(MakeUavOperand(decl.BindPoint));
    uint32_t flags = decl.GloballyCoherent ? D3D11_SB_GLOBALLY_COHERENT_ACCESS : 0;
    if (decl.HasOrderPreservingCounter) {
      flags |= D3D11_SB_UAV_HAS_ORDER_PRESERVING_COUNTER;
    }
    instruction.RawTokens.push_back(
        ENCODE_D3D10_SB_OPCODE_TYPE(D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED) |
        ENCODE_D3D11_SB_ACCESS_COHERENCY_FLAGS(flags) |
        ENCODE_D3D11_SB_UAV_FLAGS(flags) |
        ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(
            2u + static_cast<uint32_t>(operand.size())));
    instruction.RawTokens.insert(instruction.RawTokens.end(), operand.begin(), operand.end());
    instruction.RawTokens.push_back(decl.StructureStride);
    instruction.LengthInDwords = static_cast<uint32_t>(instruction.RawTokens.size());
    instruction.Operands.push_back(MakeUavOperand(decl.BindPoint));
    return instruction;
  }

  instruction.Opcode = Opcode{D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED};
  const auto operand = EncodeOperand(MakeUavOperand(decl.BindPoint));
  const uint32_t returnTypeToken =
      ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, 0) |
      ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, 1) |
      ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, 2) |
      ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, 3);

  instruction.RawTokens.push_back(
      ENCODE_D3D10_SB_OPCODE_TYPE(D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED) |
      ENCODE_D3D10_SB_RESOURCE_DIMENSION(
          static_cast<D3D10_SB_RESOURCE_DIMENSION>(decl.Dimension)) |
      ENCODE_D3D11_SB_ACCESS_COHERENCY_FLAGS(
          decl.GloballyCoherent ? D3D11_SB_GLOBALLY_COHERENT_ACCESS : 0) |
      ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(
          2u + static_cast<uint32_t>(operand.size())));
  instruction.RawTokens.insert(instruction.RawTokens.end(), operand.begin(), operand.end());
  instruction.RawTokens.push_back(returnTypeToken);
  instruction.LengthInDwords = static_cast<uint32_t>(instruction.RawTokens.size());
  instruction.Operands.push_back(MakeUavOperand(decl.BindPoint));
  return instruction;
}

static uint32_t FindInsertAfterLastDeclaration(const Program &program,
                                               OpcodeType opcode) {
  uint32_t insertIndex = 0;
  for (uint32_t i = 0; i < program.Instructions.size(); ++i) {
    if (static_cast<OpcodeType>(program.Instructions[i].Opcode) == opcode) {
      insertIndex = i + 1;
    }
  }
  return insertIndex;
}

static bool AllocateBindPoint(std::unordered_set<uint32_t> &occupied,
                              bool autoBind,
                              uint32_t requestedBindPoint,
                              uint32_t &resolvedBindPoint,
                              std::string &error) {
  if (!autoBind) {
    if (occupied.find(requestedBindPoint) != occupied.end()) {
      error = "SM5 declaration bind point already occupied: " +
              std::to_string(requestedBindPoint);
      return false;
    }

    resolvedBindPoint = requestedBindPoint;
    occupied.insert(resolvedBindPoint);
    return true;
  }

  uint32_t candidate = requestedBindPoint;
  while (occupied.find(candidate) != occupied.end()) {
    if (candidate == std::numeric_limits<uint32_t>::max()) {
      error = "SM5 declaration auto_bind exhausted available bind points";
      return false;
    }
    ++candidate;
  }

  resolvedBindPoint = candidate;
  occupied.insert(resolvedBindPoint);
  return true;
}

static bool RecordNamedBinding(std::unordered_map<std::string, uint32_t> &bindings,
                               const std::string &handle,
                               uint32_t bindPoint,
                               const char *resourceKind,
                               std::string &error) {
  if (handle.empty()) {
    return true;
  }

  if (bindings.find(handle) != bindings.end()) {
    error = std::string("duplicate SM5 ") + resourceKind +
            " declaration handle: " + handle;
    return false;
  }

  bindings.emplace(handle, bindPoint);
  return true;
}

static bool AddRecipeDeclarations(Program &program,
                                  const Recipe &recipe,
                                  RecipeContext &context,
                                  std::string &error) {
  const auto &inputDecls = recipe.GetInputDecls();
  const auto &outputDecls = recipe.GetOutputDecls();
  const auto &cbufferDecls = recipe.GetCBufferDecls();
  const auto &textureDecls = recipe.GetTextureDecls();
  const auto &rawResourceDecls = recipe.GetRawResourceDecls();
  const auto &structuredResourceDecls = recipe.GetStructuredResourceDecls();
  const auto &samplerDecls = recipe.GetSamplerDecls();
  const auto &uavDecls = recipe.GetUavDecls();

  std::unordered_set<uint32_t> occupiedCBufferBindPoints;
  for (const auto &cbuffer : program.CBuffers) {
    occupiedCBufferBindPoints.insert(cbuffer.RegisterBindPoint);
  }

  std::unordered_set<uint32_t> occupiedTextureBindPoints;
  std::unordered_set<uint32_t> occupiedUavBindPoints;
  for (const auto &instruction : program.Instructions) {
    const auto opcode = static_cast<OpcodeType>(instruction.Opcode);
    if ((opcode == D3D10_SB_OPCODE_DCL_RESOURCE ||
         opcode == D3D11_SB_OPCODE_DCL_RESOURCE_RAW ||
         opcode == D3D11_SB_OPCODE_DCL_RESOURCE_STRUCTURED) &&
        !instruction.Operands.empty() && !instruction.Operands.front().Indices.empty()) {
      occupiedTextureBindPoints.insert(instruction.Operands.front().Indices.front());
    }
    if ((opcode == D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED ||
         opcode == D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW ||
         opcode == D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED) &&
        !instruction.Operands.empty() && !instruction.Operands.front().Indices.empty()) {
      occupiedUavBindPoints.insert(instruction.Operands.front().Indices.front());
    }
  }

  std::unordered_set<uint32_t> occupiedSamplerBindPoints;
  for (const auto &sampler : program.Samplers) {
    occupiedSamplerBindPoints.insert(sampler.RegisterBindPoint);
  }

  std::unordered_set<uint32_t> occupiedInputBindPoints;
  std::unordered_set<uint32_t> occupiedOutputBindPoints;
  for (const auto &instruction : program.Instructions) {
    const auto opcode = static_cast<OpcodeType>(instruction.Opcode);
    if ((opcode == D3D10_SB_OPCODE_DCL_INPUT ||
         opcode == D3D10_SB_OPCODE_DCL_INPUT_PS ||
         opcode == D3D10_SB_OPCODE_DCL_INPUT_PS_SIV ||
         opcode == D3D10_SB_OPCODE_DCL_INPUT_PS_SGV) &&
        !instruction.Operands.empty() && !instruction.Operands.front().Indices.empty()) {
      occupiedInputBindPoints.insert(instruction.Operands.front().Indices.front());
    }
    if ((opcode == D3D10_SB_OPCODE_DCL_OUTPUT ||
         opcode == D3D10_SB_OPCODE_DCL_OUTPUT_SIV ||
         opcode == D3D10_SB_OPCODE_DCL_OUTPUT_SGV) &&
        !instruction.Operands.empty() && !instruction.Operands.front().Indices.empty()) {
      occupiedOutputBindPoints.insert(instruction.Operands.front().Indices.front());
    }
  }

  if (!inputDecls.empty()) {
    uint32_t insertIndex = 0;
    for (uint32_t i = 0; i < program.Instructions.size(); ++i) {
      const auto opcode = static_cast<OpcodeType>(program.Instructions[i].Opcode);
      if (opcode == D3D10_SB_OPCODE_DCL_INPUT ||
          opcode == D3D10_SB_OPCODE_DCL_INPUT_PS ||
          opcode == D3D10_SB_OPCODE_DCL_INPUT_PS_SIV ||
          opcode == D3D10_SB_OPCODE_DCL_INPUT_PS_SGV) {
        insertIndex = i + 1;
      }
    }

    for (const auto &decl : inputDecls) {
      uint32_t resolvedBindPoint = decl.BindPoint;
      if (!AllocateBindPoint(occupiedInputBindPoints, decl.AutoBind,
                             decl.BindPoint, resolvedBindPoint, error)) {
        return false;
      }

      RecipeInputDecl resolvedDecl = decl;
      resolvedDecl.BindPoint = resolvedBindPoint;
      program.Instructions.insert(program.Instructions.begin() + insertIndex,
                                  BuildInputDeclaration(resolvedDecl));
      ++insertIndex;
      context.ProgramModified = true;

      if (!RecordNamedBinding(context.InputBindings, decl.Handle,
                              resolvedBindPoint, "input", error)) {
        return false;
      }
    }
  }

  if (!outputDecls.empty()) {
    uint32_t insertIndex = 0;
    for (uint32_t i = 0; i < program.Instructions.size(); ++i) {
      const auto opcode = static_cast<OpcodeType>(program.Instructions[i].Opcode);
      if (opcode == D3D10_SB_OPCODE_DCL_OUTPUT ||
          opcode == D3D10_SB_OPCODE_DCL_OUTPUT_SIV ||
          opcode == D3D10_SB_OPCODE_DCL_OUTPUT_SGV) {
        insertIndex = i + 1;
      }
    }

    for (const auto &decl : outputDecls) {
      uint32_t resolvedBindPoint = decl.BindPoint;
      if (!AllocateBindPoint(occupiedOutputBindPoints, decl.AutoBind,
                             decl.BindPoint, resolvedBindPoint, error)) {
        return false;
      }

      RecipeOutputDecl resolvedDecl = decl;
      resolvedDecl.BindPoint = resolvedBindPoint;
      program.Instructions.insert(program.Instructions.begin() + insertIndex,
                                  BuildOutputDeclaration(resolvedDecl));
      ++insertIndex;
      context.ProgramModified = true;

      if (!RecordNamedBinding(context.OutputBindings, decl.Handle,
                              resolvedBindPoint, "output", error)) {
        return false;
      }
    }
  }

  if (!cbufferDecls.empty()) {
    uint32_t insertIndex = FindInsertAfterLastDeclaration(program, D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER);
    for (const auto &decl : cbufferDecls) {
      uint32_t resolvedBindPoint = decl.BindPoint;
      if (!AllocateBindPoint(occupiedCBufferBindPoints, decl.AutoBind,
                             decl.BindPoint, resolvedBindPoint, error)) {
        return false;
      }

      RecipeCBufferDecl resolvedDecl = decl;
      resolvedDecl.BindPoint = resolvedBindPoint;
      program.Instructions.insert(program.Instructions.begin() + insertIndex,
                                  BuildConstantBufferDeclaration(resolvedDecl));
      ++insertIndex;
      context.ProgramModified = true;

      if (!RecordNamedBinding(context.CBufferBindings, decl.Handle,
                              resolvedBindPoint, "cbuffer", error)) {
        return false;
      }
    }
  }

  if (!textureDecls.empty()) {
    uint32_t insertIndex = FindInsertAfterLastDeclaration(program, D3D10_SB_OPCODE_DCL_RESOURCE);
    for (const auto &decl : textureDecls) {
      uint32_t resolvedBindPoint = decl.BindPoint;
      if (!AllocateBindPoint(occupiedTextureBindPoints, decl.AutoBind,
                             decl.BindPoint, resolvedBindPoint, error)) {
        return false;
      }

      RecipeTextureDecl resolvedDecl = decl;
      resolvedDecl.BindPoint = resolvedBindPoint;
      program.Instructions.insert(program.Instructions.begin() + insertIndex,
                                  BuildTextureDeclaration(resolvedDecl));
      ++insertIndex;
      context.ProgramModified = true;

      if (!RecordNamedBinding(context.TextureBindings, decl.Handle,
                              resolvedBindPoint, "texture", error)) {
        return false;
      }
    }
  }

  if (!rawResourceDecls.empty()) {
    uint32_t insertIndex = FindInsertAfterLastDeclaration(program, D3D11_SB_OPCODE_DCL_RESOURCE_RAW);
    for (const auto &decl : rawResourceDecls) {
      uint32_t resolvedBindPoint = decl.BindPoint;
      if (!AllocateBindPoint(occupiedTextureBindPoints, decl.AutoBind,
                             decl.BindPoint, resolvedBindPoint, error)) {
        return false;
      }

      RecipeRawResourceDecl resolvedDecl = decl;
      resolvedDecl.BindPoint = resolvedBindPoint;
      program.Instructions.insert(program.Instructions.begin() + insertIndex,
                                  BuildRawResourceDeclaration(resolvedDecl));
      ++insertIndex;
      context.ProgramModified = true;

      if (!RecordNamedBinding(context.RawResourceBindings, decl.Handle,
                              resolvedBindPoint, "raw resource", error)) {
        return false;
      }
    }
  }

  if (!structuredResourceDecls.empty()) {
    uint32_t insertIndex = FindInsertAfterLastDeclaration(program, D3D11_SB_OPCODE_DCL_RESOURCE_STRUCTURED);
    for (const auto &decl : structuredResourceDecls) {
      uint32_t resolvedBindPoint = decl.BindPoint;
      if (!AllocateBindPoint(occupiedTextureBindPoints, decl.AutoBind,
                             decl.BindPoint, resolvedBindPoint, error)) {
        return false;
      }

      RecipeStructuredResourceDecl resolvedDecl = decl;
      resolvedDecl.BindPoint = resolvedBindPoint;
      program.Instructions.insert(program.Instructions.begin() + insertIndex,
                                  BuildStructuredResourceDeclaration(resolvedDecl));
      ++insertIndex;
      context.ProgramModified = true;

      if (!RecordNamedBinding(context.StructuredResourceBindings, decl.Handle,
                              resolvedBindPoint, "structured resource", error)) {
        return false;
      }
    }
  }

  if (!samplerDecls.empty()) {
    uint32_t insertIndex = FindInsertAfterLastDeclaration(program, D3D10_SB_OPCODE_DCL_SAMPLER);
    for (const auto &decl : samplerDecls) {
      uint32_t resolvedBindPoint = decl.BindPoint;
      if (!AllocateBindPoint(occupiedSamplerBindPoints, decl.AutoBind,
                             decl.BindPoint, resolvedBindPoint, error)) {
        return false;
      }

      RecipeSamplerDecl resolvedDecl = decl;
      resolvedDecl.BindPoint = resolvedBindPoint;
      program.Instructions.insert(program.Instructions.begin() + insertIndex,
                                  BuildSamplerDeclaration(resolvedDecl));
      ++insertIndex;
      context.ProgramModified = true;

      if (!RecordNamedBinding(context.SamplerBindings, decl.Handle,
                              resolvedBindPoint, "sampler", error)) {
        return false;
      }
    }
  }

  if (!uavDecls.empty()) {
    uint32_t insertIndex = FindInsertAfterLastDeclaration(program, D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED);
    for (const auto &decl : uavDecls) {
      uint32_t resolvedBindPoint = decl.BindPoint;
      if (!AllocateBindPoint(occupiedUavBindPoints, decl.AutoBind,
                             decl.BindPoint, resolvedBindPoint, error)) {
        return false;
      }

      RecipeUavDecl resolvedDecl = decl;
      resolvedDecl.BindPoint = resolvedBindPoint;
      program.Instructions.insert(program.Instructions.begin() + insertIndex,
                                  BuildUavDeclaration(resolvedDecl));
      ++insertIndex;
      context.ProgramModified = true;

      if (!RecordNamedBinding(context.UavBindings, decl.Handle,
                              resolvedBindPoint, "uav", error)) {
        return false;
      }
    }
  }

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

static bool ExecutePrefilters(const Program &program,
                              const std::vector<RecipePrefilter> &prefilters,
                              RecipeContext &context) {
  for (const RecipePrefilter &prefilterModel : prefilters) {
    RuntimePrefilter prefilter;
    std::string compileError;
    if (!CompilePrefilter(prefilterModel, prefilter, compileError)) {
      context.LastError = std::move(compileError);
      context.AddDiagnostic(context.LastError);
      return false;
    }

    if (PrefilterMatches(program, prefilter)) {
      continue;
    }

    const std::string name = prefilter.Name.empty() ? "prefilter" : prefilter.Name;
    if (!prefilter.Required) {
      context.AddDiagnostic("optional SM5 prefilter did not match: " + name);
      continue;
    }

    context.LastError = "required SM5 prefilter did not match: " + name;
    context.AddDiagnostic(context.LastError);
    return false;
  }
  return true;
}

static void ApplyReservedTemps(Program &program,
                               const Recipe &recipe,
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
}

class ScopedProgramBinding {
public:
  ScopedProgramBinding(RecipeContext &context, Program &program)
      : context_(context), previous_(context.ProgramHandle) {
    context_.ProgramHandle = &program;
  }

  ~ScopedProgramBinding() {
    context_.ProgramHandle = previous_;
  }

private:
  RecipeContext &context_;
  Program *previous_;
};

static RecipeStepResult ExecuteRewriteRules(Program &program,
                                            const std::string &stepName,
                                            const std::vector<RecipeRule> &rules,
                                            RecipeRuleApplicationMode mode,
                                            bool required,
                                            RecipeContext &context) {
  RecipeStepResult result;
  for (const RecipeRule &ruleModel : rules) {
    RuntimeRule rule;
    std::string compileError;
    if (!CompileRule(ruleModel, mode, rule, compileError)) {
      return MakeRecipeStepFailure(context, std::move(compileError));
    }

    const auto matches = rule.HasMatchSequence
                             ? CollectSequenceMatches(program, rule.MatchSequence)
                             : CollectMatches(program, rule.Match);
    result.MatchCount += static_cast<uint32_t>(matches.size());
    if (matches.empty()) {
      if (required)
        return MakeRecipeStepFailure(context,
                                     "required recipe step had no matches");
      continue;
    }

    const auto selectedMatches = SelectMatchIndices(matches, rule.ApplicationMode);

    if (!rule.Emit.empty()) {
      std::vector<RewriteAction> actions;
      actions.reserve(selectedMatches.size());
      uint32_t requiredTempCount = program.TempCount;
      for (uint32_t selectedIndex : selectedMatches) {
        const auto &match = matches[selectedIndex];
        bool shouldApply = true;
        std::string predicateError;
        if (!EvaluateRulePredicate(rule, match, stepName, required,
                                   context, shouldApply, predicateError)) {
          return MakeRecipeStepFailure(context, std::move(predicateError));
        }
        if (!shouldApply) {
          continue;
        }

        std::string error;
        RewriteAction action;
        if (!ResolveRangeReplacement(rule, match, action, error)) {
          return MakeRecipeStepFailure(context, error);
        }
        uint32_t actionRequiredTempCount = program.TempCount;
        if (!BuildRewriteInstructions(rule.Emit, match, program.TempCount,
                                      context,
                                      action.NewInstructions,
                                      actionRequiredTempCount,
                                      error)) {
          return MakeRecipeStepFailure(context, error);
        }
        requiredTempCount = std::max(requiredTempCount, actionRequiredTempCount);
        actions.push_back(std::move(action));
      }

      std::sort(actions.begin(), actions.end(),
                [](const RewriteAction &lhs, const RewriteAction &rhs) {
                  const uint32_t lhsIndex = lhs.Type == RewriteActionType::ReplaceRange
                                                ? lhs.RangeStart
                                                : lhs.ReplaceIndex;
                  const uint32_t rhsIndex = rhs.Type == RewriteActionType::ReplaceRange
                                                ? rhs.RangeStart
                                                : rhs.ReplaceIndex;
                  return lhsIndex > rhsIndex;
                });

      if (actions.empty()) {
        continue;
      }

      if (!ApplyRewriteActions(program, actions))
        return MakeRecipeStepFailure(context, "failed to apply rewrite action");
      EnsureTempDeclaration(program, requiredTempCount);
      result.Changed = true;
    }
  }

  (void)stepName;
  result.Success = true;
  return result;
}

} // namespace

RecipeStepResult MakeRecipeStepSuccess(bool changed,
                                       uint32_t matchCount,
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

RecipeStep MakeCustomRecipeStep(std::string name,
                                RecipeStepExecutor execute) {
  RecipeStep step;
  step.Name = std::move(name);
  step.Execute = std::move(execute);
  return step;
}

RecipeStep MakeRewriteRulesStep(std::string name,
                                std::vector<RecipeRule> rules,
                                RecipeRuleApplicationMode mode,
                                bool required) {
  RecipeStep step;
  step.Name = std::move(name);
  step.Required = required;
  step.Execute = [name = step.Name, rules = std::move(rules), mode,
                  required](RecipeContext &context) {
    if (context.ProgramHandle == nullptr) {
      return MakeRecipeStepFailure(context,
                                   "recipe context is missing active SM5 program");
    }
    return ExecuteRewriteRules(*context.ProgramHandle, name, rules, mode,
                               required, context);
  };
  return step;
}

RecipePrefilter MakeShaderVersionPrefilter(uint32_t majorVersion,
                                           uint32_t minorVersion,
                                           std::string name,
                                           bool required) {
  RecipePrefilter prefilter;
  prefilter.Kind = PrefilterKind::CheckShaderVersion;
  prefilter.Name = std::move(name);
  prefilter.Required = required;
  prefilter.ExpectedMajorVersion = majorVersion;
  prefilter.ExpectedMinorVersion = minorVersion;
  return prefilter;
}

RecipePrefilter MakeOpcodeCountPrefilter(std::string opcode,
                                         int32_t expectedCount,
                                         std::string name,
                                         bool required) {
  RecipePrefilter prefilter;
  prefilter.Kind = PrefilterKind::CheckOpcodeCount;
  prefilter.Name = std::move(name);
  prefilter.Required = required;
  prefilter.Opcode = std::move(opcode);
  prefilter.ExpectedCount = expectedCount;
  return prefilter;
}

RecipePrefilter MakeResourceCountPrefilter(int32_t expectedResourceCount,
                                           std::string name,
                                           bool required) {
  RecipePrefilter prefilter;
  prefilter.Kind = PrefilterKind::CheckResourceCount;
  prefilter.Name = std::move(name);
  prefilter.Required = required;
  prefilter.ExpectedResourceCount = expectedResourceCount;
  return prefilter;
}

RecipePrefilter MakePatternPrefilter(RecipeMatchPattern match,
                                     std::string name,
                                     bool required) {
  RecipePrefilter prefilter;
  prefilter.Kind = PrefilterKind::CheckPatternMatch;
  prefilter.Name = std::move(name);
  prefilter.Required = required;
  prefilter.Match = std::move(match);
  return prefilter;
}

bool ReserveTempRegisters(RecipeContext &context,
                          uint32_t count,
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
  return true;
}

static RecipeStepResult ExecuteRecipeStep(Program &program,
                                          const RecipeStep &step,
                                          RecipeContext &context) {
  if (step.Execute) {
    if (!step.Rules.empty()) {
      return MakeRecipeStepFailure(
          context, "SM5 recipe step '" + step.Name +
                       "' cannot combine execute callback with declarative rules");
    }
    return step.Execute(context);
  }

  return ExecuteRewriteRules(program, step.Name, step.Rules,
                             step.ApplicationMode, step.Required, context);
}

bool ExecuteRecipe(Program &program,
                   const Recipe &recipe,
                   RecipeContext &context) {
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

  if (!ExecutePrefilters(program, recipe.GetPrefilters(), context))
    return false;

  ApplyReservedTemps(program, recipe, context);

  std::string declarationError;
  if (!AddRecipeDeclarations(program, recipe, context, declarationError)) {
    context.LastError = std::move(declarationError);
    context.AddDiagnostic(context.LastError);
    return false;
  }

  for (const auto &step : recipe.GetSteps()) {
    const auto result = ExecuteRecipeStep(program, step, context);
    context.TotalRuleMatches += result.MatchCount;
    context.ProgramModified = context.ProgramModified || result.Changed;
    if (!result.Success && step.Required)
      return false;
    if (result.StopRecipe)
      break;
  }

  return true;
}

} // namespace dxp::sm5
