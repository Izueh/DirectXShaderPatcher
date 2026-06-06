#pragma once

#include "Model.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace dxp::sm5 {

/// @brief Describes how one ordered index slot of an operand must match.
///
/// A rule operand can carry a list of these patterns, one per expected index
/// slot, checked in order. Fields that are not set are not checked.
///
/// Capture workflow:
///  - `CaptureName` — when set and the slot matches, the immediate value of
///    this slot is stored in `MatchResult::CapturedOperandIndexValues` under
///    this name for later reuse.
///  - `MatchCapture` — when set, the slot's immediate value must equal the
///    previously captured index value with this name. Used to enforce that two
///    independently-matched slots carry the same register number.
struct OperandIndexMatchPattern {
  bool Any = false;
  bool HasRepresentation = false;
  Operand::IndexRepresentation Representation =
      Operand::IndexRepresentation::Immediate32;
  bool HasImmediateLo = false;
  uint32_t ImmediateLo = 0;
  bool HasImmediateHi = false;
  uint32_t ImmediateHi = 0;
  std::string CaptureName;
  std::string MatchCapture;
};

/// @brief Describes how a declarative rule matches one operand.
struct OperandMatch {
  bool Any;

  OperandType MatchType;
  bool HasTypeMatch;

  /// Ordered set of per-slot match patterns, evaluated in order against the
  /// candidate operand's index slots.
  std::vector<OperandIndexMatchPattern> MatchIndexPatterns;

  uint32_t MatchComponentMode;
  bool HasComponentMatch;

  uint32_t MatchNumComponents;
  bool HasNumComponentsMatch;

  OperandModifier MatchModifier;
  bool HasModifierMatch;

  std::vector<uint32_t> MatchImmediates;
  bool HasImmediateMatch;

  bool MatchRelativeAddressing;
  bool HasRelativeMatch;

  std::string CaptureName;
  std::string MatchAgainstCapture;

  bool MatchCaptureType = false;
  bool MatchCaptureComponents = false;
  bool MatchCaptureModifier = false;
  bool MatchCaptureIndices = false;
  bool MatchCaptureImmediates = false;

  bool HasMatchCaptureProjection() const {
    return MatchCaptureType || MatchCaptureComponents || MatchCaptureModifier ||
           MatchCaptureIndices || MatchCaptureImmediates;
  }

  OperandMatch();
};

/// @brief Describes how a declarative rule matches one instruction.
struct InstructionMatch {
  dxp::sm5::Opcode Opcode;
  bool HasOpcode;

  bool MatchSaturate;
  bool HasSaturateMatch;
  bool SaturateValue;

  uint32_t MatchTestBoolean;
  bool HasTestBooleanMatch;

  uint32_t MatchInputInterpolationMode;
  bool HasInputInterpolationModeMatch;

  std::vector<OperandMatch> OperandPatterns;
  std::string CaptureName;
  std::string MatchAgainstCapture;

  InstructionMatch();
};

/// @brief Stores one successful pattern match and its captures.
struct MatchResult {
  uint32_t InstructionIndex;
  const dxp::sm5::Instruction *Instruction;
  uint32_t RangeStartIndex;
  uint32_t RangeEndIndex;
  std::unordered_map<std::string, const dxp::sm5::Operand *> CapturedOperands;
  std::unordered_map<std::string, const dxp::sm5::Instruction *>
      CapturedInstructions;
  std::unordered_map<std::string, uint32_t> CapturedInstructionIndices;
  std::unordered_map<std::string, uint32_t> CapturedOperandIndexValues;
  std::unordered_map<std::string, size_t> CapturedOperandPositions;

  /// @brief Looks up a captured operand by name.
  /// @param name Capture name to resolve.
  /// @return The captured operand, or `nullptr` when absent.
  const dxp::sm5::Operand *GetCapturedOperand(const std::string &name) const;

  /// @brief Looks up a captured instruction by name.
  /// @param name Capture name to resolve.
  /// @return The captured instruction, or `nullptr` when absent.
  const dxp::sm5::Instruction *
  GetCapturedInstruction(const std::string &name) const;

  /// @brief Looks up a captured instruction index by name.
  /// @param name Capture name to resolve.
  /// @return Pointer to the captured index, or `nullptr` when absent.
  const uint32_t *GetCapturedInstructionIndex(const std::string &name) const;

  /// @brief Looks up a captured operand index immediate value by name.
  /// @param name Capture name set on an `OperandIndexMatchPattern`.
  /// @return Pointer to the 32-bit immediate, or `nullptr` when absent.
  const uint32_t *
  GetCapturedOperandIndexValue(const std::string &name) const;
};

/// @brief Collects all instructions that match a pattern.
/// @param program Program to scan.
/// @param pattern Pattern to match.
/// @return All match results, including captures.
std::vector<MatchResult> CollectMatches(const Program &program,
                                        const InstructionMatch &pattern);

/// @brief Collects contiguous instruction ranges matching a sequence.
/// @param program Program to scan.
/// @param patterns Sequence of patterns to match.
/// @return All sequence matches, including captures.
std::vector<MatchResult>
CollectSequenceMatches(const Program &program,
                       const std::vector<InstructionMatch> &patterns);

/// @brief Enumerates rewrite operations for instruction stream mutation.
enum class RewriteActionType {
  ReplaceOne,
  ReplaceRange,
  InsertBefore,
  InsertAfter,
  RemoveRange,
};

/// @brief Describes one rewrite operation to apply to a program.
struct RewriteAction {
  RewriteActionType Type;

  uint32_t ReplaceIndex;

  uint32_t RangeStart;
  uint32_t RangeEnd;

  uint32_t InsertPosition;

  uint32_t RemoveStart;
  uint32_t RemoveEnd;

  uint32_t RequiredTempCount;

  std::vector<Instruction> NewInstructions;

  RewriteAction();
};

/// @brief Applies rewrite actions to a decoded program.
/// @param program Program to mutate.
/// @param actions Actions to apply in order.
/// @return `true` on success, or `false` when the action set is invalid.
bool ApplyRewriteActions(Program &program,
                         const std::vector<RewriteAction> &actions);

/// @brief Rebuilds derived declaration metadata from the instruction stream.
/// @param program Program whose metadata should be refreshed.
void RebuildProgramMetadata(Program &program);

} // namespace dxp::sm5
