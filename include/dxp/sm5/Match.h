#pragma once

#include "Model.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace dxp {
namespace sm5 {

/// Operand matching criteria for declarative matching
struct OperandMatch {
  // Match by operand type (register type)
  OperandType MatchType;
  bool HasTypeMatch;

  // Match by register indices
  std::vector<int32_t> MatchIndices;
  bool HasIndexMatch;

  // Match by component selection
  uint32_t MatchComponentMode;
  bool HasComponentMatch;

  // Match by number of components
  uint32_t MatchNumComponents;
  bool HasNumComponentsMatch;

  // Match by modifier
  OperandModifier MatchModifier;
  bool HasModifierMatch;

  // Match by immediate values
  std::vector<uint32_t> MatchImmediates;
  bool HasImmediateMatch;

  // Match relative addressing shape
  bool MatchRelativeAddressing;
  bool HasRelativeMatch;

  // Capture name for this operand
  std::string CaptureName;

  // Match against a previously captured operand
  std::string MatchAgainstCapture;

  OperandMatch();
};

/// Instruction matching criteria
struct InstructionMatch {
  dxp::sm5::Opcode Opcode;
  bool HasOpcode;

  // Opcode controls to match
  bool MatchSaturate;
  bool HasSaturateMatch;
  bool SaturateValue;

  uint32_t MatchTestBoolean;
  bool HasTestBooleanMatch;

  uint32_t MatchInputInterpolationMode;
  bool HasInputInterpolationModeMatch;

  // Operand patterns
  std::vector<OperandMatch> OperandPatterns;

  // Capture name for this instruction
  std::string CaptureName;

  // Match against a previously captured instruction
  std::string MatchAgainstCapture;

  InstructionMatch();
};

/// A matched instruction with its position
struct MatchResult {
  uint32_t InstructionIndex;
  const dxp::sm5::Instruction *Instruction;
  uint32_t RangeStartIndex;
  uint32_t RangeEndIndex;
  std::unordered_map<std::string, const dxp::sm5::Operand *> CapturedOperands;
  std::unordered_map<std::string, const dxp::sm5::Instruction *> CapturedInstructions;
  std::unordered_map<std::string, uint32_t> CapturedInstructionIndices;

  /// Get a captured operand by name
  const dxp::sm5::Operand *GetCapturedOperand(const std::string &name) const;

  /// Get a captured instruction by name
  const dxp::sm5::Instruction *GetCapturedInstruction(const std::string &name) const;

  /// Get a captured instruction index by name
  const uint32_t *GetCapturedInstructionIndex(const std::string &name) const;
};

/// Collect all instructions matching the given criteria.
/// Returns a vector of match results with captured operands.
std::vector<MatchResult> CollectMatches(
  const Program &program,
  const InstructionMatch &pattern);

/// Collect all contiguous instruction ranges matching the given sequence.
std::vector<MatchResult> CollectSequenceMatches(
  const Program &program,
  const std::vector<InstructionMatch> &patterns);

/// Match a single operand against the pattern.
bool MatchOperand(const Operand &operand, const OperandMatch &pattern);

/// Match a single instruction against the pattern.
bool MatchInstruction(const Instruction &instruction,
                      const InstructionMatch &pattern,
                      std::unordered_map<std::string, const Operand *> &capturedOperands,
                      const std::unordered_map<std::string, const Operand *> &existingCaptures);

} // namespace sm5
} // namespace dxp
