#include "dxp/sm5/Match.h"

#include <algorithm>

namespace dxp {
namespace sm5 {

OperandMatch::OperandMatch()
  : MatchType(D3D10_SB_OPERAND_TYPE_TEMP), HasTypeMatch(false), HasIndexMatch(false),
  MatchComponentMode(D3D10_SB_OPERAND_4_COMPONENT_NOSWIZZLE), HasComponentMatch(false),
      MatchNumComponents(0), HasNumComponentsMatch(false),
  MatchModifier(D3D10_SB_OPERAND_MODIFIER_NONE), HasModifierMatch(false),
      HasImmediateMatch(false), MatchRelativeAddressing(false), HasRelativeMatch(false) {}

InstructionMatch::InstructionMatch()
  : Opcode(Opcode::Unknown()), HasOpcode(false), MatchSaturate(false),
      HasSaturateMatch(false), SaturateValue(false), MatchTestBoolean(0),
  HasTestBooleanMatch(false),
  MatchInputInterpolationMode(D3D10_SB_INTERPOLATION_UNDEFINED),
  HasInputInterpolationModeMatch(false) {}

const Operand *MatchResult::GetCapturedOperand(const std::string &name) const {
  const auto it = CapturedOperands.find(name);
  return it == CapturedOperands.end() ? nullptr : it->second;
}

const Instruction *MatchResult::GetCapturedInstruction(const std::string &name) const {
  const auto it = CapturedInstructions.find(name);
  return it == CapturedInstructions.end() ? nullptr : it->second;
}

const uint32_t *MatchResult::GetCapturedInstructionIndex(const std::string &name) const {
  const auto it = CapturedInstructionIndices.find(name);
  return it == CapturedInstructionIndices.end() ? nullptr : &it->second;
}

namespace {

static bool OperandsEqual(const Operand &lhs, const Operand &rhs) {
  if (lhs.Type != rhs.Type || lhs.NumComponents != rhs.NumComponents ||
      lhs.ComponentMode != rhs.ComponentMode || lhs.Modifier != rhs.Modifier ||
      lhs.Indices != rhs.Indices || lhs.ImmediateValues != rhs.ImmediateValues) {
    return false;
  }

  if (static_cast<bool>(lhs.RelativeOperand) != static_cast<bool>(rhs.RelativeOperand)) {
    return false;
  }

  if (lhs.RelativeOperand && rhs.RelativeOperand) {
    return OperandsEqual(*lhs.RelativeOperand, *rhs.RelativeOperand);
  }

  return true;
}

/// Decode operand type from operand token0 using WDK spec
static OperandType DecodeOperandType(uint32_t token) {
  return static_cast<OperandType>(DECODE_D3D10_SB_OPERAND_TYPE(token));
}

/// Decode number of components from operand token0
static uint32_t DecodeNumComponents(uint32_t token) {
  return DECODE_D3D10_SB_OPERAND_NUM_COMPONENTS(token);
}

/// Decode swizzle from operand token0
static uint32_t DecodeSwizzle(uint32_t token) {
  return DECODE_D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE(token);
}

/// Check if operand is extended
static bool IsOperandExtended(uint32_t token) {
  return DECODE_IS_D3D10_SB_OPERAND_EXTENDED(token) != 0;
}

/// Decode extended operand type
static ExtendedOpcodeType DecodeExtendedOperandType(uint32_t token) {
  return static_cast<ExtendedOpcodeType>(DECODE_D3D10_SB_EXTENDED_OPERAND_TYPE(token));
}

/// Decode operand modifier from extended operand token
static OperandModifier DecodeOperandModifier(uint32_t token) {
  return static_cast<OperandModifier>(DECODE_D3D10_SB_OPERAND_MODIFIER(token));
}

/// Decode index representation for a given dimension
static uint32_t DecodeIndexRepresentation(uint32_t token, uint32_t dim) {
  return (token >> (22 + 3 * dim)) & 0x7u;
}

/// Extract index values from an operand by re-scanning raw tokens
static std::vector<uint32_t> ExtractIndexValues(const Operand &operand, const uint8_t *data, uint32_t startDword) {
  // If we already have immediate values, return them
  if (!operand.ImmediateValues.empty()) {
    return operand.ImmediateValues;
  }
  return {};
}

bool MatchesOperand(const Operand &operand, const OperandMatch &pattern) {
  // Type match
  if (pattern.HasTypeMatch && operand.Type != pattern.MatchType)
    return false;
  
  // Component mode match (swizzle/mask)
  if (pattern.HasComponentMatch && operand.ComponentMode != pattern.MatchComponentMode)
    return false;
  
  // Number of components match
  if (pattern.HasNumComponentsMatch && operand.NumComponents != pattern.MatchNumComponents)
    return false;
  
  // Modifier match
  if (pattern.HasModifierMatch && operand.Modifier != pattern.MatchModifier)
    return false;
  
  // Immediate values match
  if (pattern.HasImmediateMatch) {
    if (operand.ImmediateValues.size() != pattern.MatchImmediates.size())
      return false;
    for (size_t i = 0; i < pattern.MatchImmediates.size(); ++i) {
      if (operand.ImmediateValues[i] != pattern.MatchImmediates[i])
        return false;
    }
  }
  
  // Relative addressing match
  if (pattern.HasRelativeMatch) {
    bool hasRelative = (operand.RelativeOperand != nullptr);
    if (hasRelative != pattern.MatchRelativeAddressing)
      return false;
  }
  
  // Index match - check if any immediate values match the expected indices
  if (pattern.HasIndexMatch && !pattern.MatchIndices.empty()) {
    if (operand.Indices.size() != pattern.MatchIndices.size())
      return false;
    for (size_t index = 0; index < pattern.MatchIndices.size(); ++index) {
      if (operand.Indices[index] != static_cast<uint32_t>(pattern.MatchIndices[index]))
        return false;
    }
  }
  
  return true;
}

} // namespace

bool MatchOperand(const Operand &operand, const OperandMatch &pattern) {
  return MatchesOperand(operand, pattern);
}

bool MatchInstruction(const Instruction &instruction,
                      const InstructionMatch &pattern,
                      std::unordered_map<std::string, const Operand *> &capturedOperands,
                      const std::unordered_map<std::string, const Operand *> &existingCaptures) {
  // Opcode match
  if (pattern.HasOpcode && instruction.Opcode != pattern.Opcode)
    return false;
  
  // Saturate match
  if (pattern.HasSaturateMatch && instruction.Controls.Saturate != pattern.SaturateValue)
    return false;
  
  // Test boolean match
  if (pattern.HasTestBooleanMatch && instruction.Controls.TestBoolean != pattern.MatchTestBoolean)
    return false;

  if (pattern.HasInputInterpolationModeMatch) {
    if (!instruction.Controls.HasInputInterpolationMode ||
        instruction.Controls.InputInterpolationMode != pattern.MatchInputInterpolationMode) {
      return false;
    }
  }
  
  // Operand pattern count check
  if (pattern.OperandPatterns.size() > instruction.Operands.size())
    return false;

  // Match each operand pattern against corresponding instruction operand
  for (size_t index = 0; index < pattern.OperandPatterns.size(); ++index) {
    const auto &operandPattern = pattern.OperandPatterns[index];
    const auto &operand = instruction.Operands[index];

    if (!MatchesOperand(operand, operandPattern))
      return false;

    if (!operandPattern.MatchAgainstCapture.empty()) {
      const Operand *captured = nullptr;
      const auto existingIt = capturedOperands.find(operandPattern.MatchAgainstCapture);
      if (existingIt != capturedOperands.end()) {
        captured = existingIt->second;
      } else {
        const auto priorIt = existingCaptures.find(operandPattern.MatchAgainstCapture);
        if (priorIt != existingCaptures.end()) {
          captured = priorIt->second;
        }
      }

      if (captured == nullptr || !OperandsEqual(operand, *captured)) {
        return false;
      }
    }

    // Capture operand if requested
    if (!operandPattern.CaptureName.empty())
      capturedOperands[operandPattern.CaptureName] = &operand;
  }
  
  return true;
}

std::vector<MatchResult> CollectMatches(const Program &program,
                                        const InstructionMatch &pattern) {
  std::vector<MatchResult> matches;
  for (uint32_t index = 0; index < program.Instructions.size(); ++index) {
    std::unordered_map<std::string, const Operand *> capturedOperands;
    if (!MatchInstruction(program.Instructions[index], pattern, capturedOperands, {}))
      continue;

    MatchResult result;
    result.InstructionIndex = index;
    result.Instruction = &program.Instructions[index];
    result.RangeStartIndex = index;
    result.RangeEndIndex = index;
    result.CapturedOperands = std::move(capturedOperands);
    if (!pattern.CaptureName.empty()) {
      result.CapturedInstructions[pattern.CaptureName] = &program.Instructions[index];
      result.CapturedInstructionIndices[pattern.CaptureName] = index;
    }
    matches.push_back(std::move(result));
  }
  return matches;
}

std::vector<MatchResult> CollectSequenceMatches(
    const Program &program,
    const std::vector<InstructionMatch> &patterns) {
  std::vector<MatchResult> matches;
  if (patterns.empty() || patterns.size() > program.Instructions.size()) {
    return matches;
  }

  const uint32_t limit =
      static_cast<uint32_t>(program.Instructions.size() - patterns.size() + 1);
  for (uint32_t startIndex = 0; startIndex < limit; ++startIndex) {
    std::unordered_map<std::string, const Operand *> capturedOperands;
    std::unordered_map<std::string, const Instruction *> capturedInstructions;
    std::unordered_map<std::string, uint32_t> capturedInstructionIndices;
    bool matched = true;

    for (uint32_t patternIndex = 0; patternIndex < patterns.size(); ++patternIndex) {
      const uint32_t instructionIndex = startIndex + patternIndex;
      const Instruction &instruction = program.Instructions[instructionIndex];
      const InstructionMatch &pattern = patterns[patternIndex];

      std::unordered_map<std::string, const Operand *> stepCapturedOperands;
      if (!MatchInstruction(instruction, pattern, stepCapturedOperands, capturedOperands)) {
        matched = false;
        break;
      }

      for (const auto &entry : stepCapturedOperands) {
        capturedOperands[entry.first] = entry.second;
      }

      if (!pattern.CaptureName.empty()) {
        capturedInstructions[pattern.CaptureName] = &instruction;
        capturedInstructionIndices[pattern.CaptureName] = instructionIndex;
      }
    }

    if (!matched) {
      continue;
    }

    MatchResult result;
    result.InstructionIndex = startIndex;
    result.Instruction = &program.Instructions[startIndex];
    result.RangeStartIndex = startIndex;
    result.RangeEndIndex = startIndex + static_cast<uint32_t>(patterns.size() - 1);
    result.CapturedOperands = std::move(capturedOperands);
    result.CapturedInstructions = std::move(capturedInstructions);
    result.CapturedInstructionIndices = std::move(capturedInstructionIndices);
    matches.push_back(std::move(result));
  }

  return matches;
}

} // namespace sm5
} // namespace dxp
