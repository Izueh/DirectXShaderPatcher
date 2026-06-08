#pragma once

#include "dxp/sm5/Model.h"  // Operand, Instruction
#include "dxp/sm5/Recipe.h"  // CaptureStore

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace dxp::sm5 {

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

struct OperandMatch {
  bool Any;

  OperandType MatchType;
  bool HasTypeMatch;

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

struct MatchResult {
  uint32_t InstructionIndex;
  const dxp::sm5::Instruction *Instruction;
  uint32_t RangeStartIndex;
  uint32_t RangeEndIndex;
  std::unordered_map<std::string, Operand> operands;
  std::unordered_map<std::string, dxp::sm5::Instruction> instructions;
  std::unordered_map<std::string, uint32_t> indexValues;
};

/// @brief Collects all instructions that match a pattern.
/// @param program Program to scan.
/// @param pattern Pattern to match.
/// @param captures Global capture store for storing matched operand data.
/// @return All match results.
std::vector<MatchResult> CollectMatches(const Program &program,
                                        const InstructionMatch &pattern,
                                        CaptureStore &captures);

/// @brief Collects contiguous instruction ranges matching a sequence.
/// @param program Program to scan.
/// @param patterns Sequence of patterns to match.
/// @param captures Global capture store for storing matched operand data.
/// @return All sequence matches.
std::vector<MatchResult>
CollectSequenceMatches(const Program &program,
                       const std::vector<InstructionMatch> &patterns,
                       CaptureStore &captures);

enum class RewriteActionType {
  ReplaceOne,
  ReplaceRange,
  InsertBefore,
  InsertAfter,
  RemoveRange,
};

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

/// @brief Refreshes derived declaration metadata from the instruction stream.
/// @param program Program whose declarations should be refreshed.
void RefreshDeclarations(Program &program);

} // namespace dxp::sm5
