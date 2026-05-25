#pragma once

#include "Model.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace dxp::sm5 {

struct OperandMatch {

  OperandType MatchType;
  bool HasTypeMatch;

  std::vector<int32_t> MatchIndices;
  bool HasIndexMatch;

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
  std::unordered_map<std::string, const dxp::sm5::Operand *> CapturedOperands;
  std::unordered_map<std::string, const dxp::sm5::Instruction *>
      CapturedInstructions;
  std::unordered_map<std::string, uint32_t> CapturedInstructionIndices;

  const dxp::sm5::Operand *GetCapturedOperand(const std::string &name) const;

  const dxp::sm5::Instruction *
  GetCapturedInstruction(const std::string &name) const;

  const uint32_t *GetCapturedInstructionIndex(const std::string &name) const;
};

std::vector<MatchResult> CollectMatches(const Program &program,
                                        const InstructionMatch &pattern);

std::vector<MatchResult>
CollectSequenceMatches(const Program &program,
                       const std::vector<InstructionMatch> &patterns);

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

  std::vector<Instruction> NewInstructions;

  RewriteAction();
};

bool ApplyRewriteActions(Program &program,
                         const std::vector<RewriteAction> &actions);

void RebuildProgramMetadata(Program &program);

} // namespace dxp::sm5
